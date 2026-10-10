# SPDX-License-Identifier: GPL-3.0-only
"""Adversarial regressions for bound reports, twin badges and candidate isolation."""
from __future__ import annotations

import copy
import importlib.util
import json
import sys
from pathlib import Path

import pytest

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"

from loop.mutator import CanaryMutator
from loop.pipeline import LoopPipeline
from gates.evidence_verifier import compute_assets_composite_sha256
from gates.report_contract import file_sha256, validate_scenario_report
from gates.twin_evidence import contract_sha256, verify_twin_evidence
from gates.rules import g1_carrier_landing_integrity, g1_scenario_semantic_integrity
from gates.rules import g5_network_assertion_quality, g5_wifi_assertion_quality


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")
    return path


def scenario(name="sample"):
    return {
        "header": {"name": name, "templateId": name, "timeoutUs": "1000000"},
        "steps": [{"type": "ASSERT_POINT", "target": "gpio:2", "timeUs": "100ms", "matcher": 1}],
    }


def report(scene, failure_index=None):
    results = []
    for index, step in enumerate(scene["steps"]):
        status = "passed" if failure_index is None or index < failure_index else "failed" if index == failure_index else "skipped"
        result = {"stepIndex": index, "type": step["type"], "status": status}
        if step["type"].startswith("ASSERT_") and status != "skipped":
            result.update(expected=step["matcher"], actual=1 if status == "failed" else step["matcher"])
        results.append(result)
    return {
        "total": 1, "passed": int(failure_index is None), "failed": int(failure_index is not None),
        "results": [{
            "ok": failure_index is None, "status": "passed" if failure_index is None else "failed",
            "header": copy.deepcopy(scene["header"]), "stepResults": results, "diagnostics": [],
            "summary": {
                "totalSteps": len(results),
                **{field: sum(step["status"] == status for step in results) for status, field in (
                    ("passed", "passedSteps"), ("failed", "failedSteps"), ("error", "errorSteps"), ("skipped", "skippedSteps"),
                )},
            },
        }],
    }


@pytest.fixture
def bound_mutant(tmp_path):
    base = write_json(tmp_path / "base.scenario.json", scenario())
    mutant, metadata = CanaryMutator().create_mutant_file(base)
    data = json.loads(mutant.read_text(encoding="utf-8"))
    result = report(data, metadata["step_index"])
    result_path = write_json(tmp_path / "run-report.json", result)
    metadata.update(report_path=str(result_path), report_sha256=file_sha256(result_path))
    return mutant, metadata, result_path, result


def test_valid_evaluated_assertion_kill(bound_mutant):
    _, meta, _, _ = bound_mutant
    assert CanaryMutator.verify_kill(1, "scenario failed", meta)[0]


def test_real_runner_error_diagnostic_cannot_be_counted_as_a_kill(bound_mutant):
    _, meta, path, data = bound_mutant
    data["results"][0]["diagnostics"] = [{
        "level": "error", "atUs": "1000", "source": "WASM:FW_DIAG_unsupportedFeature",
        "message": "Firmware health counter non-zero: unsupported feature use",
    }]
    write_json(path, data)
    meta["report_sha256"] = file_sha256(path)
    assert not CanaryMutator.verify_kill(1, "selected assertion failed", meta)[0]


@pytest.mark.parametrize("defect", [
    "wrong_scenario", "duplicate_step", "wrong_type", "wrong_expected", "unknown_target",
    "contradictory_count", "missing_count", "missing_actual", "passed_target", "extra_scenario",
    "boolean_expected",
])
def test_kill_rejects_misbound_or_unevaluated_results(bound_mutant, defect):
    _, meta, path, data = bound_mutant
    result = data["results"][0]
    step = result["stepResults"][0]
    if defect == "wrong_scenario":
        result["header"]["templateId"] = "another_run"
    elif defect == "duplicate_step":
        result["stepResults"].append(copy.deepcopy(step))
    elif defect == "wrong_type":
        step["type"] = "INPUT_BUS"
    elif defect == "wrong_expected":
        step["expected"] = 999
    elif defect == "boolean_expected":
        step["expected"] = False  # Python False == 0 must not weaken JSON binding.
    elif defect == "unknown_target":
        result["diagnostics"] = [{"code": "UNKNOWN_TARGET"}]
    elif defect == "contradictory_count":
        result["summary"]["failedSteps"] = 0
    elif defect == "missing_count":
        del result["summary"]["errorSteps"]
    elif defect == "missing_actual":
        del step["actual"]
    elif defect == "passed_target":
        step["status"] = "passed"
    elif defect == "extra_scenario":
        data["results"].append(copy.deepcopy(result))
    write_json(path, data)
    meta["report_sha256"] = file_sha256(path)
    assert not CanaryMutator.verify_kill(1, "Step #1 FAILED: expected mismatch", meta)[0]


