#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""scripts/migrate_micro_app_ecosystem.py — One-click ecosystem reorganization for wink-micro-app (ADR-0079 Phase 2).

Reorganizes flat apps into structured ecosystem groups:
  - native/   (AI-Native Role-Action & OS runtime)
  - mcs51/    (MCS-51 classic compatible apps)
  - arduino/  (Arduino sketch compatibility)
  - pdk/      (PDK OTP 8-bit ISA simulation)
  - fixtures/ (Platform Bring-up & deterministic test fixtures)

Usage:
  python scripts/migrate_micro_app_ecosystem.py --dry-run
  python scripts/migrate_micro_app_ecosystem.py --apply
"""

from __future__ import annotations

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

APPS_MIGRATION = [
    # ── native/ (AI-Native Role-Action & OS runtime) ──
    {"src": "avoidance_car", "dst": "native/avoidance_car", "leaf": "avoidance_car"},
    {"src": "oled_dashboard", "dst": "native/oled_dashboard", "leaf": "oled_dashboard"},
    {"src": "dual_task_demo", "dst": "native/dual_task_demo", "leaf": "dual_task_demo"},
    # ── appliances/ (Enterprise Commercial Smart Appliances: GB 4706 / Plant Profile) ──
    {"src": "mcs51_health_pot", "dst": "appliances/health_pot", "leaf": "health_pot"},
    # ── mcs51/ (MCS-51 classic compatible apps) ──
    {"src": "mcs51_analog_threshold", "dst": "mcs51/analog_threshold", "leaf": "analog_threshold"},
    {"src": "mcs51_button_led", "dst": "mcs51/button_led", "leaf": "button_led"},
    {"src": "mcs51_button_led_int", "dst": "mcs51/button_led_int", "leaf": "button_led_int"},
    {"src": "mcs51_uart_echo", "dst": "mcs51/uart_echo", "leaf": "uart_echo"},
    {"src": "mcs51_uart_hello", "dst": "mcs51/uart_hello", "leaf": "uart_hello"},
    # ── arduino/ (Arduino sketch compatibility) ──
    {"src": "arduino_blink_demo", "dst": "arduino/blink", "leaf": "blink"},
    # ── pdk/ (PDK OTP 8-bit ISA simulation) ──
    {"src": "pdk_button_led", "dst": "pdk/button_led", "leaf": "button_led"},
    # ── fixtures/ (Platform Bring-up & deterministic test fixtures) ──
    {"src": "devkitc_smoke", "dst": "fixtures/devkitc_smoke", "leaf": "devkitc_smoke"},
    {"src": "unisim_smoke", "dst": "fixtures/unisim_smoke", "leaf": "unisim_smoke"},
    {"src": "determinism_fixture", "dst": "fixtures/determinism_fixture", "leaf": "determinism_fixture"},
    {"src": "resource_conflict", "dst": "fixtures/resource_conflict", "leaf": "resource_conflict"},
]


def run_git_mv(src: Path, dst: Path, dry_run: bool = False) -> bool:
    """Move directory using git mv to preserve file history, fallback to rename."""
    if not src.exists():
        print(f"  ⏭️  Skipping non-existent source: {src.name}")
        return False

    if dry_run:
        print(f"  [DRY-RUN] git mv: {src.name} -> {dst.as_posix()}")
        return True

    dst.parent.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(["git", "mv", str(src), str(dst)], check=True, capture_output=True)
        print(f"  [git mv] {src.name} -> {dst.as_posix()}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"  [git mv fallback: {e.stderr.decode('utf-8', errors='replace').strip()}] renaming on filesystem...")
        src.rename(dst)
        print(f"  [fs rename] {src.name} -> {dst.as_posix()}")
        return True


def patch_app_cmake(cmake_path: Path, dst_rel: str, dry_run: bool = False) -> bool:
    """Patch relative paths inside an app's CMakeLists.txt to accommodate depth 2."""
    if not cmake_path.is_file():
        return False

    content = cmake_path.read_text(encoding="utf-8")
    modified = False

    # 1. sample_common.cmake inclusion
    old_sample_common = "include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)"
    robust_sample_common = (
        "if(EXISTS \"${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake\")\n"
        "    include(\"${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake\")\n"
        "elseif(EXISTS \"${CMAKE_CURRENT_LIST_DIR}/../../sample_common.cmake\")\n"
        "    include(\"${CMAKE_CURRENT_LIST_DIR}/../../sample_common.cmake\")\n"
        "endif()"
    )
    if old_sample_common in content:
        content = content.replace(old_sample_common, robust_sample_common)
        modified = True

    # 2. WINK_CODEGEN_ROOT path
    old_codegen = "set(WINK_CODEGEN_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../wink-tools/tools/codegen)"
    robust_codegen = (
        "if(DEFINED WINK_TOOLS_ROOT)\n"
        "        set(WINK_CODEGEN_ROOT \"${WINK_TOOLS_ROOT}/tools/codegen\")\n"
        "    else()\n"
        "        get_filename_component(WINK_CODEGEN_ROOT \"${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-tools/tools/codegen\" ABSOLUTE)\n"
        "    endif()"
    )
    if old_codegen in content:
        content = content.replace(old_codegen, robust_codegen)
        modified = True

    # 3. MCS-51 app OS root block
    mcs51_pattern = re.compile(
        r'get_filename_component\(_MCS51_APP_OS_ROOT\s*\r?\n\s*'
        r'"\${CMAKE_CURRENT_SOURCE_DIR}/\.\./\.\./wink-micro-os"\s+ABSOLUTE\)',
        re.MULTILINE,
    )
    robust_mcs51_block = (
        "if(DEFINED wink-micro-os_SOURCE_DIR)\n"
        "    set(_MCS51_APP_OS_ROOT \"${wink-micro-os_SOURCE_DIR}\")\n"
        "elseif(DEFINED WINK_MICRO_OS_ROOT)\n"
        "    set(_MCS51_APP_OS_ROOT \"${WINK_MICRO_OS_ROOT}\")\n"
        "else()\n"
        "    get_filename_component(_MCS51_APP_OS_ROOT\n"
        "        \"${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os\" ABSOLUTE)\n"
        "endif()"
    )
    if mcs51_pattern.search(content):
        content = mcs51_pattern.sub(robust_mcs51_block, content)
        modified = True

    # 4. PDK includes path
    old_pdk_inc = 'set(PDK_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/vendors/PDK/pdk-includes")'
    new_pdk_inc = 'get_filename_component(PDK_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/vendors/PDK/pdk-includes" ABSOLUTE)'
    if old_pdk_inc in content:
        content = content.replace(old_pdk_inc, new_pdk_inc)
        modified = True

    # 5. Dual task demo stub / wasm path
    if "dual_task_demo" in dst_rel:
        content = content.replace(
            "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os/test/stubs/js_sim_host_stub.c",
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os/test/stubs/js_sim_host_stub.c",
        )
        content = content.replace(
            "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os/targets/wasm",
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os/targets/wasm",
        )
        modified = True

    # 6. devkitc_smoke selftest sources
    if "devkitc_smoke" in dst_rel:
        content = content.replace(
            "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os/runtime/selftest/src",
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../wink-micro-os/runtime/selftest/src",
        )
        modified = True

    if modified:
        if not dry_run:
            cmake_path.write_text(content, encoding="utf-8")
        print(f"  [patch] CMakeLists.txt updated for {dst_rel}")
    return modified


