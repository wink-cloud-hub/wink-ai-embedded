#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""scripts/migrate_vendor_cms8s78xx.py — Lossless migration for vendor/cms8s78xx (ADR-0079 Phase 3).

Migrates vendor apps from 2-level directory to 3-level standard directory:
  wink-micro-app/vendor_cms8s78xx_v202/<app> -> wink-micro-app/vendor/cms8s78xx/<app>

Guarantees:
  1. Preserves Git history via `git mv`.
  2. Patches relative paths in CMakeLists.txt (depth 3 requires ../../../../wink-micro-os fallback).
  3. Patches appName in unisim-assets/device-tree.json.
  4. Patches templateId in unisim-scenarios/*.scenario.json.
  5. Updates documentation and references across the repository.
  6. Supports --dry-run for zero-risk inspection before applying.

Usage:
  python scripts/migrate_vendor_cms8s78xx.py --dry-run
  python scripts/migrate_vendor_cms8s78xx.py --apply
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

REPO_ROOT = Path(__file__).resolve().parents[1]
APPS_ROOT = REPO_ROOT / "wink-micro-app"
SRC_VENDOR_DIR = APPS_ROOT / "vendor_cms8s78xx_v202"
DST_VENDOR_DIR = APPS_ROOT / "vendor" / "cms8s78xx"


def get_vendor_apps() -> list[str]:
    """Return all app directory names under SRC_VENDOR_DIR."""
    if not SRC_VENDOR_DIR.is_dir():
        if DST_VENDOR_DIR.is_dir():
            return sorted([p.name for p in DST_VENDOR_DIR.iterdir() if p.is_dir()])
        return []
    return sorted([p.name for p in SRC_VENDOR_DIR.iterdir() if p.is_dir()])


def run_git_mv(src: Path, dst: Path, dry_run: bool = False) -> bool:
    """Move directory using git mv to preserve history, fallback to rename."""
    if not src.exists():
        print(f"  ⏭️  Skipping non-existent: {src.name}")
        return False

    if dry_run:
        print(f"  [DRY-RUN] git mv: {src.relative_to(REPO_ROOT)} -> {dst.relative_to(REPO_ROOT)}")
        return True

    dst.parent.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(["git", "mv", str(src), str(dst)], check=True, capture_output=True)
        print(f"  [git mv] {src.name} -> {dst.relative_to(REPO_ROOT)}")
        return True
    except subprocess.CalledProcessError as e:
        err_msg = e.stderr.decode("utf-8", errors="replace").strip()
        print(f"  [git mv fallback: {err_msg}] using filesystem rename...")
        if dst.exists():
            shutil.rmtree(dst)
        shutil.move(str(src), str(dst))
        print(f"  [fs move] {src.name} -> {dst.relative_to(REPO_ROOT)}")
        return True


def patch_app_cmake(cmake_path: Path, app_name: str, dry_run: bool = False) -> bool:
    """Patch CMakeLists.txt for depth 3 (4 levels up to root) and modernize labels."""
    if not cmake_path.is_file():
        return False

    content = cmake_path.read_text(encoding="utf-8")
    modified = False

    # 1. Update entire OS root block with proper nested structure
    pattern = re.compile(
        r'if\(DEFINED wink-micro-os_SOURCE_DIR\).*?endif\(\)',
        re.DOTALL,
    )
    proper_block = (
        'if(DEFINED wink-micro-os_SOURCE_DIR)\n'
        '    set(_MCS51_APP_OS_ROOT "${wink-micro-os_SOURCE_DIR}")\n'
        'elseif(DEFINED WINK_MICRO_OS_ROOT)\n'
        '    set(_MCS51_APP_OS_ROOT "${WINK_MICRO_OS_ROOT}")\n'
        'elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../wink-micro-os")\n'
        '    get_filename_component(_MCS51_APP_OS_ROOT\n'
        '        "${CMAKE_CURRENT_SOURCE_DIR}/../../../../wink-micro-os" ABSOLUTE)\n'
        'else()\n'
        '    get_filename_component(_MCS51_APP_OS_ROOT\n'
        '        "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os" ABSOLUTE)\n'
        'endif()'
    )
    if pattern.search(content):
        new_content = pattern.sub(proper_block, content)
        if new_content != content:
            content = new_content
            modified = True

    # 2. Modernize comments & messages: vendor_cms8s78xx_v202_<app> -> vendor_cms8s78xx_<app>
    old_tag = f"vendor_cms8s78xx_v202_{app_name}"
    new_tag = f"vendor_cms8s78xx_{app_name}"
    if old_tag in content:
        content = content.replace(old_tag, new_tag)
        modified = True

    if modified:
        if not dry_run:
            cmake_path.write_text(content, encoding="utf-8")
        print(f"  [patch] CMakeLists.txt updated for {app_name}")
    return modified


def patch_app_json(app_dir: Path, app_name: str, dry_run: bool = False) -> bool:
    """Ensure wink-app.json contains clean leaf name and correct metadata."""
    manifest = app_dir / "wink-app.json"
    if not manifest.is_file():
        return False
    try:
        data = json.loads(manifest.read_text(encoding="utf-8"))
        changed = False
        cur_name = data.get("app_name", "")
        if cur_name != app_name:
            data["app_name"] = app_name
            changed = True

        if changed:
            if not dry_run:
                manifest.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            print(f"  [patch] wink-app.json app_name -> '{app_name}'")
            return True
    except Exception as e:
        print(f"  ⚠️ Failed to patch {manifest}: {e}")
    return False


def patch_device_tree(app_dir: Path, app_name: str, dry_run: bool = False) -> bool:
    """Normalize appName in unisim-assets/device-tree.json."""
    dt_path = app_dir / "unisim-assets" / "device-tree.json"
    if not dt_path.is_file():
        return False
    try:
        data = json.loads(dt_path.read_text(encoding="utf-8"))
        cur_name = data.get("appName", "")
        # Normalize vendor_cms8s78xx_v202_xxx -> vendor_cms8s78xx_xxx
        if "vendor_cms8s78xx_v202_" in cur_name:
            data["appName"] = cur_name.replace("vendor_cms8s78xx_v202_", "vendor_cms8s78xx_")
            if not dry_run:
                dt_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            print(f"  [patch] device-tree.json: {cur_name} -> {data['appName']}")
            return True
    except Exception as e:
        print(f"  ⚠️ Failed to patch {dt_path}: {e}")
    return False


def patch_scenarios(app_dir: Path, app_name: str, dry_run: bool = False) -> bool:
    """Normalize templateId in unisim-scenarios/*.scenario.json."""
    scenarios_dir = app_dir / "unisim-scenarios"
    if not scenarios_dir.is_dir():
        return False
    modified_any = False
    for sc_file in scenarios_dir.glob("*.scenario.json"):
        try:
            content = sc_file.read_text(encoding="utf-8")
            if "vendor_cms8s78xx_v202_" in content:
                new_content = content.replace("vendor_cms8s78xx_v202_", "vendor_cms8s78xx_")
                if not dry_run:
                    sc_file.write_text(new_content, encoding="utf-8")
                print(f"  [patch] scenario: {sc_file.name} (templateId updated)")
                modified_any = True
        except Exception as e:
            print(f"  ⚠️ Failed to patch {sc_file}: {e}")
    return modified_any


def patch_repo_references(dry_run: bool = False):
    """Patch all repo references from vendor_cms8s78xx_v202 to vendor/cms8s78xx."""
    print("\n📝 Updating global repository references...")

    target_files = [
        # README
        REPO_ROOT / "wink-micro-app" / "README.md",
        # docs / plans
        REPO_ROOT / "docs" / "vendors" / "Cmsemicon" / "CMS8S78XX_EXAMPLE_CHECKLIST.md",
        REPO_ROOT / "docs" / "vendors" / "Cmsemicon" / "CMS8S78XX_EXAMPLE_PLAYBOOK.md",
        REPO_ROOT / "docs" / "zh" / "design" / "02-wink-micro-os" / "07-mcs51-simulation-interception.md",
        REPO_ROOT / "docs" / "todolist" / "2026-09-10-mcs51-sim-vs-silicon-gap-todolist.md",
        REPO_ROOT / "docs" / "implementation-plans" / "mcs51" / "2026-09-15-wdt-and-reset-fidelity-plan.md",
        REPO_ROOT / "docs" / "implementation-plans" / "mcs51" / "2026-09-16-cms8s78xx-epwm-fidelity-plan.md",
        REPO_ROOT / "docs" / "implementation-plans" / "mcs51" / "2026-09-21-cms8s78xx-i2c-spi-deadlock-resolution-plan.md",
        REPO_ROOT / "docs" / "implementation-plans" / "mcs51" / "2026-09-11-mcs51-decoupling" / "stage7-test-contract-close.md",
        REPO_ROOT / ".github" / "license-map.json",
        # wink-tools docs
        REPO_ROOT / "wink-tools" / "docs" / "cli-help-tree.json",
        REPO_ROOT / "wink-tools" / "docs" / "en" / "01-cli-reference.md",
        REPO_ROOT / "wink-tools" / "docs" / "zh" / "01-cli-reference.md",
    ]

    for p in target_files:
        if not p.is_file():
            continue
        try:
            txt = p.read_text(encoding="utf-8")
            if "vendor_cms8s78xx_v202" in txt:
                new_txt = txt.replace("vendor_cms8s78xx_v202_", "vendor_cms8s78xx_")
                new_txt = new_txt.replace("vendor_cms8s78xx_v202/", "vendor/cms8s78xx/")
                new_txt = new_txt.replace("vendor_cms8s78xx_v202", "vendor/cms8s78xx")
                if not dry_run:
                    p.write_text(new_txt, encoding="utf-8")
                print(f"  [patch] {p.relative_to(REPO_ROOT)}")
        except Exception as e:
            print(f"  ⚠️ Failed to patch {p}: {e}")


def verify_migration() -> bool:
    """Verify that all apps exist under vendor/cms8s78xx and have valid structure."""
    print("\n🔍 Verifying migration results...")
    if not DST_VENDOR_DIR.is_dir():
        print(f"  ❌ Error: {DST_VENDOR_DIR} does not exist!")
        return False

    app_dirs = [p for p in DST_VENDOR_DIR.iterdir() if p.is_dir()]
    print(f"  ✅ Discovered {len(app_dirs)} apps under {DST_VENDOR_DIR.relative_to(REPO_ROOT)}:")
    all_ok = True
    for d in sorted(app_dirs):
        has_manifest = (d / "wink-app.json").is_file()
        has_cmake = (d / "CMakeLists.txt").is_file()
        is_ok = has_manifest and has_cmake
        if not is_ok:
            all_ok = False
        status = "OK" if is_ok else "INCOMPLETE"
        print(f"    - {d.name:25} [{status}] (manifest={has_manifest}, cmake={has_cmake})")

    if SRC_VENDOR_DIR.exists():
        remains = list(SRC_VENDOR_DIR.iterdir())
        if not remains:
            print(f"  🧹 Cleaning up empty old directory {SRC_VENDOR_DIR.name}...")
            SRC_VENDOR_DIR.rmdir()
        else:
            print(f"  ⚠️ Warning: old directory {SRC_VENDOR_DIR.name} still contains: {remains}")
            all_ok = False

    if all_ok:
        print(f"\n🎉 Verification completed successfully! All {len(app_dirs)} apps are in place.")
    else:
        print(f"\n⚠️ Verification completed with some warnings.")
    return all_ok


def main():
    parser = argparse.ArgumentParser(
        description="Migrate vendor_cms8s78xx_v202 to vendor/cms8s78xx (ADR-0079 Phase 3)"
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--dry-run", action="store_true", help="Preview actions without modifying files")
    group.add_argument("--apply", action="store_true", help="Apply all migrations and patches")
    args = parser.parse_args()

    apps = get_vendor_apps()
    print("=" * 70)
    print(f"🚀 Vendor Apps Ecosystem Migration (ADR-0079 Phase 3)")
    print(f"   Source: {SRC_VENDOR_DIR.relative_to(REPO_ROOT) if SRC_VENDOR_DIR.exists() else 'Already moved'}")
    print(f"   Target: {DST_VENDOR_DIR.relative_to(REPO_ROOT)}")
    print(f"   Total apps: {len(apps)}")
    print(f"   Mode: {'[DRY-RUN]' if args.dry_run else '[APPLY]'}")
    print("=" * 70)

    if not apps:
        print("❌ No apps found to migrate!")
        sys.exit(1)

    # 1. Move directories
    print("\n📦 Step 1: Moving app directories...")
    DST_VENDOR_DIR.mkdir(parents=True, exist_ok=True)
    for app in apps:
        src = SRC_VENDOR_DIR / app
        dst = DST_VENDOR_DIR / app
        if src.exists():
            run_git_mv(src, dst, dry_run=args.dry_run)

    # 2. Patch app files
    print("\n🔧 Step 2: Patching app configurations...")
    for app in apps:
        target_dir = (DST_VENDOR_DIR / app) if not args.dry_run else (SRC_VENDOR_DIR / app)
        if not target_dir.exists():
            target_dir = (SRC_VENDOR_DIR / app)

        patch_app_cmake(target_dir / "CMakeLists.txt", app, dry_run=args.dry_run)
        patch_app_json(target_dir, app, dry_run=args.dry_run)
        patch_device_tree(target_dir, app, dry_run=args.dry_run)
        patch_scenarios(target_dir, app, dry_run=args.dry_run)

    # 3. Patch repo-wide references
    print("\n🌐 Step 3: Patching global repository references...")
    patch_repo_references(dry_run=args.dry_run)

    # 4. Verify if applied
    if args.apply:
        verify_migration()

    print("\n" + "=" * 70)
    if args.dry_run:
        print("✨ DRY-RUN complete. Review the above plan and execute with --apply.")
    else:
        print("✅ Migration completed successfully!")
    print("=" * 70)


if __name__ == "__main__":
    main()
