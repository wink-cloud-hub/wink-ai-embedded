#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build and execute the ESP-IDF C++ global-constructor Wasm fixture."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


def run(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    print("+", subprocess.list2cmdline(command), flush=True)
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    if result.returncode:
        raise RuntimeError(f"command failed with exit code {result.returncode}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--emcmake", required=True)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--node", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--fixture", required=True)
    parser.add_argument("--tools", required=True)
    parser.add_argument("--build", required=True)
    args = parser.parse_args()

    source = Path(args.source).resolve()
    fixture = Path(args.fixture).resolve()
    build = Path(args.build).resolve()
    tools = Path(args.tools).resolve()
    build.mkdir(parents=True, exist_ok=True)

    run(
        [
            args.emcmake,
            args.cmake,
            "-S",
            str(source),
            "-B",
            str(build),
            "-DTARGET_PLATFORM=wasm",
            "-DWINK_SDK_MODE=source",
            f"-DWINK_APP_DIR={fixture}",
            "-DWINK_APP_ESP_IDF=ON",
            f"-DWINK_TOOLS_ROOT={tools}",
        ],
        source.parent,
    )
    run([args.cmake, "--build", str(build), "--target", "wink_simulator", "-j", "4"], source.parent)

    module = (build / "wink_simulator.js").as_posix()
    js = (
        "const createModule=require(process.argv[1]);"
        "createModule().then(m=>{"
        "if(m._esp_idf_wasm_request_reset===undefined||"
        "m._esp_idf_wasm_verify_reset_resources===undefined)throw Error('reset exports missing');"
        "m._esp_idf_wasm_request_reset();"
        "if(m._pal_wasm_has_pending_reset()!==1)throw Error('reset request not observed');"
        "m._pal_wasm_clear_pending_reset();"
        "if(m._pal_wasm_has_pending_reset()!==0)throw Error('reset request not cleared');"
        "if(m._esp_idf_wasm_verify_reset_resources()!==0)throw Error('reset resources not reinitialized');"
        "m._pal_wasm_app_tick();"
        "if(m._esp_idf_wasm_h4_begin===undefined||"
        "m._esp_idf_wasm_h4_at_deadline===undefined||"
        "m._esp_idf_wasm_h4_verify===undefined)throw Error('H4 exports missing');"
        "if(m._esp_idf_wasm_h4_begin()!==0)throw Error('H4 setup failed');"
        "m._pal_wasm_app_tick();"
        "if(m._esp_idf_wasm_h4_verify(0)!==0)throw Error('H4 waiter did not block');"
        "m._esp_idf_wasm_h4_at_deadline();"
        "m._pal_wasm_app_tick();"
        "if(m._esp_idf_wasm_h4_verify(1)!==0)throw Error('H4 same-time IRQ/timeout order failed');"
        "process.stdout.write('ESP_IDF_WASM_H4_SAME_TIME_OK\\n');"
        "process.stdout.write('ESP_IDF_WASM_RESET_CONTRACT_OK\\nWASM_MODULE_READY\\n');"
        "});"
    )
    result = run([args.node, "-e", js, module], source.parent)
    output = result.stdout + result.stderr
    if "ESP-IDF_WASM_CTOR_RESOURCES_OK" not in output:
        raise RuntimeError("Wasm app_main did not consume the global-constructor resources")
    if "ESP_IDF_WASM_HEAP_CONTRACT_OK" not in output:
        raise RuntimeError("Wasm heap_caps runtime contract checks did not complete")
    if "ESP_IDF_WASM_RESET_CONTRACT_OK" not in output:
        raise RuntimeError("Wasm reset contract checks did not complete")
    if "ESP_IDF_WASM_H4_SAME_TIME_OK" not in output:
        raise RuntimeError("Wasm H4 same-time IRQ/timeout checks did not complete")
    if "WASM_MODULE_READY" not in output:
        raise RuntimeError("Wasm module did not complete initialization")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
