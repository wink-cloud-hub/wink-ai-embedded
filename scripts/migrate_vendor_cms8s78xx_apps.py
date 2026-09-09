#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""scripts/migrate_vendor_cms8s78xx_apps.py — One-click migration for vendor_cms8s78xx_v202 apps (ADR-0079)."""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

PREFIX = "vendor_cms8s78xx_v202_"
TARGET_GROUP = "vendor_cms8s78xx_v202"

ROBUST_CMAKE_BLOCK = """\
if(DEFINED wink-micro-os_SOURCE_DIR)
    set(_MCS51_APP_OS_ROOT "${wink-micro-os_SOURCE_DIR}")
elseif(DEFINED WINK_MICRO_OS_ROOT)
    set(_MCS51_APP_OS_ROOT "${WINK_MICRO_OS_ROOT}")
else()
    get_filename_component(_MCS51_APP_OS_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os" ABSOLUTE)
endif()
set(_MCS51_CLEANUP
    "${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py")"""


def run_git_mv(src: Path, dst: Path, dry_run: bool = False) -> bool:
    """Move directory using git mv to preserve file history, fallback to rename."""
    if dry_run:
        print(f"  [DRY-RUN] Move: {src.name} -> {TARGET_GROUP}/{dst.name}")
        return True

    dst.parent.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(["git", "mv", str(src), str(dst)], check=True, capture_output=True)
        print(f"  [git mv] {src.name} -> {TARGET_GROUP}/{dst.name}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"  [git mv failed: {e.stderr.decode('utf-8', errors='replace').strip()}] fallback to fs rename...")
        src.rename(dst)
        print(f"  [fs mv]  {src.name} -> {TARGET_GROUP}/{dst.name}")
        return True


def patch_cmakelists(cmake_path: Path, dry_run: bool = False) -> bool:
    """Patch CMakeLists.txt to use robust depth-agnostic OS root resolution."""
    content = cmake_path.read_text(encoding="utf-8")

    # Match get_filename_component + set(_MCS51_CLEANUP ... ) block
    pattern = re.compile(
        r'get_filename_component\(_MCS51_APP_OS_ROOT\s*\r?\n\s*'
        r'"\${CMAKE_CURRENT_SOURCE_DIR}/\.\./\.\./wink-micro-os"\s+ABSOLUTE\)\s*\r?\n'
        r'set\(_MCS51_CLEANUP\s*\r?\n\s*'
        r'"\${_MCS51_APP_OS_ROOT}/frameworks/mcs51/tools/mcs51_cleanup\.py"\)',
        re.MULTILINE
    )

    if not pattern.search(content):
        # Fallback more permissive pattern
        alt_pattern = re.compile(
            r'get_filename_component\(_MCS51_APP_OS_ROOT\s+"[^"]+"\s+ABSOLUTE\)\s*\r?\n'
            r'set\(_MCS51_CLEANUP\s+"[^"]+"\)',
            re.MULTILINE
        )
        if not alt_pattern.search(content):
            print(f"  ⚠️ Warning: Could not match _MCS51_APP_OS_ROOT block in {cmake_path}")
            return False
        pattern = alt_pattern

    new_content = pattern.sub(ROBUST_CMAKE_BLOCK, content)
    if not dry_run:
        cmake_path.write_text(new_content, encoding="utf-8")
    print(f"  [patch] CMakeLists.txt updated.")
    return True


def patch_wink_app_json(json_path: Path, leaf_name: str, dry_run: bool = False) -> bool:
    """Update app_name to short leaf name in wink-app.json."""
    try:
        data = json.loads(json_path.read_text(encoding="utf-8"))
        old_name = data.get("app_name", "")
        data["app_name"] = leaf_name
        if not dry_run:
            json_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"  [patch] wink-app.json: app_name '{old_name}' -> '{leaf_name}'")
        return True
    except Exception as e:
        print(f"  ⚠️ Failed to patch {json_path}: {e}")
        return False


def patch_external_references(workspace_root: Path, dry_run: bool = False):
    """Patch unisim test harness and README references."""
    # 1. Unisim test harness
    test_file = workspace_root / "docs/.internals/unisim/simulation-runner/consistency/__tests__/app-consistency-runner.test.ts"
    if test_file.is_file():
        content = test_file.read_text(encoding="utf-8")
        old_target = "wink-micro-app/vendor_cms8s78xx_v202_led_4com_8seg"
        new_target = "wink-micro-app/vendor_cms8s78xx_v202/led_4com_8seg"
        if old_target in content:
            if not dry_run:
                test_file.write_text(content.replace(old_target, new_target), encoding="utf-8")
            print(f"  [patch] Unisim test updated: {test_file.name}")

    # 2. wink-micro-app/README.md
    readme_file = workspace_root / "wink-micro-app/README.md"
    if readme_file.is_file():
        content = readme_file.read_text(encoding="utf-8")
        old_ref = "vendor_cms8s78xx_v202_led_4com_8seg"
        new_ref = "vendor_cms8s78xx_v202/led_4com_8seg"
        if old_ref in content:
            if not dry_run:
                readme_file.write_text(content.replace(old_ref, new_ref), encoding="utf-8")
            print(f"  [patch] README.md updated: {readme_file.name}")


def main():
    parser = argparse.ArgumentParser(description="Migrate vendor_cms8s78xx_v202 apps into group folder.")
    parser.add_argument("--dry-run", action="store_true", help="Preview changes without modifying disk")
    parser.add_argument("--apply", action="store_true", help="Execute the migration")
    args = parser.parse_args()

    if not args.apply and not args.dry_run:
        print("Please specify --dry-run to preview or --apply to execute.")
        sys.exit(1)

    workspace_root = Path(__file__).resolve().parents[1]
    apps_root = workspace_root / "wink-micro-app"
    group_dir = apps_root / TARGET_GROUP

    # 1. Discover all matching app directories
    candidates = [
        d for d in apps_root.iterdir()
        if d.is_dir() and d.name.startswith(PREFIX) and d != group_dir
    ]
    candidates.sort(key=lambda p: p.name)

    print(f"Found {len(candidates)} vendor apps to migrate into '{TARGET_GROUP}/'...")

    # 2. Process each app
    for app_dir in candidates:
        leaf_name = app_dir.name[len(PREFIX):]
        target_dir = group_dir / leaf_name

        print(f"\nProcessing: {app_dir.name} -> {TARGET_GROUP}/{leaf_name}")
        # A. Move directory
        run_git_mv(app_dir, target_dir, dry_run=args.dry_run)

        # B. Patch CMakeLists.txt
        cmake_file = (target_dir if not args.dry_run else app_dir) / "CMakeLists.txt"
        if cmake_file.is_file():
            patch_cmakelists(cmake_file, dry_run=args.dry_run)

        # C. Patch wink-app.json
        manifest_file = (target_dir if not args.dry_run else app_dir) / "wink-app.json"
        if manifest_file.is_file():
            patch_wink_app_json(manifest_file, leaf_name, dry_run=args.dry_run)

    # 3. Patch external references
    print("\nPatching test harness and documentation references...")
    patch_external_references(workspace_root, dry_run=args.dry_run)

    mode_label = "DRY-RUN PREVIEW" if args.dry_run else "MIGRATION COMPLETE"
    print(f"\n✨ {mode_label}: {len(candidates)} apps processed.")


if __name__ == "__main__":
    main()
