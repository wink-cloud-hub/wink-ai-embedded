# SPDX-License-Identifier: GPL-3.0-only
"""
Unit tests for Task T1.4: Shared Acceptance Policy and Write Ingress Convergence.
=================================================================================
Validates:
1. Unified acceptance policy across validate_scenario_report and verify_execution_report:
   - Borrowed scenario reports from different applications rejected 100%.
   - Step count discrepancies rejected 100%.
   - Scenarios lacking business assertions rejected 100%.
   - Unevaluated observations (missing actual / None) rejected 100%.
   - Infrastructure diagnostics disguised as business passes rejected 100%.
2. Error domain taxonomy & sign polarity (ADR-0001, ADR-0012, AFG Engine 2.2):
   - Strict rejection of vague matchers (!= 0, not_zero, negative without domain).
   - Domain sign polarity isolation across esp_err (+), wink_status (-), posix_errno (+), nimble_hs (+).
3. Anti-premature-overwrite write ingress protection:
   - Failing reports strictly prohibited from overwriting historical baseline reports.
   - Manifest strictly protected against fake/unverified promotion.
   - Valid reports atomically replace destinations without corruption.
"""
from __future__ import annotations

import copy
import json
import os
from pathlib import Path
from typing import Any, Dict

import pytest

from gates.report_contract import (
    validate_scenario_report,
    validate_report_standalone,
    is_business_assertion,
    file_sha256,
)
from gates.evidence_verifier import (
    verify_execution_report,
    verify_evidence,
    write_evidence_for_app,
)
from loop.afg.error_matcher import (
    is_error_matcher,
    validate_no_vague_matcher,
    match_error_assertion,
)


def create_scenario(name: str = "uart_echo", steps: list[dict[str, Any]] | None = None) -> dict[str, Any]:
    if steps is None:
        steps = [
            {
                "type": "ASSERT_POINT",
                "timeUs": "10ms",
                "target": "power:VCC_3V3",
                "matcher": 3.3,
                "description": "Power rail",
            },
            {
                "type": "ASSERT_BUS_PAYLOAD",
                "timeUs": "50ms",
                "target": "uart:0",
                "busType": "uart",
                "busId": 0,
                "direction": "tx",
                "matcher": "hello",
                "description": "Echo payload",
            },
        ]
    return {
        "header": {
            "version": "1.0.0",
            "name": name,
            "templateId": name,
            "accuracyMode": "behavioral",
            "timeoutUs": "1000000",
            "failurePolicy": "fail-fast",
        },
        "steps": steps,
    }


def create_report(scenario_dict: dict[str, Any], failure_index: int | None = None) -> dict[str, Any]:
    steps = scenario_dict["steps"]
    step_results = []
    for idx, step in enumerate(steps):
        if failure_index is None or idx < failure_index:
            st = "passed"
        elif idx == failure_index:
            st = "failed"
        else:
            st = "pending"
        res = {
            "stepIndex": idx,
            "type": step["type"],
            "status": st,
            "timeUs": 10000 + idx * 20000,
        }
        if "target" in step:
            res["target"] = step["target"]
        if is_business_assertion(step) and st != "pending":
            res["expected"] = step.get("matcher")
            res["actual"] = "corrupted" if st == "failed" else step.get("matcher")
        step_results.append(res)

    is_passed = failure_index is None
    return {
        "total": 1,
        "passed": int(is_passed),
        "failed": int(not is_passed),
        "results": [
            {
                "ok": is_passed,
                "status": "passed" if is_passed else "failed",
                "header": copy.deepcopy(scenario_dict["header"]),
                "summary": {
                    "totalSteps": len(step_results),
                    "passedSteps": sum(1 for s in step_results if s["status"] == "passed"),
                    "failedSteps": sum(1 for s in step_results if s["status"] == "failed"),
                    "errorSteps": 0,
                    "skippedSteps": 0,
                },
                "stepResults": step_results,
                "diagnostics": [],
            }
        ],
    }


# =========================================================================
# 1. Unified acceptance policy across report_contract and evidence_verifier
# =========================================================================

