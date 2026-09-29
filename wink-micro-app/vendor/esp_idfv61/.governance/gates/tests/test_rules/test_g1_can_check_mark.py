# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_can_check_mark.py.
"""

from datetime import datetime, timezone
from rules import g1_can_check_mark

VALID_HASH = "4b72cd7f72c7cffcba2b2ed504a87e92715b61dd9fdf36e5cef89476940ddf6c"


def test_can_check_mark_positive_golden():
    context = {
        "now_utc": datetime(2026, 9, 29, tzinfo=timezone.utc),
        "manifest": {
            "entries": [
                {
                    "id": "esp.golden",
                    "display_id": 1,
                    "scope": {"inclusion": "in_scope"},
                    "audit": {"verdict": "audited", "audited_configs": ["cfg1"]},
                    "required_capabilities": ["cap.mock"],
                    "executions": [
                        {
                            "config_id": "cfg1",
                            "delivery_state": "verified",
                            "evidence": {
                                "assets_sha256": VALID_HASH,
                                "scenario_sha256": VALID_HASH,
                                "execution_report_ref": "reports/pass.json",
                            },
                        }
                    ],
                }
            ]
        },
        "catalog": {"capabilities": {"cap.mock": {"status": "implemented"}}},
        "quarantine": {"quarantined_entries": []},
    }
    findings = g1_can_check_mark.run(context)
    assert len(findings) == 0


def test_can_check_mark_null_evidence_rejected_with_error():
    context = {
        "now_utc": datetime(2026, 9, 29, tzinfo=timezone.utc),
        "manifest": {
            "entries": [
                {
                    "id": "esp.fake",
                    "display_id": 2,
                    "scope": {"inclusion": "in_scope"},
                    "audit": {"verdict": "audited", "audited_configs": ["cfg1"]},
                    "executions": [
                        {
                            "config_id": "cfg1",
                            "delivery_state": "verified",
                            "evidence": None,
                        }
                    ],
                }
            ]
        },
        "catalog": {"capabilities": {}},
        "quarantine": {"quarantined_entries": []},
    }
    findings = g1_can_check_mark.run(context)
    assert len(findings) >= 1
    assert findings[0]["severity"] == "error"
    assert "evidence is null" in findings[0]["message"]


def test_can_check_mark_quarantine_grace_period_warning():
    context = {
        "now_utc": datetime(2026, 9, 29, tzinfo=timezone.utc),
        "manifest": {
            "entries": [
                {
                    "id": "esp.quarantined",
                    "display_id": 3,
                    "executions": [
                        {
                            "config_id": "wasm_sim_standard",
                            "delivery_state": "verified",
                            "evidence": None,
                        }
                    ],
                }
            ]
        },
        "catalog": {"capabilities": {}},
        "quarantine": {
            "quarantined_entries": [
                {
                    "id": "esp.quarantined",
                    "config_id": "wasm_sim_standard",
                    "grace_period_expires": "2026-10-13T23:59:59Z",
                }
            ]
        },
    }
    findings = g1_can_check_mark.run(context)
    assert len(findings) == 1
    assert findings[0]["severity"] == "warning"
    assert "in quarantine debt allowlist" in findings[0]["message"]


def test_can_check_mark_quarantine_expired_error():
    context = {
        "now_utc": datetime(2026, 10, 15, tzinfo=timezone.utc),  # Past 2026-10-13
        "manifest": {
            "entries": [
                {
                    "id": "esp.quarantined",
                    "display_id": 3,
                    "executions": [
                        {
                            "config_id": "wasm_sim_standard",
                            "delivery_state": "verified",
                            "evidence": None,
                        }
                    ],
                }
            ]
        },
        "catalog": {"capabilities": {}},
        "quarantine": {
            "quarantined_entries": [
                {
                    "id": "esp.quarantined",
                    "config_id": "wasm_sim_standard",
                    "grace_period_expires": "2026-10-13T23:59:59Z",
                }
            ]
        },
    }
    findings = g1_can_check_mark.run(context)
    assert len(findings) == 1
    assert findings[0]["severity"] == "error"
    assert "grace period expired" in findings[0]["message"]
