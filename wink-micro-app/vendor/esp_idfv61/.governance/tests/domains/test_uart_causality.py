# SPDX-License-Identifier: GPL-3.0-only
"""UART candidate behavior through the CLI and external runner boundary."""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"
VENDOR_DIR = TOOLS_DIR.parent.parent
from loop.uart_causality import UartCausalityPipeline
from gates.report_contract import file_sha256


@pytest.fixture
def uart_workspace(tmp_path):
    vendor = tmp_path / "wink-micro-app/vendor/esp_idfv61"
    app = vendor / "peripherals/uart_echo"
    shutil.copytree(VENDOR_DIR / "peripherals/uart_echo", app,
                    ignore=shutil.ignore_patterns("unisim-assets", "__pycache__"))
    original = json.loads((VENDOR_DIR / ".governance/data/checklist.data.json").read_text(encoding="utf-8"))
    entry = next(item for item in original["entries"] if item["id"] == "esp.peripherals.uart.uart_echo")
    manifest = vendor / ".governance/data/checklist.data.json"
    manifest.parent.mkdir(parents=True)
    manifest.write_text(json.dumps({"entries": [entry]}), encoding="utf-8")
    runner = tmp_path / "wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1"
    runner.parent.mkdir(parents=True)
    runner.write_text("# external runner fixture", encoding="utf-8")
    board = vendor / "CHECKLIST.md"
    board.write_text("historical board", encoding="utf-8")
    historical = vendor / ".governance/reports/peripherals/uart_echo/run-report.json"
    historical.parent.mkdir(parents=True)
    historical.write_text("historical report", encoding="utf-8")
    originals = {path: path.read_bytes() for path in (manifest, board, historical, *app.rglob("*")) if path.is_file()}
    return tmp_path, entry, originals


def test_uart_cli_dry_run_describes_checks_without_writing(uart_workspace):
    workspace, _, originals = uart_workspace
    result = subprocess.run(
        [sys.executable, "-X", "utf8", "-B", str(TOOLS_DIR / "run_loop.py"),
         "--workspace-root", str(workspace), "--app", "uart_echo",
         "--config-id", "wasm_sim_standard", "--proof-profile", "uart-causality", "--dry-run"],
        capture_output=True, text=True, encoding="utf-8", timeout=30,
    )
    assert result.returncode == 0, result.stderr
    assert "firmware dependency" in result.stdout
    assert "business mutation" in result.stdout
    assert not (workspace / "wink-micro-app/vendor/esp_idfv61/.governance/runs").exists()
    assert all(path.read_bytes() == content for path, content in originals.items())


@pytest.fixture
def uart_processes(uart_workspace, monkeypatch):
    workspace, entry, originals = uart_workspace
    pipeline = UartCausalityPipeline(workspace)
    monkeypatch.setattr(pipeline, "run_python", lambda *args: (0, "Gate 1 executed"))
    calls = []

    def run_phase(args):
        phase = Path(args[args.index("-ArtifactsDir") + 1])
        app = Path(args[args.index("-App") + 1])
        calls.append((phase.name, (app / "uart_echo_example_main.c").read_bytes()))
        scene = json.loads(Path(args[args.index("-Scenario") + 1]).read_text(encoding="utf-8"))
        failure = 2 if phase.name in ("assertion_self_check", "firmware_dependency", "business_mutation") else None
        result_steps = []
        for index, step in enumerate(scene["steps"]):
            status = "failed" if index == failure else "passed"
            result = {"stepIndex": index, "type": step["type"], "status": status}
            if step["type"].startswith("ASSERT_"):
                actual = step["matcher"]
                if step["type"] == "ASSERT_BUS_PAYLOAD":
                    payload = [] if phase.name == "firmware_dependency" else [
                        "IELLO_ESP32_WINK" if phase.name == "business_mutation" else
                        "HELLO_ESP32_WINK" if phase.name == "assertion_self_check" else step["matcher"]]
                    actual = {"matchedCount": int(status == "passed"), "candidatePayloads": payload}
                result.update(expected=step["matcher"], actual=actual)
            result_steps.append(result)
        report = {"total": 1, "passed": int(failure is None), "failed": int(failure is not None),
                  "results": [{"header": scene["header"], "ok": failure is None,
                               "status": "passed" if failure is None else "failed", "diagnostics": [],
                               "summary": {"totalSteps": len(result_steps),
                                           "passedSteps": len(result_steps) - int(failure is not None),
                                           "failedSteps": int(failure is not None), "errorSteps": 0, "skippedSteps": 0},
                               "stepResults": result_steps}]}
        (phase / "run-report.json").write_text(json.dumps(report), encoding="utf-8")
        assets = app / "unisim-assets"
        assets.mkdir(exist_ok=True)
        (assets / "wink_simulator.wasm").write_bytes({
            "firmware_dependency": b"disabled wasm", "business_mutation": b"corrupt wasm",
        }.get(phase.name, b"original wasm"))
        (assets / "wink_simulator.js").write_text("runner asset", encoding="utf-8")
        (assets / "device-tree.json").write_text('{"mcu":"esp32"}', encoding="utf-8")
        return int(failure is not None), "scenario completed"

    monkeypatch.setattr(pipeline, "run_powershell", run_phase)
    return pipeline, entry, originals, calls, run_phase


