# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_path_unique.py.
"""

from rules import g1_path_unique


def test_path_unique_positive():
    context = {
        "manifest": {
            "entries": [
                {"id": "esp.a", "display_id": 1, "upstream_path": "examples/a"},
                {"id": "esp.b", "display_id": 2, "upstream_path": "examples/b"},
            ]
        }
    }
    findings = g1_path_unique.run(context)
    assert len(findings) == 0


def test_path_unique_negative_duplicate():
    context = {
        "manifest": {
            "entries": [
                {"id": "esp.a", "display_id": 1, "upstream_path": "examples/dup"},
                {"id": "esp.b", "display_id": 2, "upstream_path": "examples/dup"},
            ]
        }
    }
    findings = g1_path_unique.run(context)
    assert len(findings) == 1
    assert findings[0]["severity"] == "error"
    assert "Duplicate upstream_path" in findings[0]["message"]
    assert findings[0]["display_id"] == 2


def test_path_unique_negative_missing():
    context = {
        "manifest": {
            "entries": [
                {"id": "esp.a", "display_id": 1, "upstream_path": None},
            ]
        }
    }
    findings = g1_path_unique.run(context)
    assert len(findings) == 1
    assert "missing 'upstream_path'" in findings[0]["message"]


def test_target_app_dir_duplicate():
    context = {
        "manifest": {
            "entries": [
                {"id": "esp.a", "display_id": 1, "upstream_path": "examples/a", "target_app_dir": "peripherals/dup"},
                {"id": "esp.b", "display_id": 2, "upstream_path": "examples/b", "target_app_dir": "peripherals/dup"},
            ]
        }
    }
    findings = g1_path_unique.run(context)
    assert len(findings) == 1
    assert "Duplicate target_app_dir" in findings[0]["message"]

