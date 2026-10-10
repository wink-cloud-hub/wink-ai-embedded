# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_version_alignment.py.

Restored alongside the gate engine: this rule was registered in gates.yaml and
shipped in __pycache__ as a .pyc, but its test source was missing.
"""

from gates.rules import g1_version_alignment as g

V = g.EXPECTED_SPEC_VERSION


def _ctx(root_version, entry_versions):
    return {"manifest": {
        "spec_version": root_version,
        "entries": [
            {"id": f"esp.a.b{n}", "display_id": n, "written_at_spec_version": v}
            for n, v in enumerate(entry_versions, 1)
        ],
    }}


def test_aligned_manifest_passes():
    assert g.run(_ctx(V, [V, V, V])) == []


def test_root_version_mismatch_is_reported():
    findings = g.run(_ctx("1.1.0", [V]))
    assert len(findings) == 1
    assert "Root spec_version" in findings[0]["message"]


def test_entry_version_mismatch_is_reported_per_entry():
    findings = g.run(_ctx(V, [V, "1.1.0", None, "2.1.0"]))
    assert len(findings) == 3, findings
    assert all(f["severity"] == "error" for f in findings)
    # Every offending entry is identified, not just the first.
    assert {f["display_id"] for f in findings} == {2, 3, 4}


def test_missing_root_version_is_reported():
    findings = g.run(_ctx(None, [V]))
    assert len(findings) == 1
    assert "None" in findings[0]["message"]


def test_spec_version_is_reported_from_manifest_not_hardcoded():
    """gate_context used to hardcode '2.0.0' in the context, hiding downgrades."""
    from gates import gate_context
    assert gate_context.validate_manifest_schema(
        {"spec_version": "2.1.0", "entries": [{"id": "x"}]}, "t") == "2.1.0"
