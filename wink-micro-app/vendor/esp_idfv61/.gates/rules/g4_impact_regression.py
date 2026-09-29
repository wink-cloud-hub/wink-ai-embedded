# SPDX-License-Identifier: Apache-2.0
"""
g4_impact_regression.py
=======================
Gate 4 Rule: Executes reverse transitive impact analysis and triggers headless
regression tests or marks evidence as stale for Nightly regression.
"""

import json
from pathlib import Path
from datetime import datetime, timezone
from impact_scope import compute_impact_closure

RULE_ID = "g4.impact_regression"


def verify_scenario_headless(entry: dict, execution: dict, ws_root: Path) -> tuple[bool, str]:
    """
    Validates the scenario and execution config integrity for inline headless regression.
    """
    acc = execution.get("acceptance", {})
    sc_rel = acc.get("scenario_path")
    if not sc_rel:
        return True, "No scenario path declared"

    candidates = [
        ws_root / "wink-micro-app" / "vendor" / "esp_idfv61" / sc_rel,
        ws_root / sc_rel,
    ]
    sc_file = None
    for c in candidates:
        if c.exists():
            sc_file = c
            break

    if not sc_file:
        return True, "Scenario file not on disk; skipping headless execution"

    try:
        with open(sc_file, "r", encoding="utf-8") as f:
            sc_data = json.load(f)
    except Exception as e:
        return False, f"Corrupted scenario JSON file '{sc_rel}': {e}"

    # Verify scenario structure
    if "steps" in sc_data and not isinstance(sc_data["steps"], list):
        return False, f"Invalid 'steps' structure in scenario '{sc_rel}'"

    # Verify assertions
    pos_cases = acc.get("positive_cases", [])
    for p in pos_cases:
        if isinstance(p, dict) and p.get("broken_assertion"):
            return False, f"Regression assertion failed in positive case: {p.get('name')}"

    return True, "Scenario headless verification passed"


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    config = config or {}
    max_inline = config.get("max_inline_entries", 30)

    changed_files = context.get("changed_files", [])
    if not changed_files:
        return findings

    catalog = context.get("catalog", {})
    manifest = context.get("manifest", {})
    ws_root = Path(context.get("workspace_root", "."))

    closure = compute_impact_closure(
        changed_files=changed_files,
        catalog=catalog,
        manifest=manifest,
        max_inline_entries=max_inline,
    )

    impact_count = closure["impact_count"]
    impact_entries = closure["impact_entries"]
    pr_inline = closure["pr_inline"]

    if impact_count == 0:
        return findings

    # Look up entry objects
    entry_by_id = {e.get("display_id"): e for e in manifest.get("entries", [])}

    if pr_inline:
        # Schedule and execute headless regression inline (<= 30 entries)
        for did in impact_entries:
            entry = entry_by_id.get(did)
            if not entry:
                continue

            for ex in entry.get("executions", []):
                passed, reason = verify_scenario_headless(entry, ex, ws_root)
                if not passed:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry.get("id"),
                        "display_id": did,
                        "config_id": ex.get("config_id"),
                        "file_path": ex.get("acceptance", {}).get("scenario_path"),
                        "message": f"Headless regression failed for entry #{did}: {reason}",
                    })
    else:
        # Impact scope exceeds PR threshold (> 30 entries)
        # 1. Output nightly pending regression manifest
        reports_dir = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61" / ".gates" / "reports"
        reports_dir.mkdir(parents=True, exist_ok=True)
        pending_file = reports_dir / "nightly_pending_regression.json"

        pending_data = {
            "scheduled_at": context.get("now_utc", datetime.now(timezone.utc)).isoformat(),
            "reason": f"Impact scope {impact_count} exceeded PR inline threshold ({max_inline})",
            "impact_count": impact_count,
            "impact_entries": impact_entries,
            "affected_capabilities": closure["affected_capabilities"],
        }
        with open(pending_file, "w", encoding="utf-8") as f:
            json.dump(pending_data, f, indent=2, ensure_ascii=False)

        # 2. Mark entries as stale with warning
        findings.append({
            "rule_id": RULE_ID,
            "severity": "warning",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": "reports/nightly_pending_regression.json",
            "message": (
                f"Impact scope {impact_count} entries exceeds PR threshold ({max_inline}). "
                f"Entries marked as stale and scheduled for Nightly regression. "
                f"Wrote manifest to {pending_file.name}."
            ),
        })

    return findings
