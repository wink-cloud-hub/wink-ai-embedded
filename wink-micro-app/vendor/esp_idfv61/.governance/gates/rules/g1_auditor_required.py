# SPDX-License-Identifier: Apache-2.0
"""
g1_auditor_required.py
======================
Gate 1 Rule: Verifies that audited entries designate a responsible auditor and audited_configs list.
"""

from datetime import datetime

RULE_ID = "g1.auditor_required"
PLACEHOLDERS = {"todo", "tbd", "placeholder", "?", "none", "null", "undefined"}


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []

    for entry in context["manifest"].get("entries", []):
        display_id = entry.get("display_id")
        entry_id = entry.get("id")

        audit = entry.get("audit", {})
        verdict = audit.get("verdict")

        if verdict == "audited":
            auditor = audit.get("auditor")
            if not auditor or not str(auditor).strip() or str(auditor).strip().lower() in PLACEHOLDERS:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": None,
                    "message": f"Entry #{display_id} ({entry_id}) has verdict='audited' but lacks a valid 'auditor'",
                })

            audited_configs = audit.get("audited_configs")
            if not audited_configs or not isinstance(audited_configs, list) or len(audited_configs) == 0:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": None,
                    "message": f"Entry #{display_id} ({entry_id}) has verdict='audited' but 'audited_configs' array is empty",
                })

            audited_at = audit.get("audited_at")
            if not audited_at:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": None,
                    "message": f"Entry #{display_id} ({entry_id}) has verdict='audited' but lacks 'audited_at'",
                })
            else:
                try:
                    datetime.fromisoformat(str(audited_at).replace("Z", "+00:00"))
                except Exception:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": None,
                        "file_path": None,
                        "message": f"Entry #{display_id} ({entry_id}) audited_at is not valid ISO datetime: '{audited_at}'",
                    })

    return findings
