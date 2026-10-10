# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_quarantine_ttl.py.
"""

from datetime import datetime, timezone
from gates.rules import g1_quarantine_ttl


def test_quarantine_ttl_within_grace_period():
    context = {
        "now_utc": datetime(2026, 9, 29, tzinfo=timezone.utc),
        "quarantine": {
            "quarantined_entries": [
                {
                    "id": "esp.a",
                    "config_id": "wasm_sim_standard",
                    "grace_period_expires": "2026-10-13T23:59:59Z",
                }
            ]
        },
    }
    findings = g1_quarantine_ttl.run(context)
    assert len(findings) == 0


def test_quarantine_ttl_expired_blocking_error():
    context = {
        "now_utc": datetime(2026, 10, 14, 0, 0, 0, tzinfo=timezone.utc),
        "quarantine": {
            "quarantined_entries": [
                {
                    "id": "esp.a",
                    "config_id": "wasm_sim_standard",
                    "grace_period_expires": "2026-10-13T23:59:59Z",
                }
            ]
        },
    }
    findings = g1_quarantine_ttl.run(context)
    assert len(findings) == 1
    assert findings[0]["severity"] == "error"
    assert "grace period expired at 2026-10-13T23:59:59Z" in findings[0]["message"]
