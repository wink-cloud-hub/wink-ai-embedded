#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
check_wasm_stub_symbols.py - Validates that JS stubs declare all wasm_bridge.h symbols.
"""
import sys
import re
from pathlib import Path

def main():
    repo_root = Path(__file__).resolve().parent.parent
    bridge_h = repo_root / "targets" / "wasm" / "wasm_bridge.h"
    stub_js = repo_root / "targets" / "wasm" / "wink_sim_stub.js"
    sim_js = repo_root / "targets" / "wasm" / "wink_sim_js.js"

    if not bridge_h.exists():
        print(f"Error: Missing {bridge_h}")
        return 1

    content_h = bridge_h.read_text(encoding="utf-8")
    symbols = set(re.findall(r"extern\s+[\w\*\s]+\s+(js_pal_\w+)\s*\([^)]*\)\s*;", content_h))

    stub_content = ""
    if stub_js.exists():
        stub_content += stub_js.read_text(encoding="utf-8")
    if sim_js.exists():
        stub_content += sim_js.read_text(encoding="utf-8")

    missing = []
    for sym in sorted(symbols):
        if sym not in stub_content:
            missing.append(sym)

    if missing:
        print(f"FAILED: {len(missing)} symbols from wasm_bridge.h not directly matched in JS stubs: {missing}")
        return 1

    print(f"All {len(symbols)} wasm_bridge.h symbols present in JS implementation / stubs.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
