# SPDX-License-Identifier: Apache-2.0
"""
g1_sla_evidence.py
==================
Gate 1 Rule: Verifies SLA error symbols and exclusion evidence for out-of-scope and expected_rejection entries.
"""

RULE_ID = "g1.sla_evidence"


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []

    for entry in context["manifest"].get("entries", []):
        display_id = entry.get("display_id")
        entry_id = entry.get("id")

        scope = entry.get("scope", {})
        inclusion = scope.get("inclusion")
        exclusion_reason = scope.get("exclusion_reason")

        # 1. Out-of-scope entries must have explicit exclusion reason
        if inclusion == "out_of_scope":
            if not exclusion_reason or not str(exclusion_reason).strip():
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": None,
                    "message": f"Entry #{display_id} ({entry_id}) is out_of_scope but missing 'scope.exclusion_reason'",
                })

        # 2. In-scope entries must not declare expected_rejection
        for ex in entry.get("executions", []):
            cid = ex.get("config_id")
            deliv = ex.get("delivery_state")
            acceptance = ex.get("acceptance", {})
            acc_type = acceptance.get("type")

            if inclusion == "in_scope" and acc_type == "expected_rejection":
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": cid,
                    "file_path": None,
                    "message": f"Entry #{display_id} config '{cid}' is in_scope but declares acceptance.type='expected_rejection'",
                })

            # 3. Expected rejection in building or verified state must declare sla_error_symbol
            if acc_type == "expected_rejection" and deliv in ("building", "verified"):
                sla_symbol = acceptance.get("sla_error_symbol")
                if not sla_symbol or not str(sla_symbol).strip():
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": (
                            f"Entry #{display_id} config '{cid}' has acceptance.type='expected_rejection' "
                            f"and delivery_state='{deliv}' but lacks 'sla_error_symbol'"
                        ),
                    })

    return findings
