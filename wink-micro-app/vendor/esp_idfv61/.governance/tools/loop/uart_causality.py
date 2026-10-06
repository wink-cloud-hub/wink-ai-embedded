# SPDX-License-Identifier: GPL-3.0-only
"""Candidate UART causality checks; no audit or formal promotion."""
from __future__ import annotations

import copy
import datetime
import difflib
import json
import shutil
import uuid
from pathlib import Path

from .pipeline import LoopPipeline, PipelineResult
from evidence_verifier import compute_assets_composite_sha256
from report_contract import file_sha256, validate_scenario_report

SOURCE_FILE = "uart_echo_example_main.c"
UPSTREAM_SHA256 = "573aac433eaf223af1b12d924e2997012d57e8fc579d918250f3c4221df01253"
TX_CALL = "uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) data, len);"


class UartCausalityPipeline(LoopPipeline):
    """Only the pinned, unmodified upstream UART Echo is eligible."""

    def execute_app(self, app_entry, config_id=None):
        app_id = app_entry.get("id", "app")
        candidate_path = None
        candidate = {}

        def save():
            temporary = candidate_path.with_suffix(".json.tmp")
            temporary.write_text(json.dumps(candidate, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            temporary.replace(candidate_path)

        def finish(stage, message, status="failed"):
            if candidate_path is not None:
                candidate.update(status=status, stage=stage, message=message)
                save()
            return PipelineResult(app_id, False, message, stage, candidate_path)

        try:
            execution = self.select_execution(app_entry, config_id)
            if (app_id != "esp.peripherals.uart.uart_echo" or
                    app_entry.get("target_app_dir") != "peripherals/uart_echo" or
                    execution["target_soc"] != "esp32" or self.auto_heal):
                raise ValueError("UART causality supports only the registered ESP32 UART Echo")
            original_app = self.vendor_root / "peripherals/uart_echo"
            original_scenario = (self.vendor_root / execution["acceptance"]["scenario_path"]).resolve()
            if not original_scenario.is_relative_to((original_app / "unisim-scenarios").resolve()):
                raise ValueError("Acceptance scenario is outside UART Echo")
            app_manifest = json.loads((original_app / "wink-app.json").read_text(encoding="utf-8"))
            if (app_manifest["upstream"]["files"].get(SOURCE_FILE) != UPSTREAM_SHA256 or
                    file_sha256(original_app / SOURCE_FILE) != UPSTREAM_SHA256):
                raise ValueError("Pinned upstream UART source hash mismatch; no source mutation was applied")
            scene = json.loads(original_scenario.read_text(encoding="utf-8"))
            if (len(scene["steps"]) != 3 or scene["steps"][1].get("busId") != 1 or
                    scene["steps"][1].get("type") != "INPUT_BUS" or
                    scene["steps"][1].get("payload") != {"encoding": "utf8", "text": "HELLO_ESP32_WINK"} or
                    scene["steps"][2].get("type") != "ASSERT_BUS_PAYLOAD" or
                    scene["steps"][2].get("busType") != "uart" or
                    scene["steps"][2].get("busId") != 1 or scene["steps"][2].get("direction") != "tx" or
                    scene["steps"][2].get("matcher") != "HELLO_ESP32_WINK" or
                    scene["steps"][2].get("windowUs") != ["100ms", "1000ms"]):
                raise ValueError("UART proof profile requires the pinned UART1 Echo acceptance contract")
            manifest = json.loads(self.manifest_path.read_text(encoding="utf-8"))
            if [item for item in manifest["entries"] if item.get("id") == app_id] != [app_entry]:
                raise ValueError("Checklist entry changed or is not uniquely registered")
            if self.dry_run:
                return PipelineResult(app_id, True,
                                      "UART candidate: baseline, assertion self-check, firmware dependency, "
                                      "business mutation, RX characterization, recovery; fault contract requires review",
                                      "PLANNED")

            run_id = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex
            run_root = self.governance_dir / "runs" / run_id
            run_root.mkdir(parents=True, exist_ok=False)
            candidate_path = run_root / "candidate_evidence.json"
            run_app = run_root / "app/uart_echo"
            original_hash = self.input_hash(original_app)
            shutil.copytree(original_app, run_app,
                            ignore=shutil.ignore_patterns("unisim-assets", "build", "build-*", "__pycache__", ".git"))
            scene_path = run_app / original_scenario.relative_to(original_app.resolve())
            source_path = run_app / SOURCE_FILE
            original_bytes = source_path.read_bytes()
            source_text = original_bytes.decode("utf-8")
            if source_text.count(TX_CALL) != 1:
                raise ValueError("UART mutation requires exactly one upstream echo TX call")
            input_hash = self.input_hash(run_app)
            candidate = {
                "format_version": 1, "kind": "candidate_evidence", "proof_profile": "uart-causality",
                "status": "collecting", "run_id": run_id, "app_id": app_id,
                "target_app_dir": app_entry["target_app_dir"], "config_id": execution["config_id"],
                "execution": {key: execution[key] for key in ("backend", "target_soc", "profile")},
                "runner_mode": "headless", "runner_sha256": file_sha256(self.runner_script),
                "process_isolation": "A new CLI/engine process for each phase; no shared VM instance",
                "original_input_sha256": original_hash, "input_sha256": input_hash,
                "source_original_sha256": UPSTREAM_SHA256, "checks": [], "causality_complete": False,
                "fault_handling": {
                    "status": "contract_gap",
                    "registered_cases": copy.deepcopy(execution["acceptance"].get("negative_cases", [])),
                    "reason": "Upstream Echo disables the event queue and has no runtime RX/TX error branch. "
                              "Buffer overflow is not an ESP_ERR_NO_MEM return from uart_read_bytes. "
                              "A changed valid string is not a framing fault. RX characterization is separate evidence.",
                },
                "limitations": ["Candidate only; independent audit and formal promotion have not been performed.",
                                "Only application inputs are isolated; runtime sources and build caches remain shared.",
                                "RX boundary characterization does not prove application fault handling or physical timing."],
            }
            (run_root / "checklist-entry.json").write_text(json.dumps(app_entry, indent=2, ensure_ascii=False), encoding="utf-8")
            save()
            gate_rc, output = self.run_python(self.governance_dir / "gates/run_gates.py", ["--gate", "1"])
            (run_root / "gate1.log").write_text(output, encoding="utf-8")
            candidate["workspace_gate1_exit_code"] = gate_rc
            if gate_rc:
                return finish("GATE_1_PRE", "Workspace Gate 1 failed")

            def collect(kind, scenario):
                phase = run_root / "reports" / kind
                phase.mkdir(parents=True, exist_ok=False)
                captured_source = phase / SOURCE_FILE
                captured_scenario = phase / "scenario.json"
                shutil.copyfile(source_path, captured_source)
                shutil.copyfile(scenario, captured_scenario)
                record = {
                    "kind": kind, "run_id": f"{run_id}-{kind}", "config_id": execution["config_id"],
                    **{key: execution[key] for key in ("backend", "target_soc", "profile")},
                    "input_sha256": self.input_hash(run_app),
                    "source_path": str(captured_source), "source_sha256": file_sha256(captured_source),
                    "scenario_path": str(captured_scenario), "scenario_sha256": file_sha256(captured_scenario),
                    "report_path": str(phase / "run-report.json"), "report_sha256": None,
                    "assets_path": str(phase / "assets"), "assets_sha256": None, "firmware_sha256": None,
                }
                if record["source_sha256"] != UPSTREAM_SHA256:
                    patch = "".join(difflib.unified_diff(source_text.splitlines(keepends=True),
                                                       captured_source.read_text(encoding="utf-8").splitlines(keepends=True),
                                                       fromfile=SOURCE_FILE, tofile=SOURCE_FILE))
                    patch_path = phase / "source.patch"
                    patch_path.write_text(patch, encoding="utf-8")
                    record.update(source_patch_path=str(patch_path), source_patch_sha256=file_sha256(patch_path))
                args = ["-File", str(self.runner_script), "-App", str(run_app), "-Scenario", str(scenario),
                        "-ArtifactsDir", str(phase), "-Reporter", "json"]
                rc, output = self.run_powershell(args)
                (phase / "runner.log").write_text(output, encoding="utf-8")
                record.update(exit_code=rc, command=args)
                if Path(record["report_path"]).is_file():
                    record["report_sha256"] = file_sha256(Path(record["report_path"]))
                assets = run_app / "unisim-assets"
                if assets.is_dir():
                    shutil.copytree(assets, phase / "assets")
                    try:
                        record["assets_sha256"] = compute_assets_composite_sha256(phase / "assets")
                        record["firmware_sha256"] = file_sha256(phase / "assets/wink_simulator.wasm")
                    except OSError as exc:
                        record["asset_binding_error"] = str(exc)
                if (self.input_hash(run_app) != record["input_sha256"] or
                        file_sha256(scenario) != record["scenario_sha256"]):
                    record["input_binding_error"] = "Application or scenario inputs changed during execution"
                candidate["checks"].append(record)
                save()
                print(f"[uart] {kind}: exit {rc}; report {record['report_path']}", flush=True)
                return record

            def validate(record, failure_index=None):
                expected_rc = 0 if failure_index is None else 1
                if (record["exit_code"] != expected_rc or not record["assets_sha256"] or
                        record.get("input_binding_error") or not record["report_sha256"]):
                    return False, "Execution did not finish with the required exit code and bound inputs/assets/report"
                tree = json.loads((Path(record["assets_path"]) / "device-tree.json").read_text(encoding="utf-8"))
                if tree.get("mcu") != execution["target_soc"]:
                    return False, "Built target_soc differs from the selected configuration"
                return validate_scenario_report(Path(record["report_path"]), Path(record["scenario_path"]), failure_index)

            def verdict(record, ok, reason):
                record.update(accepted=ok, verdict=reason)
                save()

            baseline = collect("baseline", scene_path)
            ok, reason = validate(baseline)
            verdict(baseline, ok, reason)
            if not ok:
                return finish("BASELINE", reason)
            candidate["assets_sha256"] = baseline["assets_sha256"]

            try:
                mutant_path, meta = self.mutator.create_mutant_file(scene_path, output_dir=run_root / "mutants")
                if mutant_path is None:
                    raise ValueError(meta.get("error", "No assertion self-check could be created"))
                self_check = collect("assertion_self_check", mutant_path)
                ok, reason = validate(self_check, 2)
                if self_check["assets_sha256"] != baseline["assets_sha256"]:
                    ok, reason = False, "Assertion self-check changed firmware assets"
                verdict(self_check, ok, reason)
                candidate["assertion_mutation"] = meta

                mutations = {
                    "firmware_dependency": "(void) data; /* candidate probe: suppress echo TX */",
                    "business_mutation": "if (len > 0) { data[0] ^= 1u; }\n        " + TX_CALL,
                }
                for kind, replacement in mutations.items():
                    source_path.write_bytes(source_text.replace(TX_CALL, replacement).encode("utf-8"))
                    record = collect(kind, scene_path)
                    ok, reason = validate(record, 2)
                    if record["firmware_sha256"] == baseline["firmware_sha256"]:
                        ok, reason = False, "Source mutation did not change the executed firmware hash"
                    if ok:
                        report = json.loads(Path(record["report_path"]).read_text(encoding="utf-8"))
                        actual = report["results"][0]["stepResults"][2]["actual"]
                        record["observed_tx"] = actual
                        expected_payloads = [] if kind == "firmware_dependency" else ["IELLO_ESP32_WINK"]
                        if (not isinstance(actual, dict) or actual.get("candidatePayloads") != expected_payloads or
                                type(actual.get("matchedCount")) is not int or actual["matchedCount"] != 0):
                            ok, reason = False, "Actual TX does not match the declared source mutation"
                    verdict(record, ok, reason)

                source_path.write_bytes(original_bytes)
                rx_scene = copy.deepcopy(scene)
                rx_scene["header"].update(name=f"UART spaced RX characterization {run_id}", templateId=f"uart_rx_{run_id}")
                rx_scene["steps"] = [copy.deepcopy(scene["steps"][0])]
                for index, letter in enumerate("ABCD"):
                    time_ms = 100 + index * 300
                    payload = letter * 96
                    rx_scene["steps"].extend([
                        {"type": "INPUT_BUS", "timeUs": f"{time_ms}ms", "mode": "stream", "busType": "uart",
                         "busId": 1, "payload": {"encoding": "utf8", "text": payload}},
                        {"type": "ASSERT_BUS_PAYLOAD", "windowUs": [f"{time_ms}ms", f"{time_ms + 150}ms"],
                         "busType": "uart", "busId": 1, "direction": "tx", "matcher": payload},
                    ])
                rx_path = run_root / "rx-characterization.json"
                rx_path.write_text(json.dumps(rx_scene, indent=2) + "\n", encoding="utf-8")
                rx = collect("rx_characterization", rx_path)
                ok, reason = validate(rx)
                rx.update(observation_valid=ok, outcome="passed" if ok else "untrusted_failure")
                if not ok and rx["report_sha256"]:
                    data = json.loads(Path(rx["report_path"]).read_text(encoding="utf-8"))
                    results = data.get("results", [])
                    steps = results[0].get("stepResults", []) if len(results) == 1 else []
                    failures = [step for step in steps if step.get("status") == "failed"]
                    if len(failures) == 1:
                        index = failures[0].get("stepIndex")
                        observed, detail = validate(rx, index)
                        if observed:
                            actual = failures[0]["actual"]
                            rx.update(observation_valid=True, outcome="business_regression",
                                      failed_step_index=index, observed_tx=actual, observation_verdict=detail)
                            if index == 6 and isinstance(actual, dict) and actual.get("candidatePayloads") == ["C" * 64]:
                                rx["outcome"] = "cumulative_rx_truncation"
                if rx["assets_sha256"] != baseline["assets_sha256"]:
                    ok, reason = False, "RX characterization unexpectedly changed the baseline firmware assets"
                    rx.update(observation_valid=False, outcome="asset_mismatch")
                verdict(rx, ok, reason)
            finally:
                source_path.write_bytes(original_bytes)
                recovery = collect("recovery", scene_path)
                ok, reason = validate(recovery)
                if recovery["assets_sha256"] != baseline["assets_sha256"]:
                    ok, reason = False, "Recovery assets differ from baseline after restoring the original source"
                verdict(recovery, ok, reason)

            if self.input_hash(original_app) != original_hash or self.input_hash(run_app) != input_hash:
                return finish("INPUT_INTEGRITY", "Original or restored application inputs changed")
            causal = [check for check in candidate["checks"] if check["kind"] != "rx_characterization"]
            candidate["causality_complete"] = len(causal) == 5 and all(check["accepted"] for check in causal)
            for check in causal:
                if not check["accepted"]:
                    return finish(check["kind"].upper(), check["verdict"])
            if not rx["accepted"]:
                return finish("UART_RX_BOUNDARY", "Spaced RX characterization failed; inspect the bound report and diagnostics",
                              "candidate_incomplete")
            return finish("UART_CONTRACT", "Causal checks accepted; declared UART overflow fault-handling contract remains unproven",
                          "candidate_incomplete")
        except (OSError, ValueError, KeyError, TypeError, IndexError) as exc:
            return finish("COLLECTION" if candidate_path else "CONFIGURATION", str(exc))