def patch_wink_app_json(json_path: Path, leaf_name: str, dry_run: bool = False) -> bool:
    """Update app_name to leaf name in wink-app.json."""
    if not json_path.is_file():
        return False
    try:
        data = json.loads(json_path.read_text(encoding="utf-8"))
        old_name = data.get("app_name", "")
        if old_name != leaf_name:
            data["app_name"] = leaf_name
            if not dry_run:
                json_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            print(f"  [patch] wink-app.json: app_name '{old_name}' -> '{leaf_name}'")
            return True
    except Exception as e:
        print(f"  ⚠️ Failed to patch {json_path}: {e}")
    return False


def patch_repo_configs(workspace_root: Path, dry_run: bool = False):
    """Patch root CMake, test CMake, user_surface lint rules, and CI workflows."""
    # 1. wink-micro-os/CMakeLists.txt
    root_cmake = workspace_root / "wink-micro-os/CMakeLists.txt"
    if root_cmake.is_file():
        txt = root_cmake.read_text(encoding="utf-8")
        # default app
        txt = txt.replace(
            'set(WINK_APP_DIR "wink-micro-app/avoidance_car"',
            'set(WINK_APP_DIR "wink-micro-app/native/avoidance_car"',
        )
        # sample subdirectories
        old_macro = (
            "        macro(wink_add_sample_subdirectory name)\n"
            '            get_filename_component(_abs_sample_dir "${CMAKE_CURRENT_SOURCE_DIR}/../wink-micro-app/${name}" ABSOLUTE)\n'
            '            if(EXISTS "${_abs_sample_dir}")\n'
            '                if(NOT "${WINK_APP_DIR}" STREQUAL "${_abs_sample_dir}")\n'
            '                    add_subdirectory("${_abs_sample_dir}" "${CMAKE_BINARY_DIR}/${name}_build")\n'
            "                endif()\n"
            "            endif()\n"
            "        endmacro()\n\n"
            "        wink_add_sample_subdirectory(oled_dashboard)\n"
            "        wink_add_sample_subdirectory(devkitc_smoke)\n"
            "        wink_add_sample_subdirectory(resource_conflict)\n"
            "        wink_add_sample_subdirectory(dual_task_demo)\n"
            "        wink_add_sample_subdirectory(arduino_blink_demo)"
        )
        new_macro = (
            "        macro(wink_add_sample_subdirectory name)\n"
            '            get_filename_component(_abs_sample_dir "${CMAKE_CURRENT_SOURCE_DIR}/../wink-micro-app/${name}" ABSOLUTE)\n'
            '            if(EXISTS "${_abs_sample_dir}")\n'
            '                if(NOT "${WINK_APP_DIR}" STREQUAL "${_abs_sample_dir}")\n'
            '                    string(REPLACE "/" "_" _build_tag "${name}")\n'
            '                    add_subdirectory("${_abs_sample_dir}" "${CMAKE_BINARY_DIR}/${_build_tag}_build")\n'
            "                endif()\n"
            "            endif()\n"
            "        endmacro()\n\n"
            "        wink_add_sample_subdirectory(native/oled_dashboard)\n"
            "        wink_add_sample_subdirectory(fixtures/devkitc_smoke)\n"
            "        wink_add_sample_subdirectory(fixtures/resource_conflict)\n"
            "        wink_add_sample_subdirectory(native/dual_task_demo)\n"
            "        wink_add_sample_subdirectory(arduino/blink)"
        )
        if old_macro in txt:
            txt = txt.replace(old_macro, new_macro)
            if not dry_run:
                root_cmake.write_text(txt, encoding="utf-8")
            print("  [patch] wink-micro-os/CMakeLists.txt updated.")

    # 2. wink-micro-os/test/CMakeLists.txt
    test_cmake = workspace_root / "wink-micro-os/test/CMakeLists.txt"
    if test_cmake.is_file():
        txt = test_cmake.read_text(encoding="utf-8")
        old_fn = (
            "    function(mcs51_transpile_app app_dir app_src)\n"
            "        set(_app_c\n"
            "            ${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-app/${app_dir}/${app_src}.c)\n"
            "        set(_cpp ${_MCS51_GEN_DIR}/${app_dir}_${app_src}.cpp)\n"
            "        add_custom_command(\n"
            "            OUTPUT ${_cpp}\n"
            "            COMMAND ${CMAKE_COMMAND} -E make_directory ${_MCS51_GEN_DIR}\n"
            "            COMMAND ${Python3_EXECUTABLE}\n"
            "                ${_MCS51_FW_DIR}/tools/transpile_app_keil_c51.py\n"
            "                ${_app_c} ${_cpp}\n"
            "            DEPENDS ${_app_c}\n"
            "                    ${_MCS51_FW_DIR}/tools/transpile_app_keil_c51.py\n"
            '            COMMENT "mcs51 app transpile: ${app_dir}/${app_src}.c -> ${app_dir}_${app_src}.cpp"\n'
            "            VERBATIM)\n"
            "        set(_MCS51_${app_dir}_${app_src}_CPP ${_cpp} PARENT_SCOPE)\n"
            "    endfunction()\n\n"
            "    mcs51_transpile_app(mcs51_health_pot health_pot)"
        )
        new_fn = (
            "    function(mcs51_transpile_app app_dir app_src)\n"
            '        string(REPLACE "/" "_" _app_tag "${app_dir}")\n'
            "        set(_app_c\n"
            "            ${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-app/${app_dir}/${app_src}.c)\n"
            "        set(_cpp ${_MCS51_GEN_DIR}/${_app_tag}_${app_src}.cpp)\n"
            "        add_custom_command(\n"
            "            OUTPUT ${_cpp}\n"
            "            COMMAND ${CMAKE_COMMAND} -E make_directory ${_MCS51_GEN_DIR}\n"
            "            COMMAND ${Python3_EXECUTABLE}\n"
            "                ${_MCS51_FW_DIR}/tools/transpile_app_keil_c51.py\n"
            "                ${_app_c} ${_cpp}\n"
            "            DEPENDS ${_app_c}\n"
            "                    ${_MCS51_FW_DIR}/tools/transpile_app_keil_c51.py\n"
            '            COMMENT "mcs51 app transpile: ${app_dir}/${app_src}.c -> ${_app_tag}_${app_src}.cpp"\n'
            "            VERBATIM)\n"
            "        set(_MCS51_${_app_tag}_${app_src}_CPP ${_cpp} PARENT_SCOPE)\n"
            "        # Backward-compatible variable alias for legacy test targets\n"
            "        set(_MCS51_mcs51_health_pot_health_pot_CPP ${_cpp} PARENT_SCOPE)\n"
            "    endfunction()\n\n"
            "    mcs51_transpile_app(appliances/health_pot health_pot)"
        )
        if old_fn in txt:
            txt = txt.replace(old_fn, new_fn)
            if not dry_run:
                test_cmake.write_text(txt, encoding="utf-8")
            print("  [patch] wink-micro-os/test/CMakeLists.txt updated.")

    # 3. user_surface.yaml
    lint_yaml = workspace_root / "wink-tools/tools/lint/rules/user_surface.yaml"
    if lint_yaml.is_file():
        txt = lint_yaml.read_text(encoding="utf-8")
        txt = txt.replace(
            "path: 'wink-micro-app/resource_conflict/**'",
            "path: 'wink-micro-app/fixtures/resource_conflict/**'",
        )
        txt = txt.replace(
            "path: 'wink-micro-app/dual_task_demo/**'",
            "path: 'wink-micro-app/native/dual_task_demo/**'",
        )
        if not dry_run:
            lint_yaml.write_text(txt, encoding="utf-8")
        print("  [patch] wink-tools/tools/lint/rules/user_surface.yaml updated.")

    # 4. wasm_node_smoke.cmake
    node_smoke = workspace_root / "wink-micro-os/targets/wasm/wasm_node_smoke.cmake"
    if node_smoke.is_file():
        txt = node_smoke.read_text(encoding="utf-8")
        txt = txt.replace(
            "-DWINK_APP_DIR=../wink-micro-app/unisim_smoke",
            "-DWINK_APP_DIR=../wink-micro-app/fixtures/unisim_smoke",
        )
        if not dry_run:
            node_smoke.write_text(txt, encoding="utf-8")
        print("  [patch] wink-micro-os/targets/wasm/wasm_node_smoke.cmake updated.")

    # 5. CI workflows
    for wf_name in ["clang-tidy.yml", "nightly.yml", "pr.yml"]:
        wf_path = workspace_root / f".github/workflows/{wf_name}"
        if wf_path.is_file():
            txt = wf_path.read_text(encoding="utf-8")
            if "wink-micro-app/unisim_smoke" in txt:
                txt = txt.replace(
                    "wink-micro-app/unisim_smoke",
                    "wink-micro-app/fixtures/unisim_smoke",
                )
                if not dry_run:
                    wf_path.write_text(txt, encoding="utf-8")
                print(f"  [patch] .github/workflows/{wf_name} updated.")


