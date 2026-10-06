# SPDX-License-Identifier: GPL-3.0-only
"""Candidate UART error contract at the public report and CLI boundaries."""
import copy
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

TOOLS_DIR = Path(__file__).resolve().parents[2] / "tools"
VENDOR_DIR = TOOLS_DIR.parent.parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))
from loop.uart_events_fault import make_contract, validate_fault_report
from loop.uart_events_fault import UartEventsFaultPipeline, UPSTREAM_SHA256
from report_contract import file_sha256


@pytest.fixture
def fault_report(tmp_path):
    assets = tmp_path / "assets"
    assets.mkdir()
    for name in ("wink_simulator.wasm", "wink_simulator.js", "device-tree.json"):
        (assets / name).write_text(name, encoding="utf-8")
    harness = TOOLS_DIR / "loop/uart_events_fault_harness.cjs"
    contract = tmp_path / "fault.contract.json"
    contract.write_text(json.dumps(make_contract("frame")), encoding="utf-8")
    report_path = tmp_path / "fault.report.json"
    def frames(payload, time):
        return [{"port": 0, "time_us": time, "hex": f"{byte:02x}"} for byte in payload.encode()]
    report = {
        "format_version": 1, "kind": "uart_events_fault_report", "run_id": "fixture-frame",
        "backend": "wasm_node_abi_harness", "case": "frame", "same_instance": True,
        "contract_sha256": file_sha256(contract), "harness_sha256": file_sha256(harness),
        "firmware_sha256": file_sha256(assets / "wink_simulator.wasm"),
        "glue_sha256": file_sha256(assets / "wink_simulator.js"),
        "device_tree_sha256": file_sha256(assets / "device-tree.json"),
        "runtime_errors": [], "exit_code": 0,
        "injections": [{"function": "pal_wasm_push_uart_rx_error", "port": 0, "flags": 1, "time_us": 2000}],
        "step_results": [
            {"index": 0, "type": "normal_echo", "start_us": 0, "end_us": 1000, "status": "passed",
             "expected": {"echo_hex": "5052455f4652414d455f57494e4b", "fault_log_count": 0},
             "tx_frames": frames("PRE_FRAME_WINK", 500)},
            {"index": 1, "type": "fault_handling", "start_us": 2000, "end_us": 3000, "status": "passed",
             "expected": {"fault_log": "uart frame error", "fault_log_count": 1},
             "tx_frames": [{"port": 0, "time_us": 2500, "hex": "I (2) uart_events: uart frame error\n".encode().hex()}]},
            {"index": 2, "type": "same_instance_recovery", "start_us": 4000, "end_us": 5000, "status": "passed",
             "expected": {"echo_hex": "504f53545f4652414d455f57494e4b", "fault_log_count": 0},
             "tx_frames": frames("POST_FRAME_WINK", 4500)},
        ],
    }
    def validate(value, mutant=False):
        report_path.write_text(json.dumps(value), encoding="utf-8")
        return validate_fault_report(report_path, contract, assets, harness, "fixture-frame", mutant)
    return report, validate, assets


def test_accepts_raw_fault_and_same_instance_recovery(fault_report):
    report, validate, _ = fault_report
    assert validate(report)[0]
    report = copy.deepcopy(report)
    report["step_results"][1].update(tx_frames=[], status="failed", end_us=502000)
    report["step_results"][2].update(start_us=503000, end_us=504000)
    for frame in report["step_results"][2]["tx_frames"]:
        frame["time_us"] = 503500
    report["exit_code"] = 1
    assert validate(report, mutant=True)[0]
    assert not validate(report)[0]


@pytest.mark.parametrize("defect", ["missing_log", "wrong_port", "wrong_error", "late_log", "early_log",
                                    "no_echo", "wrong_recovery", "runtime_error", "wrong_run", "wrong_injection",
                                    "bool_flags", "duplicate_step", "same_instance_false", "glue_changed"])
def test_rejects_forged_incomplete_or_misbound_fault_evidence(fault_report, defect):
    report, validate, assets = fault_report
    step = report["step_results"][1]
    if defect == "missing_log": step["tx_frames"] = []
    elif defect == "wrong_port": step["tx_frames"][0]["port"] = 1
    elif defect == "wrong_error": step["tx_frames"][0]["hex"] = "uart_events: uart parity error".encode().hex()
    elif defect == "late_log": step.update(end_us=513000); step["tx_frames"][0]["time_us"] = 512001
    elif defect == "early_log": step["tx_frames"][0]["time_us"] = 1999
    elif defect == "no_echo": report["step_results"][0]["tx_frames"] = []
    elif defect == "wrong_recovery": report["step_results"][2]["tx_frames"][0]["hex"] = "00"
    elif defect == "runtime_error": report["runtime_errors"] = ["Wasm trap"]
    elif defect == "wrong_run": report["run_id"] = "stale-frame"
    elif defect == "wrong_injection": report["injections"][0]["flags"] = 4
    elif defect == "bool_flags": report["injections"][0]["flags"] = True
    elif defect == "duplicate_step": report["step_results"].append(copy.deepcopy(step))
    elif defect == "same_instance_false": report["same_instance"] = False
    elif defect == "glue_changed": (assets / "wink_simulator.js").write_text("changed", encoding="utf-8")
    assert not validate(report)[0]


