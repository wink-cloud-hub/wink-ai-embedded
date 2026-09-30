# SPDX-License-Identifier: Apache-2.0
"""
g5_reset_registration_verified.py
=================================
Gate 5 Rule: Verifies that any module introducing file-scope static state, object pools,
or state machines registers and invokes a reset function in esp_idf_bridge.c.
Prevents cross-test state leakage across soft resets (esp_restart()).
"""

import re
from pathlib import Path

RULE_ID = "g5.reset_registration_verified"

# Regex to detect file-scope static state definitions (any variable, not just s_/g_/m_)
STATIC_STATE_PATTERN = re.compile(
    r'^\s*static\s+(?!const\b)(?!inline\b)(?:struct\s+\w+|\w+)\s+(?:\*+\s*)?([a-zA-Z_]\w*)\s*(?:\[[^\]]*\])?\s*(?:=[^;]+)?\s*;',
    re.MULTILINE,
)

# Regex to detect reset function declarations/definitions
RESET_FUNC_PATTERN = re.compile(
    r'\b(?:esp_\w+_(?:sim_)?reset|sim_\w+_reset|\w+_deinit)\s*\(\s*void\s*\)',
)


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    ws_root = Path(context.get("workspace_root", "."))
    src_dir = ws_root / "wink-micro-os" / "frameworks" / "esp_idf" / "src"
    bridge_file = src_dir / "esp_idf_bridge.c"

    if not src_dir.exists() or not bridge_file.is_file():
        return findings

    try:
        bridge_content = bridge_file.read_text(encoding="utf-8", errors="replace")
    except Exception as e:
        findings.append({
            "rule_id": RULE_ID,
            "severity": "error",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": str(bridge_file.relative_to(ws_root)),
            "message": f"Failed to read esp_idf_bridge.c: {e}",
        })
        return findings

    changed_files = context.get("changed_files", [])
    target_files = []
    if changed_files:
        for f in changed_files:
            p = ws_root / f
            if p.suffix == ".c" and src_dir in p.parents and p != bridge_file:
                target_files.append(p)
    else:
        target_files = [p for p in src_dir.rglob("*.c") if p != bridge_file]

    # Strip comments from bridge_content to prevent false positives from comment mentions
    bridge_clean = re.sub(r'//.*', '', bridge_content)
    bridge_clean = re.sub(r'/\*.*?\*/', '', bridge_clean, flags=re.DOTALL)

    for c_file in target_files:
        try:
            content = c_file.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue

        # Look for file-scope static variables
        static_matches = STATIC_STATE_PATTERN.findall(content)
        if not static_matches:
            continue

        # File defines static state; find its reset function
        reset_matches = RESET_FUNC_PATTERN.findall(content)
        if not reset_matches:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": str(c_file.relative_to(ws_root)),
                "message": (
                    f"Module declares static state ({', '.join(static_matches[:3])}) but defines "
                    f"no esp_*_sim_reset() or sim_*_reset() cleanup hook. "
                    f"All static state must be cleanly wipeable on soft reset."
                ),
            })
            continue

        # Check whether at least one reset function is wired into esp_idf_bridge.c
        wired = False
        for rf in reset_matches:
            fname = rf.split("(")[0].strip()
            if re.search(rf'\b{re.escape(fname)}\s*\(', bridge_clean):
                wired = True
                break

        if not wired:
            fname = reset_matches[0].split("(")[0].strip()
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": str(c_file.relative_to(ws_root)),
                "message": (
                    f"Module defines reset function '{fname}()', but it is not called inside "
                    f"esp_idf_bridge.c reset DAG. Must be registered in pal_wasm_target_clear_pending_reset()."
                ),
            })

    return findings