def test_borrowed_report_rejected_by_all_entrypoints(tmp_path: Path):
    """Borrowed reports with swapped name/templateId must be rejected by all entrypoints."""
    scen_a = create_scenario("app_alpha")
    scen_b = create_scenario("app_beta")

    scen_a_file = tmp_path / "app_alpha.scenario.json"
    scen_a_file.write_text(json.dumps(scen_a), encoding="utf-8")

    # Generate valid report for app_beta
    rep_b = create_report(scen_b)
    rep_b_file = tmp_path / "app_beta_report.json"
    rep_b_file.write_text(json.dumps(rep_b), encoding="utf-8")

    # Ingress 1: report_contract.validate_scenario_report
    ok1, msg1 = validate_scenario_report(rep_b_file, scen_a_file)
    assert not ok1
    assert "does not match the selected input" in msg1

    # Ingress 2: evidence_verifier.verify_execution_report with scenario
    ok2, msg2 = verify_execution_report(rep_b_file, scenario_path=scen_a_file)
    assert not ok2
    assert "does not match the selected input" in msg2


def test_step_count_mismatch_rejected_by_all_entrypoints(tmp_path: Path):
    """Reports with mismatched step counts must be rejected at all gates."""
    scen = create_scenario("test_steps")
    scen_file = tmp_path / "test.scenario.json"
    scen_file.write_text(json.dumps(scen), encoding="utf-8")

    rep = create_report(scen)
    # Remove one step from report
    rep["results"][0]["stepResults"] = rep["results"][0]["stepResults"][:1]
    rep["results"][0]["summary"]["totalSteps"] = 1
    rep["results"][0]["summary"]["passedSteps"] = 1

    rep_file = tmp_path / "mismatch_rep.json"
    rep_file.write_text(json.dumps(rep), encoding="utf-8")

    ok1, msg1 = validate_scenario_report(rep_file, scen_file)
    assert not ok1
    assert "complete step set" in msg1 or "mismatch" in msg1

    ok2, msg2 = verify_execution_report(rep_file, scenario_path=scen_file)
    assert not ok2
    assert "totalSteps" in msg2 or "step set" in msg2


def test_missing_business_assertion_rejected(tmp_path: Path):
    """Scenario lacking business assertions cannot produce qualifying evidence."""
    # Only power rail checks, zero business logic
    scen = create_scenario("no_biz", steps=[
        {"type": "ASSERT_POINT", "target": "power:VCC_3V3", "matcher": 3.3},
        {"type": "ASSERT_POINT", "target": "power:VCC_5V", "matcher": 5.0},
    ])
    scen_file = tmp_path / "no_biz.scenario.json"
    scen_file.write_text(json.dumps(scen), encoding="utf-8")

    rep = create_report(scen)
    rep_file = tmp_path / "no_biz_rep.json"
    rep_file.write_text(json.dumps(rep), encoding="utf-8")

    ok, msg = validate_scenario_report(rep_file, scen_file)
    assert not ok
    assert "no business assertion" in msg


def test_missing_evaluated_observation_rejected(tmp_path: Path):
    """Assertion steps with null or missing actual observation must be rejected."""
    scen = create_scenario("obs_check")
    scen_file = tmp_path / "obs.scenario.json"
    scen_file.write_text(json.dumps(scen), encoding="utf-8")

    rep = create_report(scen)
    # Remove actual observation from business assertion step
    rep["results"][0]["stepResults"][1]["actual"] = None

    rep_file = tmp_path / "obs_rep.json"
    rep_file.write_text(json.dumps(rep), encoding="utf-8")

    ok, msg = validate_scenario_report(rep_file, scen_file)
    assert not ok
    assert "no evaluated business observation" in msg


def test_infrastructure_error_diagnostic_rejected(tmp_path: Path):
    """Infrastructure failure diagnostics disguised inside a passing report must be rejected."""
    scen = create_scenario("infra_diag")
    scen_file = tmp_path / "infra.scenario.json"
    scen_file.write_text(json.dumps(scen), encoding="utf-8")

    rep = create_report(scen)
    rep["results"][0]["diagnostics"] = [
        {"level": "error", "source": "RUNNER_INTERNAL_CRASH", "message": "Host buffer overrun"}
    ]
    rep_file = tmp_path / "infra_rep.json"
    rep_file.write_text(json.dumps(rep), encoding="utf-8")

    ok, msg = validate_scenario_report(rep_file, scen_file)
    assert not ok
    assert "Infrastructure diagnostic cannot prove a business outcome" in msg


# =========================================================================
# 2. Error domain taxonomy & sign polarity (ADR-0001, ADR-0012, AFG 2.2)
# =========================================================================