def main():
    parser = argparse.ArgumentParser(description="Reorganize wink-micro-app ecosystem into groups.")
    parser.add_argument("--dry-run", action="store_true", help="Preview moves and patches without changing disk")
    parser.add_argument("--apply", action="store_true", help="Execute the migration")
    args = parser.parse_args()

    if not args.apply and not args.dry_run:
        print("Please specify --dry-run to preview or --apply to execute.")
        sys.exit(1)

    workspace_root = Path(__file__).resolve().parents[1]
    apps_root = workspace_root / "wink-micro-app"

    print(f"Starting ecosystem migration for {len(APPS_MIGRATION)} apps in {apps_root}...\n")

    # 1. Process each app
    for item in APPS_MIGRATION:
        src_path = apps_root / item["src"]
        dst_path = apps_root / item["dst"]
        leaf_name = item["leaf"]

        print(f"Processing: {item['src']} -> {item['dst']}")
        run_git_mv(src_path, dst_path, dry_run=args.dry_run)

        # Target app path after move (or source during dry-run)
        cur_app_dir = dst_path if not args.dry_run else src_path
        patch_app_cmake(cur_app_dir / "CMakeLists.txt", item["dst"], dry_run=args.dry_run)
        patch_wink_app_json(cur_app_dir / "wink-app.json", leaf_name, dry_run=args.dry_run)
        print()

    # 2. Patch external and repo configurations
    print("Patching root build, test harnesses, CI workflows, and lint rules...")
    patch_repo_configs(workspace_root, dry_run=args.dry_run)

    mode_label = "DRY-RUN PREVIEW" if args.dry_run else "MIGRATION COMPLETE"
    print(f"\n✨ {mode_label}: Finished processing {len(APPS_MIGRATION)} apps.")


if __name__ == "__main__":
    main()
