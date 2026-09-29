# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g2_cap_has_owned_paths.py and g2_cap_cross_mcu.py.
"""

from rules import g2_cap_has_owned_paths, g2_cap_cross_mcu


def test_cap_has_owned_paths_positive():
    context = {
        "catalog": {
            "capabilities": {
                "cap.a": {"owned_paths": ["pal/a.h"]}
            }
        }
    }
    findings = g2_cap_has_owned_paths.run(context)
    assert len(findings) == 0


def test_cap_has_owned_paths_empty_fails():
    context = {
        "catalog": {
            "capabilities": {
                "cap.a": {"owned_paths": []}
            }
        }
    }
    findings = g2_cap_has_owned_paths.run(context)
    assert len(findings) == 1
    assert "must define non-empty 'owned_paths'" in findings[0]["message"]


def test_cap_cross_mcu_warning_when_missing():
    context = {
        "catalog": {
            "capabilities": {
                "cap.a": {"status": "implemented", "cross_mcu_evidence": []}
            }
        }
    }
    findings = g2_cap_cross_mcu.run(context)
    assert len(findings) == 1
    assert findings[0]["severity"] == "warning"
    assert "lacks 'cross_mcu_evidence'" in findings[0]["message"]
