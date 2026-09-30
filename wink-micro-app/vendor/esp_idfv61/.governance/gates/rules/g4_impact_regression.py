# SPDX-License-Identifier: Apache-2.0
"""
g4_impact_regression.py
=======================
Gate 4 Rule: Computes the reverse transitive impact closure over the capability
graph and structurally validates the scenarios of impacted examples.

Scope honesty: this rule does NOT execute simulations. It performs static
structural validation only. Real behavioural regression requires the UniSim
headless evidence runner and is a separate concern.
"""

import sys
import json
from pathlib import Path
from datetime import datetime, timezone
from impact_scope import compute_impact_closure

try:
    from evidence_verifier import verify_evidence
except ImportError:
    GATES_DIR = Path(__file__).resolve().parent.parent
    if str(GATES_DIR) not in sys.path:
        sys.path.insert(0, str(GATES_DIR))
    from evidence_verifier import verify_evidence

RULE_ID = "g4.impact_regression"


def validate_scenario_manifest(entry: dict, execution: dict, ws_root: Path) -> tuple[bool, str]:
    """Structurally validates the scenario declared by an execution config.

    NOTE: this performs *no* simulation. It does not launch a Wasm runtime, Node,
    or any headless runner. Its only failure trigger is a ``broken_assertion``
    flag authored in the same manifest the rule reads, so it is a self-consistency
    check, not a regression.

    Actually executing the scenario requires the UniSim headless evidence runner
    (run_esp32_headless_evidence.ps1) and is deliberately out of scope for this
    pure-Python gate. Keep the naming honest so the rule is not mistaken for an
    executed regression.
    """
    acc = execution.get("acceptance") or {}
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
        # Fail-closed, consistent with g1_can_check_mark: once an execution claims
        # to have been built, a scenario that is declared but absent from the
        # workspace is a broken declaration, not a reason to report a clean pass.
        return False, (
            f"Scenario file declared at '{sc_rel}' is not on disk. "
            f"A declared-but-missing scenario must not be reported as a pass."
        )

    try:
        with open(sc_file, "r", encoding="utf-8") as f:
            sc_data = json.load(f)
    except Exception as e:
        return False, f"Corrupted scenario JSON file '{sc_rel}': {e}"

    if not isinstance(sc_data, dict):
        return False, f"Invalid scenario '{sc_rel}': root must be a JSON object"

    # Verify scenario structure
    if "steps" in sc_data and not isinstance(sc_data["steps"], list):
        return False, f"Invalid 'steps' structure in scenario '{sc_rel}'"

    # Verify assertions
    pos_cases = acc.get("positive_cases") or []
    for p in pos_cases:
        if isinstance(p, dict) and p.get("broken_assertion"):
            return False, f"Regression assertion flagged broken in positive case: {p.get('name')}"

    return True, "Scenario manifest structurally valid (not executed)"


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

    # 1. Check for unmapped unknown code files (Fail-Closed)
    for unk in closure.get("unknown_paths", []):
        findings.append({
            "rule_id": RULE_ID,
            "severity": "error",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": unk,
            "message": (
                f"FAIL_ON_UNKNOWN_PATH: Modified code path '{unk}' is not mapped in "
                f"capability-catalog.yaml, target_app_dir, or GLOBAL_IMPACT_PATHS. "
                f"Fail-closed: all ESP-IDF facade changes must be mapped or registered."
            ),
        })

    impact_count = closure["impact_count"]
    impact_entries = closure["impact_entries"]
    pr_inline = closure["pr_inline"]

    if impact_count == 0:
        return findings

    # Look up entry objects
    entry_by_id = {e.get("display_id"): e for e in manifest.get("entries", [])}

    if pr_inline:
        # Inline structural validation of impacted scenarios (<= max_inline entries).
        for did in impact_entries:
            entry = entry_by_id.get(did)
            if not entry:
                continue

            for ex in entry.get("executions", []):
                # A scenario path on a `planned` execution is a *planned declaration*,
                # not a claim that the artifact exists.
                if ex.get("delivery_state", "planned") == "planned":
                    continue
                passed, reason = validate_scenario_manifest(entry, ex, ws_root)
                if not passed:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry.get("id"),
                        "display_id": did,
                        "config_id": ex.get("config_id"),
                        "file_path": (ex.get("acceptance") or {}).get("scenario_path"),
                        "message": f"Scenario validation failed for entry #{did}: {reason}",
                    })
                elif ex.get("delivery_state") == "verified":
                    # Validate real evidence only if scenario passed and delivery_state == 'verified'
                    ev_ok, ev_errors = verify_evidence(entry, ex, ws_root, strict_disk=True)
                    if not ev_ok:
                        for err in ev_errors:
                            findings.append({
                                "rule_id": RULE_ID,
                                "severity": "error",
                                "entry_id": entry.get("id"),
                                "display_id": did,
                                "config_id": ex.get("config_id"),
                                "file_path": None,
                                "message": f"Regression evidence verification failed for entry #{did}: {err}",
                            })
    else:
        # Impact scope exceeds the PR inline threshold.
        reports_dir = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance" / "gates" / "reports"
        reports_dir.mkdir(parents=True, exist_ok=True)
        pending_file = reports_dir / "nightly_pending_regression.json"

        pending_data = {
            "scheduled_at": context.get("now_utc", datetime.now(timezone.utc)).isoformat(),
            "reason": f"Impact scope {impact_count} exceeded PR inline threshold ({max_inline})",
            "impact_count": impact_count,
            "impact_entries": impact_entries,
            "affected_capabilities": closure["affected_capabilities"],
            "consumed_by": None,
            "note": "Triage artifact only. No CI job consumes this file automatically; "
                    "run the UniSim headless evidence runner for these entries before merge.",
        }
        with open(pending_file, "w", encoding="utf-8") as f:
            json.dump(pending_data, f, indent=2, ensure_ascii=False)

        # In PR mode or when fail_on_overflow is enabled, overflow is an ERROR (Fail-Closed).
        # In nightly or triage mode, it produces a warning.
        is_pr_mode = (context.get("mode") == "pr") or config.get("fail_on_overflow", False)
        severity = "error" if is_pr_mode else "warning"

        findings.append({
            "rule_id": RULE_ID,
            "severity": severity,
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": "reports/nightly_pending_regression.json",
            "message": (
                f"Impact scope {impact_count} entries exceeds PR threshold ({max_inline}); "
                f"{impact_count} impacted entries were NOT regression-tested by this gate. "
                + (
                    f"FAIL_ON_OVERFLOW: In PR mode, large-scale changes exceeding inline verification "
                    f"capacity must be verified with UniSim headless runner or scoped down (Fail-Closed)."
                    if is_pr_mode else
                    f"No CI job consumes the pending list automatically -- run the UniSim headless "
                    f"evidence runner for {pending_file.name} before merging. "
                    f"This gate only performs structural scenario validation, never execution."
                )
            ),
        })

    return findings