def test_fault_mutation_requires_normal_and_recovery_alive(fault_report):
    report, validate, _ = fault_report
    report["exit_code"] = 1
    report["step_results"][1].update(status="failed", tx_frames=[])
    report["step_results"][0].update(status="failed", tx_frames=[])
    assert not validate(report, mutant=True)[0]


def test_events_cli_dry_run_locks_scope_without_writing(tmp_path):
    vendor = tmp_path / "wink-micro-app/vendor/esp_idfv61"
    app = vendor / "peripherals/uart_uart_events"
    shutil.copytree(VENDOR_DIR / "peripherals/uart_uart_events", app,
                    ignore=shutil.ignore_patterns("unisim-assets", "__pycache__"))
    manifest = vendor / ".governance/data/checklist.data.json"
    manifest.parent.mkdir(parents=True)
    data = json.loads((VENDOR_DIR / ".governance/data/checklist.data.json").read_text(encoding="utf-8"))
    entry = next(item for item in data["entries"] if item["id"] == "esp.peripherals.uart.uart_events")
    manifest.write_text(json.dumps({"entries": [entry]}), encoding="utf-8")
    original = {file: file.read_bytes() for file in vendor.rglob("*") if file.is_file()}
    args = [sys.executable, "-X", "utf8", "-B", str(TOOLS_DIR / "run_loop.py"),
            "--workspace-root", str(tmp_path), "--app", "uart_uart_events", "--config-id", "wasm_sim_standard",
            "--proof-profile", "uart-events-fault", "--dry-run"]
    result = subprocess.run(args, capture_output=True, text=True, encoding="utf-8", timeout=30)
    assert result.returncode == 0, result.stderr
    assert "same-instance recovery" in result.stdout
    assert not (vendor / ".governance/runs").exists()
    assert all(file.read_bytes() == content for file, content in original.items())
    (app / "uart_events_example_main.c").write_text("changed upstream", encoding="utf-8")
    result = subprocess.run(args, capture_output=True, text=True, encoding="utf-8", timeout=30)
    assert result.returncode != 0
    assert "upstream" in result.stdout.lower()
    assert not (vendor / ".governance/runs").exists()


