# SPDX-License-Identifier: Apache-2.0
"""
g1_orthogonal_states.py
=======================
Gate 1 Rule: Verifies 5-dimensional orthogonal state combinations across scope, schedule, audit, and delivery.
"""

RULE_ID = "g1.orthogonal_states"

VALID_INCLUSIONS = {"unknown", "in_scope", "out_of_scope"}
VALID_SCHEDULES = {"active", "deferred"}
VALID_VERDICTS = {"pending", "audited", "needs_review"}


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []

    for entry in context["manifest"].get("entries", []):
        display_id = entry.get("display_id")
        entry_id = entry.get("id")

        scope = entry.get("scope", {})
        audit = entry.get("audit", {})
        executions = entry.get("executions", [])

        inclusion = scope.get("inclusion")
        schedule = scope.get("schedule")
        verdict = audit.get("verdict")

        if inclusion not in VALID_INCLUSIONS:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": entry_id,
                "display_id": display_id,
                "config_id": None,
                "file_path": None,
                "message": f"Entry #{display_id} has invalid scope.inclusion '{inclusion}'",
            })

        if schedule not in VALID_SCHEDULES:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": entry_id,
                "display_id": display_id,
                "config_id": None,
                "file_path": None,
                "message": f"Entry #{display_id} has invalid scope.schedule '{schedule}'",
            })

        if verdict not in VALID_VERDICTS:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": entry_id,
                "display_id": display_id,
                "config_id": None,
                "file_path": None,
                "message": f"Entry #{display_id} has invalid audit.verdict '{verdict}'",
            })

        audited_configs = set(audit.get("audited_configs", []))

        for ex in executions:
            cid = ex.get("config_id")
            deliv = ex.get("delivery_state")

            # 1. Cannot advance delivery state beyond planned without an audit verdict
            if deliv in ("building", "verified", "stale", "regressed") and verdict == "pending":
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Entry #{display_id} config '{cid}' has delivery_state='{deliv}' but audit.verdict is 'pending' (cannot advance delivery without audit)",
                })

            # 2. Out-of-scope entries cannot be verified or building
            if inclusion == "out_of_scope" and deliv in ("building", "verified"):
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Entry #{display_id} config '{cid}' is out_of_scope but has delivery_state='{deliv}'",
                })

            # 3. Unknown scope entries cannot be verified or building
            if inclusion == "unknown" and deliv in ("building", "verified"):
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Entry #{display_id} config '{cid}' has scope.inclusion='unknown' but delivery_state='{deliv}'",
                })

            # 4. Verified entries must be in_scope, audited, and listed in audited_configs
            if deliv == "verified":
                if inclusion != "in_scope":
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": f"Entry #{display_id} config '{cid}' is verified but scope.inclusion is '{inclusion}'",
                    })

                if verdict != "audited":
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": f"Entry #{display_id} config '{cid}' is verified but audit.verdict is '{verdict}'",
                    })

                if cid not in audited_configs:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": f"Entry #{display_id} config '{cid}' is verified but not included in audit.audited_configs: {list(audited_configs)}",
                    })

    return findings
