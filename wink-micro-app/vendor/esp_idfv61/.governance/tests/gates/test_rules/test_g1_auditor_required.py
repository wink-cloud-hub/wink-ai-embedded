# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_auditor_required.py.
"""

from gates.rules import g1_auditor_required


def test_auditor_required_positive():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.a",
                    "display_id": 1,
                    "audit": {
                        "verdict": "audited",
                        "auditor": "arch_team",
                        "audited_at": "2026-09-29T14:00:00Z",
                        "audited_configs": ["wasm_sim_standard"],
                    },
                }
            ]
        }
    }
    findings = g1_auditor_required.run(context)
    assert len(findings) == 0


def test_auditor_required_missing_auditor():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.a",
                    "display_id": 1,
                    "audit": {
                        "verdict": "audited",
                        "auditor": None,
                        "audited_at": "2026-09-29T14:00:00Z",
                        "audited_configs": ["wasm_sim_standard"],
                    },
                }
            ]
        }
    }
    findings = g1_auditor_required.run(context)
    assert len(findings) == 1
    assert "lacks a valid 'auditor'" in findings[0]["message"]


def test_auditor_required_placeholder_auditor():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.a",
                    "display_id": 1,
                    "audit": {
                        "verdict": "audited",
                        "auditor": "TODO",
                        "audited_at": "2026-09-29T14:00:00Z",
                        "audited_configs": ["wasm_sim_standard"],
                    },
                }
            ]
        }
    }
    findings = g1_auditor_required.run(context)
    assert len(findings) == 1
    assert "lacks a valid 'auditor'" in findings[0]["message"]


def test_auditor_required_empty_configs():
    context = {
        "manifest": {
            "entries": [
                {
                    "id": "esp.a",
                    "display_id": 1,
                    "audit": {
                        "verdict": "audited",
                        "auditor": "arch_team",
                        "audited_at": "2026-09-29T14:00:00Z",
                        "audited_configs": [],
                    },
                }
            ]
        }
    }
    findings = g1_auditor_required.run(context)
    assert len(findings) == 1
    assert "'audited_configs' array is empty" in findings[0]["message"]
