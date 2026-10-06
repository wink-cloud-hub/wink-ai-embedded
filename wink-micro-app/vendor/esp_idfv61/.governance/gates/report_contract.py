# SPDX-License-Identifier: GPL-3.0-only
"""Validate one explicitly selected scenario against its evaluated step results."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any


def file_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def is_business_assertion(step: dict[str, Any]) -> bool:
    return (
        isinstance(step, dict)
        and isinstance(step.get("type"), str)
        and step["type"].startswith("ASSERT_")
        and not str(step.get("target", "")).startswith("power:")
    )


def same_json_value(left: Any, right: Any) -> bool:
    """Compare JSON matchers without Python treating booleans as integers."""
    if isinstance(left, bool) or isinstance(right, bool):
        return type(left) is type(right) and left == right
    if isinstance(left, dict) and isinstance(right, dict):
        return left.keys() == right.keys() and all(same_json_value(left[key], right[key]) for key in left)
    if isinstance(left, list) and isinstance(right, list):
        return len(left) == len(right) and all(same_json_value(a, b) for a, b in zip(left, right))
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        return left == right
    return type(left) is type(right) and left == right


def validate_scenario_report(
    report_path: Path,
    scenario_path: Path,
    failure_index: int | None = None,
) -> tuple[bool, str]:
    """Accept a full pass or exactly the specified, evaluated assertion failure.

    The public headless JSON report must contain exactly one scenario. A failed
    fail-fast run may contain an executed prefix or explicit trailing skips.
    Native reports may retain the complete planned suffix as pending after the
    specified failure; those steps are not evaluated or counted as skipped.
    Console logs, unknown signals, and incomplete/contradictory counts are not
    substitutes for evaluated results. File hashes and run identity are bound
    separately by the collector or evidence package.
    """
    try:
        scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        return False, f"Cannot read scenario/report: {exc}"
    if not isinstance(scenario, dict) or not isinstance(report, dict):
        return False, "Scenario/report must be JSON objects"
    steps = scenario.get("steps")
    if not isinstance(steps, list) or not steps or not all(isinstance(s, dict) for s in steps):
        return False, "Scenario must have a non-empty step array"
    if not any(is_business_assertion(s) for s in steps):
        return False, "Scenario has no business assertion"
    if failure_index is not None and (
        type(failure_index) is not int
        or not 0 <= failure_index < len(steps)
        or not is_business_assertion(steps[failure_index])
    ):
        return False, "Expected failure must identify a business assertion"

    results = report.get("results")
    if not isinstance(results, list) or len(results) != 1 or not isinstance(results[0], dict):
        return False, "Expected exactly one scenario result"
    result = results[0]
    expected_status = "passed" if failure_index is None else "failed"
    if result.get("status") != expected_status or result.get("ok") is not (failure_index is None):
        return False, f"Scenario result must be {expected_status}"
    expected_counts = {"total": 1, "passed": int(failure_index is None), "failed": int(failure_index is not None)}
    for key, value in expected_counts.items():
        if type(report.get(key)) is not int or report[key] != value:
            return False, f"Report {key} does not match the selected scenario outcome"

    header = scenario.get("header")
    observed_header = result.get("header")
    if not isinstance(header, dict) or not isinstance(observed_header, dict):
        return False, "Scenario identity header is missing"
    for key in ("name", "templateId"):
        if not isinstance(header.get(key), str) or not header[key] or observed_header.get(key) != header[key]:
            return False, f"Report scenario {key} does not match the selected input"

    summary = result.get("summary")
    step_results = result.get("stepResults")
    if not isinstance(summary, dict) or not isinstance(step_results, list) or not step_results:
        return False, "Structured summary and stepResults are required"
    fields = ("totalSteps", "passedSteps", "failedSteps", "errorSteps", "skippedSteps")
    if any(type(summary.get(key)) is not int or summary[key] < 0 for key in fields):
        return False, "Summary step counts must all be present and non-negative integers"
    if len(step_results) > len(steps):
        return False, "Report contains more steps than the selected scenario"
    if failure_index is None and len(step_results) != len(steps):
        return False, "Passing report does not contain the complete step set"
    if failure_index is not None and len(step_results) <= failure_index:
        return False, "Specified failure assertion was not executed"

    pending_suffix = (
        failure_index is not None
        and header.get("failurePolicy") == "fail-fast"
        and len(step_results) == len(steps)
        and failure_index + 1 < len(steps)
        and all(isinstance(item, dict) and item.get("status") == "pending"
                for item in step_results[failure_index + 1:])
    )
    observed_counts = {"passed": 0, "failed": 0, "error": 0, "skipped": 0, "pending": 0}
    for index, observed in enumerate(step_results):
        if not isinstance(observed, dict) or type(observed.get("stepIndex")) is not int or observed["stepIndex"] != index:
            return False, "Step indices must be unique and form the ordered executed prefix"
        step = steps[index]
        if observed.get("type") != step.get("type"):
            return False, f"Step #{index} type differs from the selected scenario"
        if "target" in observed and observed["target"] != step.get("target"):
            return False, f"Step #{index} target differs from the selected scenario"
        if failure_index is None or index < failure_index:
            required_status = "passed"
        elif index == failure_index:
            required_status = "failed"
        else:
            required_status = "pending" if pending_suffix else "skipped"
        if observed.get("status") != required_status:
            return False, f"Step #{index} must be {required_status}"
        observed_counts[required_status] += 1
        if required_status == "pending" and ("actual" in observed or "expected" in observed):
            return False, f"Step #{index} is pending but carries an evaluated observation"
        if is_business_assertion(step) and required_status not in ("skipped", "pending"):
            if "matcher" not in step or "expected" not in observed or not same_json_value(observed["expected"], step["matcher"]):
                return False, f"Step #{index} expected value is not bound to the selected matcher"
            if "actual" not in observed or observed["actual"] is None:
                return False, f"Step #{index} has no evaluated business observation"

    for status, field in (("passed", "passedSteps"), ("failed", "failedSteps"), ("error", "errorSteps"), ("skipped", "skippedSteps")):
        if summary[field] != observed_counts[status]:
            return False, f"Summary {field} contradicts the step results"
    # Runners may count all planned steps or only the executed fail-fast prefix.
    if summary["totalSteps"] not in (len(steps), len(step_results)):
        return False, "Summary totalSteps does not match the scenario or executed prefix"

    diagnostics = result.get("diagnostics")
    if not isinstance(diagnostics, list) or not all(isinstance(d, dict) for d in diagnostics):
        return False, "Structured diagnostics are required"
    for diagnostic in diagnostics:
        code = str(diagnostic.get("code", diagnostic.get("source", ""))).upper()
        category = str(diagnostic.get("category", "")).lower()
        if diagnostic.get("level") == "error" or category in ("infrastructure", "schema", "loader", "runner") or any(
            marker in code for marker in ("UNKNOWN_", "UNSUPPORTED_", "LOAD_FAILED", "SCHEMA_", "RUNNER_", "RUNTIME_ERROR")
        ):
            return False, f"Infrastructure diagnostic cannot prove a business outcome: {code}"
    return True, f"Selected scenario {expected_status} with matching evaluated steps"