def test_uart_collects_distinct_causal_checks_without_claiming_fault_handling(uart_processes):
    pipeline, entry, originals, calls, _ = uart_processes
    result = pipeline.execute_app(entry, "wasm_sim_standard")
    assert result.candidate_path is not None
    data = json.loads(result.candidate_path.read_text(encoding="utf-8"))
    assert not result.success and result.stage == "UART_CONTRACT"
    assert data["status"] == "candidate_incomplete" and data["causality_complete"]
    assert data["fault_handling"]["status"] == "contract_gap"
    checks = {check["kind"]: check for check in data["checks"]}
    assert list(checks) == ["baseline", "assertion_self_check", "firmware_dependency",
                            "business_mutation", "rx_characterization", "recovery"]
    assert all(check["accepted"] for check in checks.values())
    baseline = checks["baseline"]
    for kind in ("firmware_dependency", "business_mutation"):
        assert checks[kind]["scenario_sha256"] == baseline["scenario_sha256"]
        assert checks[kind]["firmware_sha256"] != baseline["firmware_sha256"]
        assert checks[kind]["source_sha256"] != baseline["source_sha256"]
        assert checks[kind]["input_sha256"] != baseline["input_sha256"]
    assert checks["recovery"]["assets_sha256"] == baseline["assets_sha256"]
    assert calls[0][1] == calls[-1][1]
    assert b"data[0] ^= 1u" in next(source for kind, source in calls if kind == "business_mutation")
    for check in checks.values():
        assert file_sha256(Path(check["report_path"])) == check["report_sha256"]
        assert file_sha256(Path(check["assets_path"]) / "wink_simulator.wasm") == check["firmware_sha256"]
        assert file_sha256(Path(check["source_path"])) == check["source_sha256"]
    assert all(path.read_bytes() == content for path, content in originals.items())


def test_spaced_rx_truncation_is_a_regression_with_a_valid_observation(uart_processes, monkeypatch):
    pipeline, entry, originals, calls, normal_run = uart_processes

    def truncated_rx(args):
        rc, output = normal_run(args)
        phase = Path(args[args.index("-ArtifactsDir") + 1])
        if phase.name == "rx_characterization":
            path = phase / "run-report.json"
            data = json.loads(path.read_text(encoding="utf-8"))
            result = data["results"][0]
            data.update(passed=0, failed=1)
            result.update(ok=False, status="failed")
            result["stepResults"] = result["stepResults"][:7]
            result["stepResults"][6].update(status="failed", actual={"matchedCount": 0, "candidatePayloads": ["C" * 64]})
            result["summary"].update(totalSteps=7, passedSteps=6, failedSteps=1)
            path.write_text(json.dumps(data), encoding="utf-8")
            return 1, "assertion observation window completed"
        return rc, output

    monkeypatch.setattr(pipeline, "run_powershell", truncated_rx)
    result = pipeline.execute_app(entry)
    assert not result.success and result.stage == "UART_RX_BOUNDARY"
    data = json.loads(result.candidate_path.read_text(encoding="utf-8"))
    rx = next(check for check in data["checks"] if check["kind"] == "rx_characterization")
    assert not rx["accepted"] and rx["observation_valid"]
    assert rx["outcome"] == "cumulative_rx_truncation"
    assert rx["failed_step_index"] == 6
    assert rx["observed_tx"]["candidatePayloads"] == ["C" * 64]
    assert data["causality_complete"] and data["checks"][-1]["accepted"]
    assert calls[0][1] == calls[-1][1]
    assert all(path.read_bytes() == content for path, content in originals.items())


