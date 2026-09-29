# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g3_winkcli_lint.py.
"""

from pathlib import Path
from rules import g3_winkcli_lint


def test_g3_strict_missing_toolchain_fails_closed(monkeypatch):
    # Ensure no winkcli in PATH and mock finder to return None
    monkeypatch.setattr(g3_winkcli_lint, "find_winkcli_executable", lambda ws: None)

    context = {
        "workspace_root": ".",
        "changed_files": ["wink-micro-os/pal/pal_gpio.c"],
    }
    findings = g3_winkcli_lint.run(context, {"strict_environment": True})
    assert len(findings) == 1
    assert findings[0]["severity"] == "error"
    assert "Fail-Closed" in findings[0]["message"]


def test_g3_suppress_allowlisted_items(monkeypatch, tmp_path):
    # Mock subprocess.run to return sample lint json output with allowlisted and non-allowlisted items
    sample_output = [
        {
            "rule_id": "BAL-SRC-HAL-ALLOWLIST",
            "severity": "error",
            "path": "bal/src/irq.c",
            "line": 10,
            "message": "Allowlisted issue",
            "allowlisted": True,
        },
        {
            "rule_id": "LAYER-VIOLATION",
            "severity": "error",
            "path": "app/main.c",
            "line": 42,
            "message": "Direct include of pal_inner.h is forbidden",
            "allowlisted": False,
        }
    ]

    class MockProc:
        returncode = 1
        stdout = __import__("json").dumps(sample_output)
        stderr = ""

    monkeypatch.setattr(g3_winkcli_lint, "find_winkcli_executable", lambda ws: ["dummy_wink"])
    monkeypatch.setattr(g3_winkcli_lint.subprocess, "run", lambda *args, **kwargs: MockProc())

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": ["wink-micro-os/app/main.c"],
    }
    findings = g3_winkcli_lint.run(context)
    # The allowlisted item must be filtered out, leaving only the real violation
    assert len(findings) == 1
    assert findings[0]["rule_id"] == "g3.winkcli.LAYER-VIOLATION"
    assert findings[0]["severity"] == "error"
    assert "Direct include of pal_inner.h is forbidden" in findings[0]["message"]
