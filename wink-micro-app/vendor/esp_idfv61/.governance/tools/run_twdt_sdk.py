# SPDX-License-Identifier: GPL-3.0-only
"""Build a public SDK test adapter in an existing TWDT candidate's isolated namespace."""
import argparse
import json
import shutil
import subprocess
import uuid
from pathlib import Path
from loop.pipeline import LoopPipeline
from report_contract import file_sha256, validate_scenario_report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--case", choices=["panic", "shared", "handles", "all"], required=True)
    args = parser.parse_args()
    workspace = Path(__file__).resolve().parents[5]
    pipeline = LoopPipeline(workspace_root=workspace)
    candidate_path = args.candidate.resolve()
    if not candidate_path.is_relative_to(pipeline.governance_dir / "runs") or candidate_path.name != "candidate_evidence.json":
        parser.error("A UUID timeout candidate is required")
    candidate = json.loads(candidate_path.read_text(encoding="utf-8"))
    if candidate.get("app_id") != "esp.system.task_watchdog" or candidate.get("proof_profile") != "twdt-timeout":
        parser.error("Not a TWDT timeout candidate")
    # Keep the full object path below the Windows compiler's 260-character limit.
    root = pipeline.governance_dir / "runs" / ("sdk-" + uuid.uuid4().hex)
    app = root / "app/task_watchdog"
    original = pipeline.vendor_root / "system/task_watchdog"
    original_input = pipeline.input_hash(original)
    shutil.copytree(original, app, ignore=shutil.ignore_patterns("unisim-assets", "build", "__pycache__"))
    probe = workspace / "wink-micro-os/frameworks/esp_idf/test/wasm/twdt_sdk_probe.c"
    shutil.copyfile(probe, app / "task_watchdog_example_main.c")
    harness = root / "twdt_sdk_harness.cjs"
    shutil.copyfile(probe.with_name(harness.name), harness)
    scene = app / "unisim-scenarios/twdt_sdk_probe.scenario.json"
    scene.write_text(json.dumps({"header": {"version": "1.0.0", "name": "Public SDK TWDT adapter readiness",
        "templateId": "esp_idfv61_system_task_watchdog",
        "accuracyMode": "behavioral", "timeoutUs": "1000ms", "failurePolicy": "fail-fast"},
        "steps": [{"type": "ASSERT_BUS_PAYLOAD", "windowUs": ["0ms", "500ms"], "busType": "uart",
                   "busId": 0, "direction": "tx", "matcher": "TWDT SDK probe ready"}]}, indent=2) + "\n", encoding="utf-8")
    build_report = root / "build-report"
    build_report.mkdir()
    command = ["-File", str(pipeline.runner_script), "-App", str(app), "-Scenario", str(scene),
               "-ArtifactsDir", str(build_report), "-Reporter", "json"]
    runtime_sources = []
    for relative in ("frameworks/esp_idf/src/core/esp_task_wdt.c", "frameworks/esp_idf/src/esp_idf_bridge.c",
                     "frameworks/esp_idf/src/freertos/freertos_timers.c", "osal/wasm/pal_osal_wasm.c",
                     "targets/common/src/wink_sim_scheduler.c"):
        live = workspace / "wink-micro-os" / relative
        frozen = root / "runtime-sources" / relative
        frozen.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(live, frozen)
        runtime_sources.append({"path": str(live), "snapshot_path": str(frozen), "sha256": file_sha256(frozen)})
    before = pipeline.input_hash(app)
    rc, output = pipeline.run_powershell(command)
    (build_report / "runner.log").write_text(output, encoding="utf-8")
    assets = app / "unisim-assets"
    report = root / f"{args.case}.report.json"
    record = {"kind": "twdt_sdk_candidate", "case": args.case, "run_id": root.name,
              "timeout_candidate_path": str(candidate_path), "timeout_candidate_sha256": file_sha256(candidate_path),
              "adapter_path": str(app / "task_watchdog_example_main.c"),
              "adapter_sha256": file_sha256(app / "task_watchdog_example_main.c"),
              "collector_sha256": file_sha256(Path(__file__)),
              "harness_path": str(harness), "harness_sha256": file_sha256(harness),
              "input_sha256": before, "runtime_sources": runtime_sources,
              "build_exit_code": rc, "build_command": command,
              "build_report_path": str(build_report / "run-report.json"), "assets_path": str(assets),
              "accepted": False}
    try:
        ok, reason = validate_scenario_report(build_report / "run-report.json", scene)
        if rc != 0 or not ok or before != pipeline.input_hash(app):
            raise ValueError(f"SDK adapter build/readiness failed: exit {rc}; {reason}")
        if any(file_sha256(Path(item["path"])) != item["sha256"] for item in runtime_sources):
            raise ValueError("Production runtime input changed during the SDK build")
        if json.loads((assets / "device-tree.json").read_text(encoding="utf-8")).get("mcu") != "esp32":
            raise ValueError("SDK adapter assets do not target ESP32")
        record["build_report_sha256"] = file_sha256(build_report / "run-report.json")
        record["firmware_sha256"] = file_sha256(assets / "wink_simulator.wasm")
        record["reports"] = []
        for case in (["panic", "shared", "handles"] if args.case == "all" else [args.case]):
            report = root / f"{case}.report.json"
            command = ["node", str(harness), str(assets), str(report), case]
            result = subprocess.run(command, cwd=workspace, text=True, capture_output=True,
                                    encoding="utf-8", errors="replace", timeout=40)
            (root / f"{case}.runner.log").write_text(result.stdout + result.stderr, encoding="utf-8")
            data = json.loads(report.read_text(encoding="utf-8"))
            accepted = (result.returncode == 0 and data["exit_code"] == 0 and not data["runtime_errors"] and
                data["kind"] == "twdt_sdk_report" and data["backend"] == "wasm_node_abi_harness" and data["case"] == case and
                data["harness_sha256"] == file_sha256(harness) and data["firmware_sha256"] == record["firmware_sha256"] and
                data["glue_sha256"] == file_sha256(assets / "wink_simulator.js") and
                data["device_tree_sha256"] == file_sha256(assets / "device-tree.json") and
                bool(data["checks"]) and all(check["passed"] is True and check["actual"] == check["expected"] and
                                           type(check["actual"]) is type(check["expected"]) for check in data["checks"]))
            record["reports"].append({"case": case, "command": command, "exit_code": result.returncode,
                "accepted": accepted, "report_path": str(report), "report_sha256": file_sha256(report),
                "check_count": len(data["checks"])})
            print(result.stdout, flush=True)
        record["accepted"] = all(item["accepted"] for item in record["reports"])
    except (ValueError, KeyError, OSError, subprocess.TimeoutExpired) as exc:
        record["error"] = str(exc)
    finally:
        record["original_input_unchanged"] = pipeline.input_hash(original) == original_input
        record["accepted"] = record["accepted"] and record["original_input_unchanged"]
        (root / "evidence.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        print(root / "evidence.json", flush=True)
    return 0 if record["accepted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