@pytest.mark.parametrize("artifact", ["report", "mutant"])
def test_kill_rejects_artifact_replacement_after_collection(bound_mutant, artifact):
    mutant, meta, result_path, _ = bound_mutant
    path = mutant if artifact == "mutant" else result_path
    path.write_text(path.read_text(encoding="utf-8") + "\n", encoding="utf-8")
    assert not CanaryMutator.verify_kill(1, "FAILED", meta)[0]


def test_platform_fault_kill_targets_business_assertion_not_injection(tmp_path):
    base = write_json(tmp_path / "base.json", scenario())
    mutant, meta = CanaryMutator().create_mutant_file(base, dimension="platform_fault")
    assert meta["mutation_step_index"] == 0
    assert meta["step_index"] == 1
    data = report(json.loads(mutant.read_text(encoding="utf-8")), failure_index=1)
    path = write_json(tmp_path / "report.json", data)
    meta.update(report_path=str(path), report_sha256=file_sha256(path))
    assert CanaryMutator.verify_kill(1, "scenario failed", meta)[0]
    data["results"][0]["stepResults"][0]["status"] = "failed"
    write_json(path, data)
    meta["report_sha256"] = file_sha256(path)
    assert not CanaryMutator.verify_kill(1, "fault injection failed", meta)[0]


def test_stimulus_without_operator_does_not_become_assertion_self_check(tmp_path):
    base = write_json(tmp_path / "base.json", scenario())
    mutant, meta = CanaryMutator().create_mutant_file(base, dimension="stimulus")
    assert mutant is None
    assert "no fallback" in meta["error"]


def test_power_only_scenario_cannot_prove_business_sensitivity(tmp_path):
    data = scenario()
    data["steps"][0]["target"] = "power:VCC_3V3"
    base = write_json(tmp_path / "base.json", data)
    assert CanaryMutator().create_mutant_file(base)[0] is None


def test_structured_contract_accepts_the_existing_real_uart_baseline():
    vendor = TOOLS_DIR.parent.parent
    ok, reason = validate_scenario_report(
        vendor / ".governance/reports/peripherals/uart_echo/run-report.json",
        vendor / "peripherals/uart_echo/unisim-scenarios/uart_echo.scenario.json",
    )
    assert ok, reason


def test_fail_fast_native_pending_suffix_is_not_counted_as_executed(tmp_path):
    scene = scenario()
    scene["header"]["failurePolicy"] = "fail-fast"
    scene["steps"] = [copy.deepcopy(scene["steps"][0]) for _ in range(3)]
    data = report(scene, failure_index=1)
    result = data["results"][0]
    result["stepResults"][2] = {"stepIndex": 2, "type": "ASSERT_POINT", "status": "pending"}
    result["summary"]["skippedSteps"] = 0
    scene_path = write_json(tmp_path / "scene.json", scene)
    report_path = write_json(tmp_path / "report.json", data)
    ok, reason = validate_scenario_report(report_path, scene_path, failure_index=1)
    assert ok, reason
    assert not validate_scenario_report(report_path, scene_path)[0]


