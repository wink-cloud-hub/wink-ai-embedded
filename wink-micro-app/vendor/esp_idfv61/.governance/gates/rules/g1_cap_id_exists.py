# SPDX-License-Identifier: Apache-2.0
"""
g1_cap_id_exists.py
===================
Gate 1 Rule: Verifies that all required_capabilities are declared in capability-catalog.yaml.
"""

RULE_ID = "g1.cap_id_exists"


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    known_caps = set(context.get("catalog", {}).get("capabilities", {}).keys())

    for entry in context["manifest"].get("entries", []):
        display_id = entry.get("display_id")
        entry_id = entry.get("id")

        for cap_id in entry.get("required_capabilities", []):
            if cap_id not in known_caps:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": None,
                    "message": f"Reference to undeclared capability ID: '{cap_id}'",
                })

    return findings
