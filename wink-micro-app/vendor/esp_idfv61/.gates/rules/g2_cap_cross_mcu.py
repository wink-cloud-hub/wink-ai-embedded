# SPDX-License-Identifier: Apache-2.0
"""
g2_cap_cross_mcu.py
===================
Gate 2 Rule: Verifies that implemented capabilities declare cross_mcu_evidence.
"""

RULE_ID = "g2.cap_cross_mcu"


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    capabilities = context.get("catalog", {}).get("capabilities", {})

    for cap_id, cap_def in capabilities.items():
        if not isinstance(cap_def, dict):
            continue

        status = cap_def.get("status")
        if status in ("implemented", "verified"):
            cross_mcu = cap_def.get("cross_mcu_evidence")
            if not cross_mcu or not isinstance(cross_mcu, list) or len(cross_mcu) == 0:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "warning",
                    "entry_id": cap_id,
                    "display_id": None,
                    "config_id": None,
                    "file_path": None,
                    "message": f"Capability '{cap_id}' has status='{status}' but lacks 'cross_mcu_evidence'",
                })

    return findings