def test_vague_matcher_strictly_rejected():
    """Detect and reject ambiguous status!=0 or <0 matchers without domain."""
    vague_not_zero = {"op": "!=", "value": 0}
    ok, reason = validate_no_vague_matcher(vague_not_zero)
    assert not ok
    assert "[VAGUE_MATCHER_REJECTED]" in reason

    vague_type = {"matcher_type": "not_zero"}
    ok, reason = validate_no_vague_matcher(vague_type)
    assert not ok
    assert "[VAGUE_MATCHER_REJECTED]" in reason

    vague_neg = {"matcher_type": "negative"}
    ok, reason = validate_no_vague_matcher(vague_neg)
    assert not ok
    assert "[VAGUE_MATCHER_REJECTED]" in reason

    # With explicit domain, typed matcher is accepted
    typed_wink = {"domain": "wink_status", "symbol": "WINK_ERR_TIMEOUT"}
    ok, reason = validate_no_vague_matcher(typed_wink)
    assert ok


def test_esp_err_domain_sign_polarity():
    """esp_err domain: positive error codes allowed, negative codes (< -1) rejected."""
    matcher = {"domain": "esp_err", "symbol": "ESP_ERR_TIMEOUT"}  # 0x107 = 263
    
    # Correct positive observation matches
    ok, _ = match_error_assertion(matcher, 263)
    assert ok

    # Negative observation (e.g. -2) in esp_err domain must be rejected
    ok, reason = match_error_assertion(matcher, -2)
    assert not ok
    assert "ERROR_DOMAIN_MISMATCH" in reason


def test_wink_status_domain_sign_polarity():
    """wink_status domain (ADR-0001): negative error codes, positive codes rejected."""
    matcher = {"domain": "wink_status", "symbol": "WINK_ERR_TIMEOUT"}  # -2

    # Correct negative observation matches
    ok, _ = match_error_assertion(matcher, -2)
    assert ok

    # Positive observation (e.g. 263) in wink_status domain must be rejected
    ok, reason = match_error_assertion(matcher, 263)
    assert not ok
    assert "ERROR_DOMAIN_MISMATCH" in reason


def test_posix_errno_domain_sign_polarity():
    """posix_errno domain: positive errno allowed, negative values rejected."""
    matcher = {"domain": "posix_errno", "symbol": "ETIMEDOUT"}  # 110

    ok, _ = match_error_assertion(matcher, 110)
    assert ok

    ok, reason = match_error_assertion(matcher, -1)
    assert not ok
    assert "ERROR_DOMAIN_MISMATCH" in reason


def test_nimble_hs_domain_sign_polarity():
    """nimble_hs domain: positive status allowed, negative rejected."""
    matcher = {"domain": "nimble_hs", "symbol": "BLE_HS_ETIMEOUT"}  # 5

    ok, _ = match_error_assertion(matcher, 5)
    assert ok

    ok, reason = match_error_assertion(matcher, -5)
    assert not ok
    assert "ERROR_DOMAIN_MISMATCH" in reason


def test_invalid_domain_rejected():
    """Unknown or uncontracted domains are strictly rejected."""
    matcher = {"domain": "unknown_crypto_domain", "symbol": "ERR_UNKNOWN"}
    ok, reason = match_error_assertion(matcher, 1)
    assert not ok
    assert "INVALID_DOMAIN" in reason


# =========================================================================
# 3. Anti-premature-overwrite write ingress protection
# =========================================================================

