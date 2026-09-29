# SPDX-License-Identifier: Apache-2.0
"""
g1_path_unique.py
=================
Gate 1 Rule: Verifies that upstream_path is globally unique across all entries.
"""

RULE_ID = "g1.path_unique"


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    seen_paths = {}

    for entry in context["manifest"].get("entries", []):
        display_id = entry.get("display_id")
        entry_id = entry.get("id")
        path = entry.get("upstream_path")

        if not path:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": entry_id,
                "display_id": display_id,
                "config_id": None,
                "file_path": None,
                "message": f"Entry #{display_id} ({entry_id}) is missing 'upstream_path'",
            })
            continue

        if path in seen_paths:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": entry_id,
                "display_id": display_id,
                "config_id": None,
                "file_path": None,
                "message": f"Duplicate upstream_path '{path}' previously registered by entry #{seen_paths[path]}",
            })
        else:
            seen_paths[path] = display_id

    return findings
