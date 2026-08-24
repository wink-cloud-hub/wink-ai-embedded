#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
update_wasm_abi_hash.py - Computes normalized SHA-256[0:8] hash of wasm_bridge.h and updates pal_wasm_degradation.c.
"""
import sys
import re
import hashlib
from pathlib import Path

def extract_normalized_decls(header_path: Path) -> list:
    content = header_path.read_text(encoding="utf-8")
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    content = re.sub(r"//.*", "", content)
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
        print(f"Error: Missing {bridge_h} or {degradation_c}", file=sys.stderr)
        return 1

    decls = extract_normalized_decls(bridge_h)
    data = "\n".join(decls).encode("utf-8")
    computed_sha = hashlib.sha256(data).hexdigest()
    computed_hex = computed_sha[:8].upper()
    computed_u32_str = f"0x{computed_hex}u"

    print(f"Total normalized extern decls: {len(decls)}")
    print(f"Full SHA256: {computed_sha}")
    print(f"Computed PAL_WASM_ABI_HASH: {computed_u32_str}")

    deg_content = degradation_c.read_text(encoding="utf-8")
    new_deg_content = re.sub(
        r"#define\s+PAL_WASM_ABI_HASH\s+0x[0-9a-fA-F]+u?",
        f"#define PAL_WASM_ABI_HASH {computed_u32_str}",
        deg_content
    )
    degradation_c.write_text(new_deg_content, encoding="utf-8")
    print(f"Updated {degradation_c} with {computed_u32_str}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