def test_anti_premature_overwrite_on_verification_failure(tmp_path: Path):
    """A failing candidate report MUST NOT overwrite the existing historical report."""
    vendor_root = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61"
    gov_data = vendor_root / ".governance" / "data"
    gov_data.mkdir(parents=True)
    app_dir = vendor_root / "peripherals" / "uart_test"
    app_dir.mkdir(parents=True)

    # Assets
    assets_dir = app_dir / "unisim-assets"
    assets_dir.mkdir()
    (assets_dir / "wink_simulator.wasm").write_bytes(b"\x00asm\x01\x00\x00\x00")
    (assets_dir / "wink_simulator.js").write_text("console.log('sim');", encoding="utf-8")
    (assets_dir / "device-tree.json").write_text('{"name": "uart"}', encoding="utf-8")

    # Scenario
    scen = create_scenario("uart_test")
    scen_dir = app_dir / "unisim-scenarios"
    scen_dir.mkdir()
    scen_file = scen_dir / "uart_test.scenario.json"
    scen_file.write_text(json.dumps(scen), encoding="utf-8")

    # Initial valid historical report on disk
    reports_dir = vendor_root / ".governance" / "reports" / "peripherals" / "uart_test"
    reports_dir.mkdir(parents=True)
    hist_report_file = reports_dir / "run-report.json"
    valid_report = create_report(scen)
    hist_report_file.write_text(json.dumps(valid_report), encoding="utf-8")
    original_sha = file_sha256(hist_report_file)

    # Manifest with planned state
    manifest = {
        "entries": [
            {
                "id": "esp.peripherals.uart_test",
                "target_app_dir": "peripherals/uart_test",
                "executions": [
                    {"config_id": "wasm_sim_standard", "delivery_state": "planned"}
                ],
            }
        ]
    }
    (gov_data / "checklist.data.json").write_text(json.dumps(manifest), encoding="utf-8")

    # Create a corrupted candidate report (failed step, ok=False)
    corrupted_report = create_report(scen, failure_index=1)
    corrupted_src = tmp_path / "candidate-run-report.json"
    corrupted_src.write_text(json.dumps(corrupted_report), encoding="utf-8")

    # Attempt to write evidence with corrupted candidate
    success = write_evidence_for_app(
        "esp.peripherals.uart_test",
        ws_root=tmp_path,
        report_src=corrupted_src,
        config_id="wasm_sim_standard",
        scenario_path=scen_file,
    )

    # MUST be rejected
    assert not success, "Writing evidence with a failing report must fail"

    # CRITICAL: Historical report MUST remain untouched!
    assert file_sha256(hist_report_file) == original_sha, "Historical report was prematurely overwritten!"

    # CRITICAL: Manifest delivery_state MUST NOT be updated to verified!
    updated_manifest = json.loads((gov_data / "checklist.data.json").read_text(encoding="utf-8"))
    exec_state = updated_manifest["entries"][0]["executions"][0]["delivery_state"]
    assert exec_state == "planned", f"Manifest state should remain 'planned', but became '{exec_state}'"


def test_atomic_overwrite_on_verification_success(tmp_path: Path):
    """A valid candidate report atomically updates destination and records verified evidence."""
    vendor_root = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61"
    gov_data = vendor_root / ".governance" / "data"
    gov_data.mkdir(parents=True)
    app_dir = vendor_root / "peripherals" / "uart_test"
    app_dir.mkdir(parents=True)

    assets_dir = app_dir / "unisim-assets"
    assets_dir.mkdir()
    (assets_dir / "wink_simulator.wasm").write_bytes(b"\x00asm\x01\x00\x00\x00")
    (assets_dir / "wink_simulator.js").write_text("console.log('sim');", encoding="utf-8")
    (assets_dir / "device-tree.json").write_text('{"name": "uart"}', encoding="utf-8")

    scen = create_scenario("uart_test")
    scen_dir = app_dir / "unisim-scenarios"
    scen_dir.mkdir()
    scen_file = scen_dir / "uart_test.scenario.json"
    scen_file.write_text(json.dumps(scen), encoding="utf-8")

    manifest = {
        "entries": [
            {
                "id": "esp.peripherals.uart_test",
                "target_app_dir": "peripherals/uart_test",
                "executions": [
                    {"config_id": "wasm_sim_standard", "delivery_state": "planned"}
                ],
            }
        ]
    }
    (gov_data / "checklist.data.json").write_text(json.dumps(manifest), encoding="utf-8")

    # Valid candidate report
    valid_report = create_report(scen)
    valid_src = tmp_path / "valid-run-report.json"
    valid_src.write_text(json.dumps(valid_report), encoding="utf-8")

    success = write_evidence_for_app(
        "esp.peripherals.uart_test",
        ws_root=tmp_path,
        report_src=valid_src,
        config_id="wasm_sim_standard",
        scenario_path=scen_file,
    )

    assert success is True

    # Report successfully installed
    dst_report = vendor_root / ".governance" / "reports" / "peripherals" / "uart_test" / "run-report.json"
    assert dst_report.is_file()
    assert file_sha256(dst_report) == file_sha256(valid_src)

    # Manifest delivery_state updated to verified with proper evidence bindings
    updated_manifest = json.loads((gov_data / "checklist.data.json").read_text(encoding="utf-8"))
    ex = updated_manifest["entries"][0]["executions"][0]
    assert ex["delivery_state"] == "verified"
    assert "evidence" in ex
    assert len(ex["evidence"]["assets_sha256"]) == 64
    assert len(ex["evidence"]["scenario_sha256"]) == 64