@pytest.mark.parametrize("defect", ["pending_prefix", "no_fail_fast", "pending_actual", "partial_suffix", "counted_as_passed"])
def test_pending_steps_cannot_hide_an_unexecuted_or_inconsistent_assertion(tmp_path, defect):
    scene = scenario()
    scene["header"]["failurePolicy"] = "fail-fast"
    scene["steps"] = [copy.deepcopy(scene["steps"][0]) for _ in range(4)]
    data = report(scene, failure_index=1)
    result = data["results"][0]
    result["stepResults"][2:] = [{"stepIndex": i, "type": "ASSERT_POINT", "status": "pending"} for i in (2, 3)]
    result["summary"]["skippedSteps"] = 0
    if defect == "pending_prefix":
        result["stepResults"][0]["status"] = "pending"
    elif defect == "no_fail_fast":
        scene["header"]["failurePolicy"] = "continue"
    elif defect == "pending_actual":
        result["stepResults"][2].update(actual=1, expected=1)
    elif defect == "partial_suffix":
        result["stepResults"].pop()
    else:
        result["summary"]["passedSteps"] += 2
    assert not validate_scenario_report(write_json(tmp_path / "report.json", data),
                                        write_json(tmp_path / "scene.json", scene), 1)[0]


def assets(directory):
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "wink_simulator.wasm").write_bytes(b"\0asm\1\0\0\0")
    (directory / "wink_simulator.js").write_text("// test asset", encoding="utf-8")
    write_json(directory / "device-tree.json", {"mcu": "esp32"})


@pytest.fixture
def twin_setup(tmp_path):
    vendor = tmp_path / "wink-micro-app/vendor/esp_idfv61"
    app = vendor / "peripherals/test_app"
    positive = write_json(app / "unisim-scenarios/positive.scenario.json", scenario("positive"))
    negative_scene = {
        "header": {"name": "fault", "templateId": "fault"},
        "steps": [
            {"type": "INJECT_PLATFORM_FAULT", "domain": "i2c", "fault": "timeout", "timeUs": "0ms"},
            {"type": "ASSERT_BUS_PAYLOAD", "busType": "uart", "busId": 0, "direction": "tx", "matcher": "ESP_ERR_TIMEOUT"},
        ],
    }
    negative = write_json(app / "unisim-scenarios/negative.fail.scenario.json", negative_scene)
    report_dir = vendor / ".governance/reports/peripherals/test_app"
    positive_report = write_json(report_dir / "run-report.json", report(scenario("positive")))
    negative_report = write_json(report_dir / "cfg/fault-report.json", report(negative_scene))
    assets(app / "unisim-assets")
    case = {"stimulus": "i2c_timeout", "expect_error": "ESP_ERR_TIMEOUT", "detects": "silent_success"}
    execution = {
        "config_id": "cfg", "delivery_state": "verified", "backend": "wasm_browser", "target_soc": "esp32", "profile": "standard",
        "acceptance": {"type": "wasm_simulation", "scenario_path": positive.relative_to(vendor).as_posix(), "negative_cases": [case]},
        "evidence": {
            "run_id": "baseline-1", "assets_sha256": compute_assets_composite_sha256(app / "unisim-assets"),
            "scenario_sha256": file_sha256(positive), "execution_report_ref": str(positive_report),
        },
    }
    entry = {
        "id": "esp.test", "display_id": 1, "upstream_path": "examples/peripherals/test",
        "target_app_dir": "peripherals/test_app", "scope": {"inclusion": "in_scope", "schedule": "active"},
        "audit": {"verdict": "audited", "auditor": "existing_independent_audit", "audited_configs": ["cfg"]},
        "executions": [execution],
    }

    def record(scene, result, run_id):
        return {"scenario_ref": scene.relative_to(vendor).as_posix(), "scenario_sha256": file_sha256(scene),
                "report_ref": result.relative_to(vendor).as_posix(), "report_sha256": file_sha256(result),
                "run_id": run_id, "assets_sha256": execution["evidence"]["assets_sha256"],
                **{key: execution[key] for key in ("config_id", "backend", "target_soc", "profile")}}

    proof = {
        "format_version": 1, "kind": "twin_proof", "app_id": entry["id"], "config_id": "cfg",
        "backend": "wasm_browser", "target_soc": "esp32", "profile": "standard",
        "baseline_run_id": "baseline-1", "assets_sha256": execution["evidence"]["assets_sha256"],
        "positive": record(positive, positive_report, "baseline-1"),
        "negative": [{**record(negative, negative_report, "fault-1"), "case_index": 0, "contract_sha256": contract_sha256(case),
                      "stimulus_step_index": 0, "assertion_step_index": 1}],
    }
    proof_path = report_dir / "cfg/twin-proof.json"
    return tmp_path, entry, execution, proof, proof_path