@pytest.mark.parametrize("defect, stage", [
    ("stale_firmware", "FIRMWARE_DEPENDENCY"), ("wrong_tx", "BUSINESS_MUTATION"),
    ("positive_match_count", "BUSINESS_MUTATION"),
    ("no_report", "BUSINESS_MUTATION"), ("survived", "BUSINESS_MUTATION"),
    ("runtime_error", "BUSINESS_MUTATION"), ("bad_recovery", "RECOVERY"),
])
def test_uart_rejects_invalid_kills_and_still_restores_the_source(uart_processes, monkeypatch, defect, stage):
    pipeline, entry, originals, calls, normal_run = uart_processes

    def faulty_run(args):
        phase = Path(args[args.index("-ArtifactsDir") + 1])
        if phase.name == "business_mutation" and defect == "no_report":
            return 124, "ASSERTION FAILED then runner timed out"
        rc, output = normal_run(args)
        if phase.name == "firmware_dependency" and defect == "stale_firmware":
            app = Path(args[args.index("-App") + 1])
            (app / "unisim-assets/wink_simulator.wasm").write_bytes(b"original wasm")
        if phase.name == "business_mutation" and defect in ("wrong_tx", "runtime_error", "positive_match_count"):
            path = phase / "run-report.json"
            data = json.loads(path.read_text(encoding="utf-8"))
            if defect == "wrong_tx":
                data["results"][0]["stepResults"][2]["actual"]["candidatePayloads"] = ["HELLO_ESP32_WINK"]
            elif defect == "positive_match_count":
                data["results"][0]["stepResults"][2]["actual"]["matchedCount"] = 1
            else:
                data["results"][0]["diagnostics"] = [{"level": "error", "source": "WASM:FW_DIAG_unsupportedFeature"}]
            path.write_text(json.dumps(data), encoding="utf-8")
        if phase.name == "business_mutation" and defect == "survived":
            return 0, "all steps passed"
        if phase.name == "recovery" and defect == "bad_recovery":
            return 1, "failed to restore runtime"
        return rc, output

    monkeypatch.setattr(pipeline, "run_powershell", faulty_run)
    result = pipeline.execute_app(entry)
    assert not result.success and result.stage == stage
    data = json.loads(result.candidate_path.read_text(encoding="utf-8"))
    assert not data["causality_complete"]
    assert data["checks"][-1]["kind"] == "recovery"
    assert calls[0][1] == calls[-1][1]
    assert all(path.read_bytes() == content for path, content in originals.items())


def test_uart_exception_during_mutation_runs_recovery_before_returning(uart_processes, monkeypatch):
    pipeline, entry, originals, calls, normal_run = uart_processes

    def interrupted_run(args):
        if Path(args[args.index("-ArtifactsDir") + 1]).name == "business_mutation":
            raise OSError("external process could not start")
        return normal_run(args)

    monkeypatch.setattr(pipeline, "run_powershell", interrupted_run)
    result = pipeline.execute_app(entry)
    assert not result.success and result.stage == "COLLECTION"
    data = json.loads(result.candidate_path.read_text(encoding="utf-8"))
    assert data["checks"][-1]["kind"] == "recovery" and data["checks"][-1]["accepted"]
    assert calls[0][1] == calls[-1][1]
    assert all(path.read_bytes() == content for path, content in originals.items())


@pytest.mark.parametrize("config_id", ["not_registered", "wasm_sim_standard"])
def test_uart_rejects_unknown_configuration_or_modified_upstream(uart_processes, config_id):
    pipeline, entry, _, calls, _ = uart_processes
    if config_id == "wasm_sim_standard":
        original = pipeline.vendor_root / "peripherals/uart_echo" / "uart_echo_example_main.c"
        original.write_text("/* existing user edit */", encoding="utf-8")
    result = pipeline.execute_app(entry, config_id)
    assert not result.success and result.stage == "CONFIGURATION"
    assert not calls and not (pipeline.governance_dir / "runs").exists()
