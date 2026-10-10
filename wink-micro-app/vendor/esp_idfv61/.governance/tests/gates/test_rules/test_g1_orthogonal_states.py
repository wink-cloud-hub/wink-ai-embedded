# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_orthogonal_states.py.
"""

from gates.rules import g1_orthogonal_states


def test_orthogonal_states_positive():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.a",
                    "display_id": 1,
                    "scope": {"inclusion": "in_scope", "schedule": "active"},
                    "audit": {"verdict": "audited", "audited_configs": ["cfg1"]},
                    "executions": [
                        {"config_id": "cfg1", "delivery_state": "verified"}
                    ],
                }
            ]
        }
    }
    findings = g1_orthogonal_states.run(context)
    assert len(findings) == 0


def test_orthogonal_states_no_advance_without_audit():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.a",
                    "display_id": 1,
                    "scope": {"inclusion": "in_scope", "schedule": "active"},
                    "audit": {"verdict": "pending", "audited_configs": []},
                    "executions": [
                        {"config_id": "cfg1", "delivery_state": "building"}
                    ],
                }
            ]
        }
    }
    findings = g1_orthogonal_states.run(context)
    assert len(findings) == 1
    assert "cannot advance delivery without audit" in findings[0]["message"]


def test_orthogonal_states_out_of_scope_verified():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.a",
                    "display_id": 1,
                    "scope": {"inclusion": "out_of_scope", "schedule": "deferred"},
                    "audit": {"verdict": "audited", "audited_configs": ["cfg1"]},
                    "executions": [
                        {"config_id": "cfg1", "delivery_state": "verified"}
                    ],
                }
            ]
        }
    }
    findings = g1_orthogonal_states.run(context)
    assert len(findings) >= 1
    msgs = " ".join(f["message"] for f in findings)
    assert "out_of_scope but has delivery_state='verified'" in msgs
