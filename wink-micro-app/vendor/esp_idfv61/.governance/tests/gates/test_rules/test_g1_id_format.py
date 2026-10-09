# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_id_format.py.

Restored alongside the gate engine: this rule was registered in gates.yaml and
shipped in __pycache__ as a .pyc, but its test source was missing.
"""

from rules import g1_id_format


def _ctx(*ids):
    return {"manifest": {"entries": [
        {"id": i, "display_id": n} for n, i in enumerate(ids, 1)
    ]}}


def test_valid_dotted_ids_pass():
    findings = g1_id_format.run(_ctx(
        "esp.get_started.blink",
        "esp.peripherals.adc.oneshot_read",
        "esp.build_system.cmake.component_manager",
    ))
    assert findings == []


def test_rejects_malformed_ids():
    bad = [
        "blink",                      # no namespace
        "esp.blink",                  # only one segment after namespace
        "esp.Blink",                  # uppercase
        "esp.get-started.blink",      # hyphen
        "esp.get_started.blink ",     # trailing space
        "",                           # empty
    ]
    findings = g1_id_format.run(_ctx(*bad))
    assert len(findings) == len(bad), findings


def test_rejects_trailing_newline():
    """`$` in Python matches before a trailing newline; `\\Z` must be used."""
    assert g1_id_format.ID_PATTERN.match("esp.a.b\n") is None
    assert g1_id_format.run(_ctx("esp.a.b\n")), "trailing newline must be rejected"


def test_finding_shape():
    findings = g1_id_format.run(_ctx("nope"))
    assert len(findings) == 1
    f = findings[0]
    assert f["rule_id"] == "g1.id_format"
    assert f["severity"] == "error"
    assert f["display_id"] == 1
    assert "does not match required regex" in f["message"]
