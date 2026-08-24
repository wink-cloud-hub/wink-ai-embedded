#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
check_wasm_abi_hash.py - Validates PAL_WASM_ABI_HASH against normalized wasm_bridge.h extern declarations.
"""
import sys
import re
import hashlib
from pathlib import Path

def extract_normalized_decls(header_path: Path) -> list:
    content = header_path.read_text(encoding="utf-8")
    # Strip C-style multiline and line comments
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    content = re.sub(r"//.*", "", content)
    
    # Match extern declarations
    raw_decls = re.findall(r"extern\s+([^;]+);", content)
    normalized = []
    for d in raw_decls:
        norm = " ".join(d.split())
        if norm and not norm.startswith('"C"'):
            normalized.append(f"extern {norm};")
    return sorted(set(normalized))

def main():
    repo_root = Path(__file__).resolve().parent.parent
    bridge_h = repo_root / "targets" / "wasm" / "wasm_bridge.h"
    degradation_c = repo_root / "targets" / "wasm" / "pal_wasm_degradation.c"

    if not bridge_h.exists() or not degradation_c.exists():
        print(f"Error: Missing {bridge_h} or {degradation_c}")
        return 1

    decls = extract_normalized_decls(bridge_h)
    if not decls:
        print("Error: No extern declarations found in wasm_bridge.h")
        return 1

    data = "\n".join(decls).encode("utf-8")
    computed_sha = hashlib.sha256(data).hexdigest()
    computed_u32 = int(computed_sha[:8], 16)

    deg_content = degradation_c.read_text(encoding="utf-8")
    hash_match = re.search(r"#define\s+PAL_WASM_ABI_HASH\s+(0x[0-9a-fA-F]+u?)", deg_content)
    if not hash_match:
        print("Error: Could not find PAL_WASM_ABI_HASH define in pal_wasm_degradation.c")
        return 1

    current_hash_str = hash_match.group(1).rstrip("uU")
    current_u32 = int(current_hash_str, 16)

    print(f"Extern declarations in wasm_bridge.h: {len(decls)}")
    print(f"Computed SHA256: {computed_sha}")
    print(f"Expected PAL_WASM_ABI_HASH: 0x{computed_u32:08X}")
    print(f"Current PAL_WASM_ABI_HASH:  0x{current_u32:08X}")

    if computed_u32 != current_u32:
        print(f"FAILED: PAL_WASM_ABI_HASH mismatch! Expected 0x{computed_u32:08X}u but found 0x{current_u32:08X}u in pal_wasm_degradation.c")
        print("Please update PAL_WASM_ABI_HASH in pal_wasm_degradation.c to match the updated wasm_bridge.h contract.")
        return 1

    print("ABI Check Passed!")
    return 0

if __name__ == "__main__":
    sys.exit(main())
