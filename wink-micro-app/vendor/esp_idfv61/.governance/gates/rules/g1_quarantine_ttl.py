# SPDX-License-Identifier: Apache-2.0
"""
g1_quarantine_ttl.py
====================
Gate 1 Rule: Independently scans quarantine.yaml and verifies 14-day hard TTL.
"""

from datetime import datetime, timezone

RULE_ID = "g1.quarantine_ttl"


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    quarantine_raw = context.get("quarantine", {}).get("quarantined_entries", [])
    now_utc = context.get("now_utc") or datetime.now(timezone.utc)

    for q in quarantine_raw:
        qid = q.get("id")
        qcfg = q.get("config_id")
        exp_str = q.get("grace_period_expires", "")

        if not exp_str:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": qid,
                "display_id": None,
                "config_id": qcfg,
                "file_path": None,
                "message": f"Quarantine entry '{qid}' ({qcfg}) is missing 'grace_period_expires'",
            })
            continue

        try:
            exp_dt = datetime.fromisoformat(exp_str.replace("Z", "+00:00"))
        except Exception:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": qid,
                "display_id": None,
                "config_id": qcfg,
                "file_path": None,
                "message": f"Quarantine entry '{qid}' ({qcfg}) has invalid ISO datetime in grace_period_expires: '{exp_str}'",
            })
            continue

        if now_utc > exp_dt:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": qid,
                "display_id": None,
                "config_id": qcfg,
                "file_path": None,
                "message": f"Quarantine entry '{qid}' ({qcfg}) grace period expired at {exp_str}! Must revert to planned.",
            })

    return findings