@pytest.mark.parametrize("failure", [None, "node_runtime_error", "stale_mutant", "recovery_runner_error"])
def test_fault_collector_preserves_sources_and_restores_after_failures(tmp_path, monkeypatch, failure):
    if os.name == "nt":
        tmp_path = Path("\\\\?\\" + str(tmp_path))
    vendor = tmp_path / "wink-micro-app/vendor/esp_idfv61"
    app = vendor / "peripherals/uart_uart_events"
    shutil.copytree(VENDOR_DIR / "peripherals/uart_uart_events", app,
                    ignore=shutil.ignore_patterns("unisim-assets", "__pycache__"))
    manifest = vendor / ".governance/data/checklist.data.json"
    manifest.parent.mkdir(parents=True)
    data = json.loads((VENDOR_DIR / ".governance/data/checklist.data.json").read_text(encoding="utf-8"))
    entry = next(item for item in data["entries"] if item["id"] == "esp.peripherals.uart.uart_events")
    manifest.write_text(json.dumps({"entries": [entry]}), encoding="utf-8")
    originals = {file: file.read_bytes() for file in vendor.rglob("*") if file.is_file()}
    runner = tmp_path / "wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1"
    runner.parent.mkdir(parents=True)
    runner.write_text("# external runner fixture", encoding="utf-8")
    pipeline = UartEventsFaultPipeline(tmp_path)
    monkeypatch.setattr(pipeline, "run_python", lambda *args: (0, "external Gate 1 fixture"))

    def cli(args):
        phase = Path(args[args.index("-ArtifactsDir") + 1])
        run_app = Path(args[args.index("-App") + 1])
        scene = json.loads(Path(args[args.index("-Scenario") + 1]).read_text(encoding="utf-8"))
        if failure == "recovery_runner_error" and phase.name == "events_restored":
            return 2, "external runner failed"
        steps = []
        failed_index = 6 if phase.name == "assertion_self_check" else None
        for index, step in enumerate(scene["steps"]):
            result = {"stepIndex": index, "type": step["type"], "status": "failed" if index == failed_index else "passed"}
            if step["type"].startswith("ASSERT_"):
                result.update(expected=step["matcher"], actual={"matchedCount": int(index != failed_index),
                              "candidatePayloads": ["read pat : +++" if index == failed_index else step["matcher"]]})
            steps.append(result)
        report = {"total": 1, "passed": int(failed_index is None), "failed": int(failed_index is not None),
                  "results": [{"header": scene["header"], "ok": failed_index is None,
                  "status": "passed" if failed_index is None else "failed", "diagnostics": [],
                  "summary": {"totalSteps": len(steps), "passedSteps": len(steps) - int(failed_index is not None),
                              "failedSteps": int(failed_index is not None), "errorSteps": 0, "skippedSteps": 0},
                  "stepResults": steps}]}
        (phase / "run-report.json").write_text(json.dumps(report), encoding="utf-8")
        assets = run_app / "unisim-assets"
        assets.mkdir(exist_ok=True)
        source_bytes = (run_app / "uart_events_example_main.c").read_bytes()
        content = b"fixture original" if failure == "stale_mutant" else hashlib.sha256(source_bytes).digest()
        (assets / "wink_simulator.wasm").write_bytes(content)
        (assets / "wink_simulator.js").write_text("fixture glue", encoding="utf-8")
        (assets / "device-tree.json").write_text('{"mcu":"esp32"}', encoding="utf-8")
        return (1 if failed_index is not None else 0), "external CLI fixture"

    def node(args):
        harness = Path(args[0])
        assets = Path(args[args.index("--assets") + 1])
        contract_path = Path(args[args.index("--contract") + 1])
        report_path = Path(args[args.index("--report") + 1])
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
        case = contract["case"]
        removed = "handler_removal" in report_path.parts and case != "control"
        error_text = {"control": None, "frame": "uart frame error", "parity": "uart parity error", "fifo": "hw fifo overflow"}[case]
        steps = []
        for index, start in enumerate((0, 2000, 503000)):
            if index == 1:
                expected = {"fault_log": error_text, "fault_log_count": int(case != "control")}
                frames = [] if removed or case == "control" else [
                    {"port": 0, "time_us": start + 500, "hex": f"uart_events: {error_text}\n".encode().hex()}]
            else:
                payload = contract["before_payload" if index == 0 else "recovery_payload"]
                expected = {"echo_hex": payload.encode().hex(), "fault_log_count": 0}
                frames = [{"port": 0, "time_us": start + 500, "hex": f"{byte:02x}"} for byte in payload.encode()]
            steps.append({"index": index, "type": ["normal_echo", "fault_handling", "same_instance_recovery"][index],
                          "start_us": start, "end_us": start + (500000 if index == 1 else 1000),
                          "expected": expected, "status": "failed" if removed and index == 1 else "passed", "tx_frames": frames})
        report = {"format_version": 1, "kind": "uart_events_fault_report", "backend": "wasm_node_abi_harness",
                  "case": case, "run_id": args[args.index("--run-id") + 1], "same_instance": True,
                  "contract_sha256": file_sha256(contract_path), "harness_sha256": file_sha256(harness),
                  "firmware_sha256": file_sha256(assets / "wink_simulator.wasm"),
                  "glue_sha256": file_sha256(assets / "wink_simulator.js"),
                  "device_tree_sha256": file_sha256(assets / "device-tree.json"), "runtime_errors": [],
                  "exit_code": int(removed), "step_results": steps,
                  "injections": [{"function": "pal_wasm_push_uart_rx_error", "port": 0, "flags": contract["flags"], "time_us": 2000}]}
        if failure == "node_runtime_error" and removed:
            report.update(runtime_errors=["Wasm trap"], exit_code=2)
        report_path.write_text(json.dumps(report), encoding="utf-8")
        return report["exit_code"], "external Node fixture"

    monkeypatch.setattr(pipeline, "run_powershell", cli)
    monkeypatch.setattr(pipeline, "run_node", node)
    result = pipeline.execute_app(entry, "wasm_sim_standard")
    assert result.success is (failure is None), result.message
    candidate = json.loads(result.candidate_path.read_text(encoding="utf-8"))
    assert candidate["fault_handling_complete"] is (failure is None)
    assert candidate["restoration_accepted"] is (failure != "recovery_runner_error")
    run_app = result.candidate_path.parent / "app/uart_uart_events"
    assert file_sha256(run_app / "uart_events_example_main.c") == UPSTREAM_SHA256
    assert all(file.read_bytes() == content for file, content in originals.items())
    assert not list(vendor.rglob("twin-proof.json"))
