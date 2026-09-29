# SPDX-License-Identifier: Apache-2.0
"""
g1_id_format.py
===============
Gate 1 Rule: Verifies that entry ID conforms to the stable identifier regex ^esp\\.[a-z0-9_]+(\\.[a-z0-9_]+)+$
"""

import re

RULE_ID = "g1.id_format"
# `\Z` rather than `$`: Python's `$` also matches immediately before a trailing
# newline, so "esp.a.b\n" would otherwise pass the stable-ID check.
ID_PATTERN = re.compile(r"^esp\.[a-z0-9_]+(\.[a-z0-9_]+)+\Z")


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []

    for entry in context["manifest"].get("entries", []):
        display_id = entry.get("display_id")
        entry_id = entry.get("id", "")

        if not entry_id or not ID_PATTERN.match(entry_id):
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": entry_id,
                "display_id": display_id,
                "config_id": None,
                "file_path": None,
                "message": f"Entry ID '{entry_id}' does not match required regex pattern '^esp\\.[a-z0-9_]+(\\.[a-z0-9_]+)+$'",
            })

    return findings
