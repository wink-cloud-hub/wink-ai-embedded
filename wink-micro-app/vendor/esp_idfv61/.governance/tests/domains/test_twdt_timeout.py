# SPDX-License-Identifier: GPL-3.0-only
"""Deadline and evidence integrity through the public candidate report/CLI."""
import copy
import json
import subprocess
import sys
from pathlib import Path
import pytest

TOOLS = Path(__file__).resolve().parents[2] / "tools"
from loop.twdt_timeout import make_contract, validate_timeout_report
from gates.report_contract import file_sha256


@pytest.fixture
def timeout_report(tmp_path):
    assets = tmp_path / "assets"
    assets.mkdir()
    for name in ("wink_simulator.wasm", "wink_simulator.js", "device-tree.json"):
        (assets / name).write_text(name, encoding="utf-8")
    harness = TOOLS / "loop/twdt_timeout_harness.cjs"
    contract = tmp_path / "contract.json"
    contract.write_text(json.dumps(make_contract("func_a")), encoding="utf-8")
    report = {"format_version": 1, "kind": "twdt_timeout_report", "run_id": "fixture-a",
              "backend": "wasm_node_abi_harness", "case": "func_a", "same_instance": True,
              "contract_sha256": file_sha256(contract), "harness_sha256": file_sha256(harness),
              "firmware_sha256": file_sha256(assets / "wink_simulator.wasm"),
              "glue_sha256": file_sha256(assets / "wink_simulator.js"),
              "device_tree_sha256": file_sha256(assets / "device-tree.json"),
              "runtime_errors": [], "reset_requested": False, "exit_code": 0, "start_us": 0, "end_us": 16000000,
              "frames": [{"channel": "uart", "time_us": 1000, "text": "TWDT initialized; Subscribed to TWDT"},
                         {"channel": "uart", "time_us": 3001000, "text": "Task watchdog got triggered."},
                         {"channel": "uart", "time_us": 3001000, "text": "did not reset: user 'func_a' (CPU 0)"},
                         {"channel": "uart", "time_us": 12000000, "text": "Unsubscribed from TWDT; TWDT deinitialized; Example complete"}],
              "step_results": [{"index": 0, "type": "automatic_timeout", "status": "passed"},
                               {"index": 1, "type": "unsubscribe_recovery", "status": "passed"}]}
    report_path = tmp_path / "report.json"
    def validate(value, red=False):
        report_path.write_text(json.dumps(value), encoding="utf-8")
        return validate_timeout_report(report_path, contract, assets, harness, "fixture-a", red)
    return report, validate, assets


def test_accepts_bounded_timeout_and_live_red(timeout_report):
    report, validate, _ = timeout_report
    assert validate(report)[0]
    report["frames"][1:3] = []
    report["exit_code"] = 1
    report["step_results"][0]["status"] = "failed"
    assert validate(report, red=True)[0]
    assert not validate(report)[0]


@pytest.mark.parametrize("defect", ["early", "late", "wrong_user", "extra_user", "missing_alarm", "incomplete_window",
                                   "post_deinit", "reset", "runtime_error", "stale", "missing_lifecycle", "wrong_hash",
                                   "duplicate_step", "bool_index", "bool_time", "out_of_order"])
def test_rejects_wrong_timeout_and_forged_evidence(timeout_report, defect):
    report, validate, assets = timeout_report
    if defect == "early": report["frames"][1]["time_us"] = 3000999
    elif defect == "late": report["frames"][1]["time_us"] = 3051001
    elif defect == "wrong_user": report["frames"][2]["text"] = "did not reset: user 'func_b' (CPU 0)"
    elif defect == "extra_user": report["frames"].insert(3, copy.deepcopy(report["frames"][2]))
    elif defect == "missing_alarm": report["frames"][1:3] = []
    elif defect == "incomplete_window": report["end_us"] = 15999999
    elif defect == "post_deinit": report["frames"][1:3] = [dict(f, time_us=12500000) for f in report["frames"][1:3]]
    elif defect == "reset": report["reset_requested"] = True
    elif defect == "runtime_error": report["runtime_errors"] = ["RuntimeError"]
    elif defect == "stale": report["run_id"] = "stale"
    elif defect == "missing_lifecycle": report["frames"][-1]["text"] = "Example complete"
    elif defect == "wrong_hash": (assets / "wink_simulator.js").write_text("different", encoding="utf-8")
    elif defect == "duplicate_step": report["step_results"].append(report["step_results"][0])
    elif defect == "bool_index": report["step_results"][0]["index"] = False
    elif defect == "bool_time": report["start_us"] = False
    elif defect == "out_of_order": report["frames"] = list(reversed(report["frames"]))
    assert not validate(report)[0]


def test_red_cannot_accept_observed_alarm_or_failed_recovery(timeout_report):
    report, validate, _ = timeout_report
    report["exit_code"] = 1
    report["step_results"][0]["status"] = "failed"
    assert not validate(report, red=True)[0]
    report["frames"][1:3] = []
    report["step_results"][1]["status"] = "failed"
    assert not validate(report, red=True)[0]


@pytest.mark.parametrize("args", [[], ["--app", "does_not_exist"], ["--app", "uart_echo"]])
def test_twdt_cli_rejects_missing_or_wrong_scope(args):
    result = subprocess.run([sys.executable, "-X", "utf8", "-B", str(TOOLS / "run_loop.py"),
                             "--proof-profile", "twdt-timeout", "--dry-run", *args],
                            capture_output=True, text=True, encoding="utf-8", timeout=30)
    assert result.returncode != 0


def test_twdt_cli_dry_run_is_read_only():
    result = subprocess.run([sys.executable, "-X", "utf8", "-B", str(TOOLS / "run_loop.py"),
                             "--proof-profile", "twdt-timeout", "--app", "task_watchdog", "--dry-run"],
                            capture_output=True, text=True, encoding="utf-8", timeout=30)
    assert result.returncode == 0, result.stderr
    assert "three feed omissions" in result.stdout
