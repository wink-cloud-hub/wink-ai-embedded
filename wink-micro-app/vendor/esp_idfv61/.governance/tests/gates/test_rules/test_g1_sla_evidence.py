# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_sla_evidence.py.
"""

from rules import g1_sla_evidence


def test_sla_evidence_positive():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.phy.cert",
                    "display_id": 478,
                    "scope": {
                        "inclusion": "out_of_scope",
                        "exclusion_reason": "Physical hardware RF calibration required",
                    },
                    "executions": [
                        {
                            "config_id": "cfg1",
                            "delivery_state": "planned",
                            "acceptance": {
                                "type": "expected_rejection",
                                "sla_error_symbol": None,
                            },
                        }
                    ],
                }
            ]
        }
    }
    findings = g1_sla_evidence.run(context)
    assert len(findings) == 0


def test_sla_evidence_out_of_scope_missing_reason():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.bad_oos",
                    "display_id": 10,
                    "scope": {
                        "inclusion": "out_of_scope",
                        "exclusion_reason": "",
                    },
                    "executions": [],
                }
            ]
        }
    }
    findings = g1_sla_evidence.run(context)
    assert len(findings) == 1
    assert "missing 'scope.exclusion_reason'" in findings[0]["message"]


def test_sla_evidence_verified_expected_rejection_missing_symbol():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.bad_symbol",
                    "display_id": 11,
                    "scope": {
                        "inclusion": "out_of_scope",
                        "exclusion_reason": "Reason given",
                    },
                    "executions": [
                        {
                            "config_id": "cfg1",
                            "delivery_state": "verified",
                            "acceptance": {
                                "type": "expected_rejection",
                                "sla_error_symbol": None,
                            },
                        }
                    ],
                }
            ]
        }
    }
    findings = g1_sla_evidence.run(context)
    assert len(findings) == 1
    assert "lacks 'sla_error_symbol'" in findings[0]["message"]
