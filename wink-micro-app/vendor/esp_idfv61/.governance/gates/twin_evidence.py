# SPDX-License-Identifier: GPL-3.0-only
"""Read-only validation of an independently delivered, configuration-bound pair.

Candidate packages under runs/ never confer a checklist badge. This auxiliary
v1 format preserves the existing single-file scenario_sha256 registry contract.
"""
from __future__ import annotations

import hashlib
import json
import re
from pathlib import Path
from typing import Any

try:
    from gates.evidence_verifier import compute_assets_composite_sha256, resolve_execution_report_path
    from gates.report_contract import file_sha256, is_business_assertion, validate_scenario_report
except ImportError:
    from evidence_verifier import compute_assets_composite_sha256, resolve_execution_report_path
    from report_contract import file_sha256, is_business_assertion, validate_scenario_report


def contract_sha256(contract: dict[str, Any]) -> str:
    canonical = json.dumps(contract, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def is_fault_stimulus(step: dict[str, Any]) -> bool:
    """Recognize explicit supported fault operations, not names/descriptions."""
    step_type = step.get("type", "")
    if step_type == "INJECT_PLATFORM_FAULT":
        return bool(step.get("domain") and step.get("fault"))
    if isinstance(step_type, str) and step_type.startswith("INJECT_"):
        fault = step.get("fault")
        if isinstance(fault, dict) and fault.get("action"):
            return True
        routes = step.get("routes")
        if isinstance(routes, list) and any(
            isinstance(route, dict)
            and type(route.get("status_code")) is int
            and 400 <= route["status_code"] <= 599
            for route in routes
        ):
            return True
        requests = step.get("requests")
        if isinstance(requests, list) and any(
            isinstance(req, dict)
            and (
                bool(req.get("fault"))
                or (
                    isinstance(req.get("uri"), str)
                    and any(bad in req["uri"].lower() for bad in ("unknown", "invalid", "error", "fault", "bad", "not_found"))
                )
                or (
                    req.get("method") == "PUT"
                    and req.get("uri") == "/ctrl"
                    and str(req.get("body", "")).strip() in ("0", "stop", "disable", "deregister", "false")
                )
            )
            for req in requests
        ):
            return True
    return False


def verify_twin_evidence(
    entry: dict[str, Any], execution: dict[str, Any], ws_root: Path,
) -> tuple[bool, str]:
    """Require the declared fault cases and full passes for the same configuration."""
    vendor_root = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61"
    target_dir, config_id = entry.get("target_app_dir"), execution.get("config_id")
    if not isinstance(target_dir, str) or not isinstance(config_id, str) or not config_id:
        return False, "Application/configuration identity is missing"
    app_dir = (vendor_root / target_dir).resolve()
    formal_reports = (vendor_root / ".governance" / "reports").resolve()
    report_dir = (formal_reports / target_dir).resolve()
    config_dir = (report_dir / config_id).resolve()
    if not app_dir.is_relative_to(vendor_root.resolve()) or not report_dir.is_relative_to(formal_reports) or config_dir.parent != report_dir:
        return False, "Application/configuration path is invalid"
    proof_path = config_dir / "twin-proof.json"
    if not proof_path.is_file():
        return False, "No delivered configuration-bound twin proof"
    try:
        proof = json.loads(proof_path.read_text(encoding="utf-8"))
        if not isinstance(proof, dict):
            return False, "Twin proof must be an object"
        evidence = execution.get("evidence") or {}
        audit = entry.get("audit") or {}
        acceptance = execution.get("acceptance") or {}
        if not all(isinstance(value, dict) for value in (evidence, audit, acceptance)):
            return False, "Evidence/audit/acceptance must be objects"
        if execution.get("delivery_state") != "verified" or audit.get("verdict") != "audited":
            return False, "Twin proof requires verified delivery and existing audit coverage"
        audited_configs, auditor = audit.get("audited_configs"), audit.get("auditor")
        if not isinstance(audited_configs, list) or config_id not in audited_configs or not isinstance(auditor, str) or not auditor.strip() or auditor == "loop_sop_daemon":
            return False, "Independent audit does not cover this configuration"
        if acceptance.get("type") != "wasm_simulation" or any(not isinstance(execution.get(key), str) or not execution[key] for key in ("backend", "target_soc", "profile")):
            return False, "Wasm execution configuration is incomplete"
        identity = {
            "format_version": 1, "kind": "twin_proof", "app_id": entry.get("id"),
            "config_id": config_id,
            **{key: execution[key] for key in ("backend", "target_soc", "profile")},
            "baseline_run_id": evidence.get("run_id"), "assets_sha256": evidence.get("assets_sha256"),
        }
        if not identity["baseline_run_id"] or not identity["assets_sha256"]:
            return False, "Positive evidence lacks a run/asset binding"
        if type(proof.get("format_version")) is not int or any(proof.get(key) != value for key, value in identity.items()):
            return False, "Twin proof application/configuration/run/assets do not match"
        if proof["assets_sha256"] != compute_assets_composite_sha256(app_dir / "unisim-assets"):
            return False, "Twin proof assets no longer match the current application"
        tree = json.loads((app_dir / "unisim-assets" / "device-tree.json").read_text(encoding="utf-8"))
        if not isinstance(tree, dict) or tree.get("mcu") != execution["target_soc"]:
            return False, "Twin proof assets have a different target_soc"

        def check_record(record: dict[str, Any]) -> tuple[Path, Path]:
            if not isinstance(record, dict):
                raise ValueError("Scenario/report binding must be an object")
            if not isinstance(record.get("run_id"), str) or not record["run_id"]:
                raise ValueError("Scenario/report record has no run identity")
            if any(record.get(key) != proof[key] for key in ("config_id", "backend", "target_soc", "profile", "assets_sha256")):
                raise ValueError("Scenario/report record belongs to another configuration or asset set")
            scenario = (vendor_root / record["scenario_ref"]).resolve()
            report = (vendor_root / record["report_ref"]).resolve()
            if not scenario.is_relative_to(app_dir / "unisim-scenarios"):
                raise ValueError("Scenario is outside this application")
            if not report.is_relative_to(report_dir):
                raise ValueError("Report is not delivered in this application's formal report directory")
            if file_sha256(scenario) != record["scenario_sha256"] or file_sha256(report) != record["report_sha256"]:
                raise ValueError("Scenario/report content hash mismatch")
            ok, reason = validate_scenario_report(report, scenario)
            if not ok:
                raise ValueError(reason)
            return scenario, report

        positive_scenario, positive_report = check_record(proof.get("positive"))
        if proof["positive"]["run_id"] != proof["baseline_run_id"]:
            return False, "Positive record run identity differs from the registered baseline"
        declared_scenario = (vendor_root / acceptance["scenario_path"]).resolve()
        declared_report = resolve_execution_report_path(evidence.get("execution_report_ref", ""), ws_root)
        if positive_scenario != declared_scenario or declared_report is None or positive_report != declared_report.resolve():
            return False, "Positive twin record differs from the registered acceptance evidence"
        if proof["positive"]["scenario_sha256"] != evidence.get("scenario_sha256"):
            return False, "Positive twin scenario hash differs from the registered evidence"

        declared_cases = acceptance.get("negative_cases")
        negative_records = proof.get("negative")
        if not isinstance(declared_cases, list) or not declared_cases:
            return False, "No declared fault-handling contracts"
        if not isinstance(negative_records, list) or len(negative_records) != len(declared_cases):
            return False, "Twin proof must cover every declared fault-handling case"
        seen_scenarios = {positive_scenario}
        seen_reports = {positive_report}
        seen_run_ids = {proof["baseline_run_id"]}
        seen_report_hashes = {proof["positive"]["report_sha256"]}
        for index, (case, record) in enumerate(zip(declared_cases, negative_records)):
            if not isinstance(case, dict) or any(not case.get(key) for key in ("stimulus", "expect_error", "detects")):
                return False, "Fault-handling contract is incomplete"
            if not isinstance(record, dict) or type(record.get("case_index")) is not int or record["case_index"] != index or record.get("contract_sha256") != contract_sha256(case):
                return False, "Fault-handling case identity differs from the declaration"
            scenario, report = check_record(record)
            if scenario in seen_scenarios or report in seen_reports or record["run_id"] in seen_run_ids or record["report_sha256"] in seen_report_hashes:
                return False, "Fault cases must have distinct scenarios and independent reports"
            seen_scenarios.add(scenario)
            seen_reports.add(report)
            seen_run_ids.add(record["run_id"])
            seen_report_hashes.add(record["report_sha256"])
            steps = json.loads(scenario.read_text(encoding="utf-8"))["steps"]
            stimulus_index, assertion_index = record.get("stimulus_step_index"), record.get("assertion_step_index")
            if type(stimulus_index) is not int or type(assertion_index) is not int or not 0 <= stimulus_index < assertion_index < len(steps):
                return False, "Fault stimulus and subsequent error assertion are not identified"
            if not is_fault_stimulus(steps[stimulus_index]) or not is_business_assertion(steps[assertion_index]):
                return False, "Fault proof has no explicit fault stimulus and business assertion"
            expected_error = case["expect_error"]
            expected = json.dumps(steps[assertion_index].get("matcher"), ensure_ascii=False)
            observed_steps = json.loads(report.read_text(encoding="utf-8"))["results"][0]["stepResults"]
            actual = json.dumps(observed_steps[assertion_index]["actual"], ensure_ascii=False)
            if not isinstance(expected_error, str) or not expected_error:
                return False, "Declared error must be an explicit symbolic observation"
            error_token = r"(?<![A-Za-z0-9_])" + re.escape(expected_error) + r"(?![A-Za-z0-9_])"
            if not re.search(error_token, expected) or not re.search(error_token, actual):
                return False, "Declared error was not asserted and observed by the firmware outcome"
    except (OSError, ValueError, KeyError, TypeError, IndexError) as exc:
        return False, f"Invalid twin proof: {exc}"
    return True, "Same configuration has complete positive and fault-handling evidence"
