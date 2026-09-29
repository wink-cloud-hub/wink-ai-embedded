# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_cap_id_exists.py.
"""

from rules import g1_cap_id_exists


def test_cap_id_exists_positive():
    context = {
        "manifest": {
            "entries": [
                {"id": "esp.a", "display_id": 1, "required_capabilities": ["cap.core.fiber_task"]}
            ]
        },
        "catalog": {
            "capabilities": {
                "cap.core.fiber_task": {}
            }
        }
    }
    findings = g1_cap_id_exists.run(context)
    assert len(findings) == 0


def test_cap_id_exists_negative_undeclared():
    context = {
        "manifest": {
            "entries": [
                {"id": "esp.a", "display_id": 1, "required_capabilities": ["cap.nonexistent.fake"]}
            ]
        },
        "catalog": {
            "capabilities": {
                "cap.core.fiber_task": {}
            }
        }
    }
    findings = g1_cap_id_exists.run(context)
    assert len(findings) == 1
    assert findings[0]["severity"] == "error"
    assert "undeclared capability ID: 'cap.nonexistent.fake'" in findings[0]["message"]