def renderer(tmp_path, monkeypatch):
    spec = importlib.util.spec_from_file_location("batch0_renderer", TOOLS_DIR / "generate_checklist_v1_1.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    monkeypatch.setattr(module, "WS_ROOT", tmp_path)
    monkeypatch.setattr(module, "verify_evidence", lambda *args, **kwargs: (True, []))
    return module


def test_fail_file_alone_never_lights_red_badge(twin_setup, monkeypatch):
    root, entry, execution, _, _ = twin_setup
    assert not verify_twin_evidence(entry, execution, root)[0]
    row, metric = renderer(root, monkeypatch).render_row(entry, {})
    assert metric == "verified"
    assert "Red ⏳" in row and "TWIN-PROOF" not in row


def test_complete_same_config_fault_proof_lights_red_badge(twin_setup, monkeypatch):
    root, entry, execution, proof, path = twin_setup
    write_json(path, proof)
    assert verify_twin_evidence(entry, execution, root)[0]
    assert renderer(root, monkeypatch).render_row(entry, {})[1] == "verified_twin"


def test_twin_rejects_negative_report_from_other_assets(twin_setup):
    root, entry, execution, proof, path = twin_setup
    proof["negative"][0]["assets_sha256"] = "f" * 64
    write_json(path, proof)
    assert not verify_twin_evidence(entry, execution, root)[0]


@pytest.mark.parametrize("defect", [
    "config", "run", "assets", "report_hash", "contract", "ordinary_stimulus", "candidate_report", "self_audit",
    "negative_config", "version_bool", "case_bool", "reused_run", "missing_run", "error_suffix",
])
def test_twin_rejects_wrong_identity_or_non_fault_proof(twin_setup, defect):
    root, entry, execution, proof, path = twin_setup
    if defect == "config":
        proof["config_id"] = "another_cfg"
    elif defect == "run":
        proof["baseline_run_id"] = "old_run"
    elif defect == "assets":
        proof["assets_sha256"] = "f" * 64
    elif defect == "negative_config":
        proof["negative"][0]["config_id"] = "other_cfg"
    elif defect == "version_bool":
        proof["format_version"] = True
    elif defect == "case_bool":
        proof["negative"][0]["case_index"] = False
    elif defect == "reused_run":
        proof["negative"][0]["run_id"] = proof["baseline_run_id"]
    elif defect == "missing_run":
        del proof["negative"][0]["run_id"]
    elif defect == "error_suffix":
        vendor = root / "wink-micro-app/vendor/esp_idfv61"
        report_path = vendor / proof["negative"][0]["report_ref"]
        data = json.loads(report_path.read_text(encoding="utf-8"))
        data["results"][0]["stepResults"][1]["actual"] = "ESP_ERR_TIMEOUT_IGNORED"
        write_json(report_path, data)
        proof["negative"][0]["report_sha256"] = file_sha256(report_path)
    elif defect == "report_hash":
        proof["negative"][0]["report_sha256"] = "f" * 64
    elif defect == "contract":
        execution["acceptance"]["negative_cases"][0]["expect_error"] = "ESP_ERR_NOT_FOUND"
    elif defect == "self_audit":
        entry["audit"]["auditor"] = "loop_sop_daemon"
    elif defect == "candidate_report":
        proof["negative"][0]["report_ref"] = ".governance/runs/candidate/run-report.json"
    elif defect == "ordinary_stimulus":
        vendor = root / "wink-micro-app/vendor/esp_idfv61"
        scene_path = vendor / proof["negative"][0]["scenario_ref"]
        scene = json.loads(scene_path.read_text(encoding="utf-8"))
        scene["steps"][0] = {"type": "INPUT_BUS", "payload": {"text": "CORRUPTED_PACKET_TEST"}}
        write_json(scene_path, scene)
        report_path = vendor / proof["negative"][0]["report_ref"]
        write_json(report_path, report(scene))
        proof["negative"][0].update(scenario_sha256=file_sha256(scene_path), report_sha256=file_sha256(report_path))
    write_json(path, proof)
    assert not verify_twin_evidence(entry, execution, root)[0]


@pytest.fixture
def candidate_setup(tmp_path, monkeypatch):
    vendor = tmp_path / "wink-micro-app/vendor/esp_idfv61"
    app = vendor / "peripherals/test_app"
    write_json(app / "wink-app.json", {"app_name": "test_app"})
    (app / "main.c").write_text("/* existing user changes */\n", encoding="utf-8")
    source = write_json(app / "unisim-scenarios/base.scenario.json", scenario())
    execution = {
        "config_id": "wasm_sim_standard", "backend": "wasm_browser", "target_soc": "esp32", "profile": "standard",
        "delivery_state": "verified", "evidence": {"run_id": "historical"},
        "acceptance": {"type": "wasm_simulation", "scenario_path": source.relative_to(vendor).as_posix()},
    }
    entry = {"id": "esp.test", "target_app_dir": "peripherals/test_app", "executions": [execution]}
    manifest = write_json(vendor / ".governance/data/checklist.data.json", {"entries": [entry]})
    old_report = vendor / ".governance/reports/peripherals/test_app/run-report.json"
    old_report.parent.mkdir(parents=True)
    old_report.write_text("historical report", encoding="utf-8")
    board = vendor / "CHECKLIST.md"
    board.write_text("historical board", encoding="utf-8")
    runner_script = tmp_path / "wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1"
    runner_script.parent.mkdir(parents=True)
    runner_script.write_text("# fixture runner", encoding="utf-8")
    pipeline = LoopPipeline(tmp_path)
    monkeypatch.setattr(pipeline, "run_python", lambda *args: (0, "Gate 1 passed"))
    calls = []

    def run_phase(args):
        calls.append(args)
        phase_dir = Path(args[args.index("-ArtifactsDir") + 1])
        scene_path = Path(args[args.index("-Scenario") + 1])
        scene = json.loads(scene_path.read_text(encoding="utf-8"))
        failure_index = len(scene["steps"]) - 1 if phase_dir.name == "assertion_self_check" else None
        write_json(phase_dir / "run-report.json", report(scene, failure_index))
        assets(Path(args[args.index("-App") + 1]) / "unisim-assets")
        return int(failure_index is not None), "selected scenario completed"

    monkeypatch.setattr(pipeline, "run_powershell", run_phase)
    originals = {path: path.read_bytes() for path in (manifest, old_report, board, app / "main.c", source)}
    return pipeline, entry, calls, originals, run_phase


def test_candidate_success_preserves_formal_evidence_and_user_changes(candidate_setup):
    pipeline, entry, calls, originals, _ = candidate_setup
    result = pipeline.execute_app(entry)
    assert result.success and result.stage == "CANDIDATE"
    data = json.loads(result.candidate_path.read_text(encoding="utf-8"))
    assert data["status"] == "candidate_ready" and "audit" not in data
    assert [check["kind"] for check in data["checks"]] == ["baseline", "assertion_self_check", "recovery"]
    assert all("-WriteEvidence" not in args for args in calls)
    assert all(path.read_bytes() == content for path, content in originals.items())
    again = pipeline.execute_app(entry)
    assert again.success and again.candidate_path != result.candidate_path


@pytest.mark.parametrize("defect, expected_stage", [
    ("survived", "CANARY_KILL"), ("no_report", "CANARY_KILL"),
    ("recovery_failed", "RECOVERY"), ("baseline_failed", "BASELINE"),
])
def test_candidate_failure_preserves_formal_evidence(candidate_setup, monkeypatch, defect, expected_stage):
    pipeline, entry, calls, originals, normal_run = candidate_setup

    def faulty_run(args):
        phase = Path(args[args.index("-ArtifactsDir") + 1]).name
        if phase == "assertion_self_check" and defect == "no_report":
            calls.append(args)
            return 124, "Runner timeout while waiting for runtime"
        rc, output = normal_run(args)
        if phase == "assertion_self_check" and defect == "survived":
            return 0, "all passed"
        if phase == "recovery" and defect == "recovery_failed" or phase == "baseline" and defect == "baseline_failed":
            return 1, "Runner failed"
        return rc, output

    monkeypatch.setattr(pipeline, "run_powershell", faulty_run)
    result = pipeline.execute_app(entry)
    assert not result.success and result.stage == expected_stage
    assert all(path.read_bytes() == content for path, content in originals.items())
    data = json.loads(result.candidate_path.read_text(encoding="utf-8"))
    assert data["status"] == "failed"
    if defect != "baseline_failed":
        assert data["checks"][-1]["kind"] == "recovery"


def test_dry_run_has_no_execution_or_candidate_writes(candidate_setup):
    pipeline, entry, calls, _, _ = candidate_setup
    pipeline.dry_run = True
    result = pipeline.execute_app(entry)
    assert result.success and result.stage == "PLANNED"
    assert not calls and not (pipeline.governance_dir / "runs").exists()


def test_workspace_gate_failure_retains_candidate_without_running_simulation(candidate_setup, monkeypatch):
    pipeline, entry, calls, originals, _ = candidate_setup
    monkeypatch.setattr(pipeline, "run_python", lambda *args: (2, "Executor configuration failed"))
    result = pipeline.execute_app(entry)
    assert not result.success and result.stage == "GATE_1_PRE"
    assert not calls
    assert json.loads(result.candidate_path.read_text(encoding="utf-8"))["checks"] == []
    assert all(path.read_bytes() == content for path, content in originals.items())


def test_unknown_or_ambiguous_configuration_never_falls_back(candidate_setup):
    pipeline, entry, calls, _, _ = candidate_setup
    assert not pipeline.execute_app(entry, config_id="unknown").success
    entry["executions"].append({**entry["executions"][0], "config_id": "second"})
    assert not pipeline.execute_app(entry).success
    assert not calls


@pytest.mark.parametrize("rule", [
    g1_carrier_landing_integrity, g1_scenario_semantic_integrity,
    g5_network_assertion_quality, g5_wifi_assertion_quality,
])
def test_candidate_snapshots_are_excluded_from_formal_corpus(tmp_path, rule):
    vendor = tmp_path / "wink-micro-app/vendor/esp_idfv61"
    snapshot = vendor / ".governance/runs/run/app/wifi_bad"
    write_json(snapshot / "wink-app.json", {})
    broken_scenario = snapshot / "unisim-scenarios/bad.scenario.json"
    broken_scenario.parent.mkdir()
    broken_scenario.write_text("invalid JSON", encoding="utf-8")
    context = {"workspace_root": str(tmp_path), "manifest": {"entries": []}}
    assert rule.run(context) == []
    # The same defects outside the reserved namespace remain subject to gates.
    landed = vendor / "peripherals/wifi_bad"
    write_json(landed / "wink-app.json", {})
    landed_scenario = landed / "unisim-scenarios/bad.scenario.json"
    landed_scenario.parent.mkdir()
    landed_scenario.write_text("invalid JSON", encoding="utf-8")
    assert rule.run(context)


def test_candidate_namespace_cannot_be_registered_as_a_formal_carrier(tmp_path):
    vendor = tmp_path / "wink-micro-app/vendor/esp_idfv61"
    target = ".governance/runs/run/app/sample"
    write_json(vendor / target / "wink-app.json", {})
    context = {"workspace_root": str(tmp_path), "manifest": {"entries": [
        {"id": "esp.test", "target_app_dir": target, "executions": []},
    ]}}
    assert any(finding["severity"] == "error" for finding in g1_carrier_landing_integrity.run(context))
