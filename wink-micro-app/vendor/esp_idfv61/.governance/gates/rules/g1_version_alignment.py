# SPDX-License-Identifier: Apache-2.0
"""
g1_version_alignment.py
=======================
Gate 1 Rule: Verifies that root spec_version and all entries' written_at_spec_version equal '2.0.0'.
"""

RULE_ID = "g1.version_alignment"
EXPECTED_SPEC_VERSION = "2.0.0"


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    root_version = context["manifest"].get("spec_version")

    if root_version != EXPECTED_SPEC_VERSION:
        findings.append({
            "rule_id": RULE_ID,
            "severity": "error",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": None,
            "message": f"Root spec_version '{root_version}' does not match expected '{EXPECTED_SPEC_VERSION}'",
        })

    for entry in context["manifest"].get("entries", []):
        display_id = entry.get("display_id")
        entry_id = entry.get("id")
        entry_version = entry.get("written_at_spec_version")

        if entry_version != EXPECTED_SPEC_VERSION:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": entry_id,
                "display_id": display_id,
                "config_id": None,
                "file_path": None,
                "message": f"Entry #{display_id} ({entry_id}) written_at_spec_version is '{entry_version}' (expected '{EXPECTED_SPEC_VERSION}')",
            })

    return findings
