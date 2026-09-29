# SPDX-License-Identifier: Apache-2.0
"""
g2_cap_has_owned_paths.py
=========================
Gate 2 Rule: Verifies that each capability in capability-catalog.yaml declares a non-empty owned_paths list.
"""

RULE_ID = "g2.cap_has_owned_paths"


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    capabilities = context.get("catalog", {}).get("capabilities", {})

    for cap_id, cap_def in capabilities.items():
        if not isinstance(cap_def, dict):
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": cap_id,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": f"Capability '{cap_id}' definition is not an object",
            })
            continue

        owned_paths = cap_def.get("owned_paths")
        if not owned_paths or not isinstance(owned_paths, list) or len(owned_paths) == 0:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": cap_id,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": f"Capability '{cap_id}' must define non-empty 'owned_paths' list",
            })

    return findings
