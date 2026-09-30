#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Negative boundary test runner for ESP-IDF facade (Task T1.4).

Verifies that private framework headers (freertos_sync.h, sim_wifi_env.h,
sim_network_broker.h, sim_net_responder.h) are inaccessible to external consumers
using only the PUBLIC include directories of wink_framework_esp_idf.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def find_c_compiler() -> str:
    for candidate in ("gcc", "clang", "cl"):
        path = shutil.which(candidate)
        if path:
            return path
    return "gcc"


def test_header_isolated(
    compiler: str,
    header_name: str,
    public_includes: list[Path],
    temp_dir: Path,
) -> tuple[bool, str]:
    test_src = temp_dir / f"test_{header_name.replace('/', '_').replace('.', '_')}.c"
    test_src.write_text(
        f'#include "esp_err.h"\n#include "{header_name}"\nint main(void) {{ return 0; }}\n',
        encoding="utf-8",
    )
    obj_out = temp_dir / f"test_{header_name.replace('/', '_').replace('.', '_')}.o"

    is_cl = "cl" in Path(compiler).stem.lower()
    if is_cl:
        cmd = [compiler, "/c", str(test_src), f"/Fo:{obj_out}"]
        for inc in public_includes:
            cmd.append(f"/I{inc}")
    else:
        cmd = [compiler, "-c", str(test_src), "-o", str(obj_out)]
        for inc in public_includes:
            cmd.extend(["-I", str(inc)])

    res = subprocess.run(cmd, capture_output=True, text=True)
    # Success means failure of isolation!
    isolated = res.returncode != 0 and (header_name in res.stderr or header_name in res.stdout)
    output = res.stderr + "\n" + res.stdout
    return isolated, output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=None, help="C compiler to use")
    parser.add_argument("--esp-idf-dir", default=None, help="Path to frameworks/esp_idf")
    args = parser.parse_args()

    here = Path(__file__).resolve().parent
    esp_idf_dir = Path(args.esp_idf_dir).resolve() if args.esp_idf_dir else here.parents[2]
    compiler = args.compiler or find_c_compiler()

    public_includes = [
        esp_idf_dir / "include",
        esp_idf_dir / "shim" / "include",
        esp_idf_dir / "chips" / "esp32" / "include",
    ]

    private_headers = [
        "freertos_sync.h",
        "sim_wifi_env.h",
        "sim_network_broker.h",
        "sim_net_responder.h",
    ]

    print(f"[negative-boundary-test] Using compiler: {compiler}")
    print(f"[negative-boundary-test] Public includes: {[str(p) for p in public_includes]}")

    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_path = Path(tmp_dir)
        all_passed = True
        for hdr in private_headers:
            isolated, out = test_header_isolated(compiler, hdr, public_includes, tmp_path)
            if isolated:
                print(f"[negative-boundary-test] PASS: '{hdr}' correctly blocked from public boundary")
            else:
                print(f"[negative-boundary-test] FAIL: '{hdr}' was NOT blocked! Output:\n{out}", file=sys.stderr)
                all_passed = False

    if all_passed:
        print("[negative-boundary-test] ALL private headers successfully isolated.")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
