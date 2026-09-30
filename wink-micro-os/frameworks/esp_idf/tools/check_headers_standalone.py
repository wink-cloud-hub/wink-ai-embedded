#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Standalone header self-containment check for ESP-IDF facade (Task T1.5).

Verifies that all public headers under frameworks/esp_idf/include are standalone:
they can be compiled in isolation without requiring prior header includes,
using only the public include closure (facade include + shim + chip + pal/include).
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


# C 类：内部专用头与底层芯片硬件抽象头，不承诺向 App 独立自包含
VENDOR_INTERNAL_PREFIXES = (
    "esp_private/",
    "driver/esp_private/",
    "hal/",
    "soc/",
)

# 需由主头包裹的体系结构或片段子头（乐鑫原厂规范由主头包含，如 FreeRTOS 端口头与芯片基础宏）
WRAPPER_FRAGMENT_HEADERS = {
    "sdkconfig_base.h",           # 由 sdkconfig.h 包含
    "freertos/idf_additions.h",   # 由 freertos/FreeRTOS.h 包含
    "freertos/portable.h",        # 由 freertos/FreeRTOS.h 包含
    "freertos/portmacro.h",       # 由 freertos/portable.h 包含
    "driver/i2c_types_legacy.h",  # 由 driver/i2c.h 包含
}

# 原厂非独立片段头（Harvester 生成的原厂头，原厂头契约依赖前置宿主头，如 driver/uart.h 或 sys/queue.h）
VENDOR_CONTEXT_DEPENDENT_HEADERS = {
    "driver/gpio_filter.h",       # 原厂依赖 esp_pm / clock_source
    "driver/rtc_io.h",            # 原厂依赖 driver/gpio.h 声明的 rtc 类型
    "driver/spi_slave_hd.h",      # 原厂依赖 spi_slave 宿主
    "driver/uart_select.h",       # 原厂依赖 driver/uart.h + sys/select.h
    "driver/uart_vfs.h",          # 原厂依赖 esp_vfs.h
    "driver/uhci.h",              # 原厂底座 UHCI 依赖 uart_port_t
    "esp_clock_output.h",         # 原厂依赖 hal/gpio_types.h
    "esp_rom_lldesc.h",           # 原厂依赖 sys/queue.h STAILQ
    "esp_wake_stub.h",            # 原厂依赖 esp_sleep.h
    "miniz.h",                    # 第三方数据压缩库内部头
    "nvs_bootloader.h",           # bootloader 级 NVS 内部头
    "rtc_wdt.h",                  # 底层 RTC 看门狗内部头
}

# 跨靶仿真桥接头（B 类）
SIM_BRIDGE_HEADERS = {
    "esp_idf_wink.h",
}


def find_c_compiler() -> str:
    for candidate in ("gcc", "clang", "cl"):
        path = shutil.which(candidate)
        if path:
            return path
    return "gcc"


def compile_single_header(
    compiler: str,
    header_rel: str,
    include_dirs: list[Path],
    temp_dir: Path,
    extra_defs: list[str] | None = None,
) -> tuple[bool, str]:
    test_src = temp_dir / f"test_{header_rel.replace('/', '_').replace('.', '_')}.c"
    test_src.write_text(
        f'#include "{header_rel}"\nint main(void) {{ return 0; }}\n',
        encoding="utf-8",
    )
    obj_out = temp_dir / f"test_{header_rel.replace('/', '_').replace('.', '_')}.o"

    is_cl = "cl" in Path(compiler).stem.lower()
    cmd = [compiler]
    if is_cl:
        cmd.extend(["/c", str(test_src), f"/Fo:{obj_out}"])
        for inc in include_dirs:
            cmd.append(f"/I{inc}")
        if extra_defs:
            for d in extra_defs:
                cmd.append(f"/D{d}")
    else:
        cmd.extend(["-c", str(test_src), "-o", str(obj_out), "-Wall", "-Wextra", "-Wno-unused-parameter"])
        for inc in include_dirs:
            cmd.extend(["-I", str(inc)])
        if extra_defs:
            for d in extra_defs:
                cmd.append(f"-D{d}")

    res = subprocess.run(cmd, capture_output=True, text=True)
    success = res.returncode == 0
    output = res.stderr + "\n" + res.stdout
    return success, output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=None, help="C compiler to use")
    parser.add_argument("--esp-idf-dir", default=None, help="Path to frameworks/esp_idf")
    args = parser.parse_args()

    here = Path(__file__).resolve().parent
    esp_idf_dir = Path(args.esp_idf_dir).resolve() if args.esp_idf_dir else here.parent
    compiler = args.compiler or find_c_compiler()

    include_dir = esp_idf_dir / "include"
    shim_include_dir = esp_idf_dir / "shim" / "include"
    chip_include_dir = esp_idf_dir / "chips" / "esp32" / "include"
    pal_include_dir = esp_idf_dir.parents[1] / "pal" / "include"

    public_includes = [
        include_dir,
        shim_include_dir,
        chip_include_dir,
        pal_include_dir,
    ]

    all_headers = sorted(
        p.relative_to(include_dir).as_posix()
        for p in include_dir.rglob("*.h")
    )

    print(f"[header-standalone] Compiler: {compiler}")
    print(f"[header-standalone] Scanning {len(all_headers)} headers under {include_dir}")

    passed_count = 0
    skipped_count = 0
    failed_headers: list[tuple[str, str]] = []

    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_path = Path(tmp_dir)

        for rel in all_headers:
            # 1. C 类：内部头跳过单头自包含
            if any(rel.startswith(pfx) for pfx in VENDOR_INTERNAL_PREFIXES):
                skipped_count += 1
                continue

            # 2. 片段包装头与原厂上下文头跳过
            if rel in WRAPPER_FRAGMENT_HEADERS or rel in VENDOR_CONTEXT_DEPENDENT_HEADERS:
                skipped_count += 1
                continue

            # 3. B 类：跨靶仿真桥接头
            if rel in SIM_BRIDGE_HEADERS:
                # 3.1 正测（带仿真宏）
                ok, out = compile_single_header(compiler, rel, public_includes, tmp_path, ["SIMULATION=1", "WINK_SIM_TEST=1"])
                if not ok:
                    failed_headers.append((rel, f"Simulation build failed:\n{out}"))
                    continue
                # 3.2 负测（无仿真宏必须触发 #error）
                ok_no_sim, out_no_sim = compile_single_header(compiler, rel, public_includes, tmp_path, [])
                if ok_no_sim or "FATAL: This header is a WinkMicroOS simulation-only header" not in out_no_sim:
                    failed_headers.append((rel, f"Simulation guard failed (should have triggered #error):\n{out_no_sim}"))
                    continue
                passed_count += 1
                continue

            # 4. A 类：通用公开头
            ok, out = compile_single_header(compiler, rel, public_includes, tmp_path, ["SIMULATION=1", "WINK_SIM_TEST=1"])
            if ok:
                passed_count += 1
            else:
                failed_headers.append((rel, out))

    print(f"[header-standalone] Results: {passed_count} passed, {skipped_count} skipped, {len(failed_headers)} failed.")

    if failed_headers:
        print("[header-standalone] Failures:", file=sys.stderr)
        for rel, out in failed_headers[:10]:
            print(f"--- FAIL: {rel} ---\n{out.strip()}\n", file=sys.stderr)
        return 1

    print("[header-standalone] SUCCESS: 100% of tested public headers are self-contained!")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
