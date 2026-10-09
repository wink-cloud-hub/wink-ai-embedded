# SPDX-License-Identifier: GPL-3.0-only
"""UART Events candidate fault handling through the production public Wasm ABI."""
from __future__ import annotations

import difflib
import json
import re
import shutil
import subprocess
from pathlib import Path

try:
    from loop.pipeline.pipeline import LoopPipeline, PipelineResult
except ImportError:
    try:
        from ...pipeline.pipeline import LoopPipeline, PipelineResult
    except ImportError:
        from .pipeline import LoopPipeline, PipelineResult

try:
    from gates.evidence_verifier import compute_assets_composite_sha256
    from gates.report_contract import file_sha256, validate_scenario_report
except ImportError:
    from evidence_verifier import compute_assets_composite_sha256
    from report_contract import file_sha256, validate_scenario_report


SOURCE_FILE = "uart_events_example_main.c"
UPSTREAM_SHA256 = "8d173ac3cd0e1a9fec6dc1bf6de48a865172a43cc5f910635c63e00dbef770de"
FLAGS = {"control": 0, "frame": 1, "parity": 2, "fifo": 4}
ERROR_TEXT = {"frame": "uart frame error", "parity": "uart parity error", "fifo": "hw fifo overflow"}
ANSI = re.compile(r"\x1b\[[0-9;]*m")


def make_contract(case):
    return {"format_version": 1, "kind": "uart_events_fault_contract", "case": case,
            "port": 0, "flags": FLAGS[case], "window_us": 500000,
            "before_payload": f"PRE_{case.upper()}_WINK", "recovery_payload": f"POST_{case.upper()}_WINK"}


def validate_fault_report(report_path, contract_path, assets_path, harness_path, run_id,
                          expected_fault_failure=False):
    """Recompute behavior from bounded UART TX frames; never trust status alone."""
    try:
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
        case = contract["case"]
        canonical = make_contract(case)
        if contract != canonical or any(type(contract[key]) is not type(value) for key, value in canonical.items()):
            raise ValueError("Fault contract differs from the pinned public ABI contract")
        report = json.loads(report_path.read_text(encoding="utf-8"))
        identity = {"format_version": 1, "kind": "uart_events_fault_report", "run_id": run_id,
                    "backend": "wasm_node_abi_harness", "case": case,
                    "contract_sha256": file_sha256(contract_path), "harness_sha256": file_sha256(harness_path),
                    "firmware_sha256": file_sha256(assets_path / "wink_simulator.wasm"),
                    "glue_sha256": file_sha256(assets_path / "wink_simulator.js"),
                    "device_tree_sha256": file_sha256(assets_path / "device-tree.json"), "same_instance": True,
                    "runtime_errors": [], "exit_code": 1 if expected_fault_failure else 0}
        if any(report.get(key) != value or type(report.get(key)) is not type(value)
               for key, value in identity.items()):
            raise ValueError("Report identity, assets, runtime or exit status mismatch")
        steps = report["step_results"]
        if len(steps) != 3 or [step["index"] for step in steps] != [0, 1, 2]:
            raise ValueError("Normal, fault and same-instance recovery steps are required exactly once")
        previous_end = 0
        for index, step in enumerate(steps):
            start, end = step["start_us"], step["end_us"]
            if (type(step["index"]) is not int or type(start) is not int or type(end) is not int or
                    not previous_end <= start <= end <= start + contract["window_us"] + 11000):
                raise ValueError("Invalid or unordered virtual time bounds")
            previous_end = end
            expected_type = ["normal_echo", "fault_handling", "same_instance_recovery"][index]
            if step["type"] != expected_type:
                raise ValueError("Step type mismatch")
            echo, lines = "", []
            for frame in step["tx_frames"]:
                if (type(frame["port"]) is not int or frame["port"] != 0 or
                        type(frame["time_us"]) is not int or not start <= frame["time_us"] <= end or
                        frame["time_us"] > start + contract["window_us"] or
                        not isinstance(frame["hex"], str) or
                        re.fullmatch(r"(?:[0-9a-f]{2})+", frame["hex"]) is None):
                    raise ValueError("UART TX frame is malformed, late or on another port")
                if len(frame["hex"]) == 2:
                    echo += frame["hex"]
                line = ANSI.sub("", bytes.fromhex(frame["hex"]).decode("utf-8", errors="replace"))
                if any(f"uart_events: {text}" in line for text in ERROR_TEXT.values()):
                    lines.append(line)
            if index == 1:
                text = ERROR_TEXT.get(case)
                expected = {"fault_log": text, "fault_log_count": 0 if case == "control" else 1}
                passed = not echo and (not lines if case == "control" else
                                      len(lines) == 1 and f"uart_events: {text}" in lines[0])
                if case == "control" and end < start + contract["window_us"]:
                    raise ValueError("No-fault control must observe the full window")
            else:
                payload = contract["before_payload" if index == 0 else "recovery_payload"]
                expected = {"echo_hex": payload.encode().hex(), "fault_log_count": 0}
                passed = echo == expected["echo_hex"] and not lines and end <= start + contract["window_us"]
            if step["expected"] != expected or step["status"] != ("passed" if passed else "failed"):
                raise ValueError("Declared result contradicts raw UART TX")
            if index == 1 and expected_fault_failure and (lines or end < start + contract["window_us"]):
                raise ValueError("Handler removal must miss the error log throughout the full observation window")
            if passed != (not expected_fault_failure or index != 1):
                raise ValueError("Required fault assertion failure or successful behavior was not observed")
        injection = {"function": "pal_wasm_push_uart_rx_error", "port": 0,
                     "flags": FLAGS[case], "time_us": steps[1]["start_us"]}
        if report["injections"] != [injection] or any(type(report["injections"][0][key]) is not type(value)
                                                      for key, value in injection.items()):
            raise ValueError("Missing or mismatched one-shot injection")
        return True, "Bound UART fault handling and same-instance recovery accepted" if not expected_fault_failure else \
            "Only the designated firmware fault-handling assertion failed; normal and recovery echo passed"
    except (OSError, ValueError, KeyError, TypeError, IndexError) as exc:
        return False, str(exc)


class UartEventsFaultPipeline(LoopPipeline):
    """Reuse the normal CLI collector, then bind isolated ABI fault evidence."""

    def run_node(self, args):
        try:
            result = subprocess.run(["node", *args], cwd=self.ws_root, capture_output=True,
                                    text=True, encoding="utf-8", errors="replace", timeout=30)
            return result.returncode, result.stdout + result.stderr
        except (OSError, subprocess.TimeoutExpired) as exc:
            return 124, str(exc)

    def execute_app(self, app_entry, config_id=None):
        app_id = app_entry.get("id", "app")
        try:
            execution = self.select_execution(app_entry, config_id)
            original_app = self.vendor_root / "peripherals/uart_uart_events"
            if (app_id != "esp.peripherals.uart.uart_events" or
                    app_entry.get("target_app_dir") != "peripherals/uart_uart_events" or
                    execution["config_id"] != "wasm_sim_standard" or execution["target_soc"] != "esp32" or
                    self.auto_heal):
                raise ValueError("UART Events fault profile requires the registered standard ESP32 configuration")
            manifest = json.loads((original_app / "wink-app.json").read_text(encoding="utf-8"))
            if (manifest["upstream"]["files"].get(SOURCE_FILE) != UPSTREAM_SHA256 or
                    file_sha256(original_app / SOURCE_FILE) != UPSTREAM_SHA256):
                raise ValueError("Pinned upstream UART Events source hash mismatch")
            if self.dry_run:
                return PipelineResult(app_id, True, "UART Events: normal CLI, fault propagation, "
                                      "handler removal and same-instance recovery; independent ABI harness", "PLANNED")
        except (OSError, ValueError, KeyError, TypeError) as exc:
            return PipelineResult(app_id, False, str(exc), "CONFIGURATION")

        base = super().execute_app(app_entry, config_id)
        if not base.success:
            return base
        candidate_path = base.candidate_path
        root = candidate_path.parent
        candidate = json.loads(candidate_path.read_text(encoding="utf-8"))
        run_app = root / "app/uart_uart_events"
        source = run_app / SOURCE_FILE
        original_bytes = source.read_bytes()
        original_input = self.input_hash(original_app)
        scene = run_app / "unisim-scenarios" / Path(execution["acceptance"]["scenario_path"]).name
        harness = root / "uart_events_fault_harness.cjs"
        shutil.copyfile(Path(__file__).with_name(harness.name), harness)
        candidate.update(proof_profile="uart-events-fault", status="collecting", stage="UART_EVENTS_FAULT",
                         fault_builds=[], fault_checks=[], fault_handling_complete=False,
                         fault_scope="One-shot PAL events through the public Wasm ABI; no line/parity timing model",
                         harness_path=str(harness), harness_sha256=file_sha256(harness),
                         upstream_source_sha256=UPSTREAM_SHA256,
                         formal_contract_gaps=["Native UART fault scenario contract is not registered",
                                               "Independent audit is required for formal delivery"])

        def save():
            temporary = candidate_path.with_suffix(".json.tmp")
            temporary.write_text(json.dumps(candidate, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            temporary.replace(candidate_path)

        def snapshot(kind):
            phase = root / "fault-builds" / kind
            phase.mkdir(parents=True, exist_ok=False)
            shutil.copyfile(source, phase / SOURCE_FILE)
            shutil.copyfile(scene, phase / "scenario.json")
            shutil.copytree(run_app / "unisim-assets", phase / "assets")
            record = {"kind": kind, "source_path": str(phase / SOURCE_FILE),
                      "source_sha256": file_sha256(phase / SOURCE_FILE),
                      "scenario_path": str(phase / "scenario.json"), "scenario_sha256": file_sha256(phase / "scenario.json"),
                      "input_sha256": self.input_hash(run_app), "assets_path": str(phase / "assets"),
                      "assets_sha256": compute_assets_composite_sha256(phase / "assets"),
                      "firmware_sha256": file_sha256(phase / "assets/wink_simulator.wasm")}
            if kind != "baseline":
                build_dir = root / "reports" / f"events_{kind}"
                record.update(report_path=str(build_dir / "run-report.json"),
                              report_sha256=file_sha256(build_dir / "run-report.json"))
            if record["source_sha256"] != UPSTREAM_SHA256:
                patch = "".join(difflib.unified_diff(original_bytes.decode().splitlines(keepends=True),
                                                       source.read_text(encoding="utf-8").splitlines(keepends=True),
                                                       fromfile=SOURCE_FILE, tofile=SOURCE_FILE))
                (phase / "source.patch").write_text(patch, encoding="utf-8")
                record.update(source_patch_path=str(phase / "source.patch"), source_patch_sha256=file_sha256(phase / "source.patch"))
            tree = json.loads((phase / "assets/device-tree.json").read_text(encoding="utf-8"))
            if tree.get("mcu") != "esp32":
                raise ValueError("Actual production asset target differs from ESP32")
            candidate["fault_builds"].append(record)
            save()
            return record

        def build(kind):
            phase = root / "reports" / f"events_{kind}"
            phase.mkdir(parents=True, exist_ok=False)
            before = self.input_hash(run_app)
            args = ["-File", str(self.runner_script), "-App", str(run_app), "-Scenario", str(scene),
                    "-ArtifactsDir", str(phase), "-Reporter", "json"]
            rc, output = self.run_powershell(args)
            (phase / "runner.log").write_text(output, encoding="utf-8")
            ok, reason = validate_scenario_report(phase / "run-report.json", scene)
            if rc != 0 or not ok or self.input_hash(run_app) != before:
                raise ValueError(f"{kind} normal CLI build/run failed: exit {rc}; {reason}")
            record = snapshot(kind)
            record.update(exit_code=rc, command=args, accepted=True)
            save()
            return record

        def fault_checks(kind, firmware, mutant=False):
            accepted = True
            for case in FLAGS:
                phase = root / "fault-reports" / kind / case
                phase.mkdir(parents=True, exist_ok=False)
                contract = phase / "fault.contract.json"
                contract.write_text(json.dumps(make_contract(case), indent=2) + "\n", encoding="utf-8")
                report = phase / "fault.report.json"
                run_id = f"{candidate['run_id']}-{kind}-{case}"
                args = [str(harness), "--assets", firmware["assets_path"], "--contract", str(contract),
                        "--report", str(report), "--run-id", run_id]
                rc, output = self.run_node(args)
                (phase / "runner.log").write_text(output, encoding="utf-8")
                expected_failure = mutant and case != "control"
                ok, reason = validate_fault_report(report, contract, Path(firmware["assets_path"]), harness,
                                                   run_id, expected_failure)
                expected_rc = 1 if expected_failure else 0
                if rc != expected_rc:
                    ok, reason = False, f"Node exit {rc} differs from the required assertion outcome {expected_rc}: {reason}"
                record = {"kind": kind, "case": case, "run_id": run_id, "command": ["node", *args],
                          "exit_code": rc, "accepted": ok, "verdict": reason,
                          "backend": "wasm_node_abi_harness", "config_id": execution["config_id"],
                          "target_soc": "esp32", "profile": execution["profile"],
                          "contract_path": str(contract), "contract_sha256": file_sha256(contract),
                          "report_path": str(report), "report_sha256": file_sha256(report) if report.is_file() else None,
                          "expected_failed_step": 1 if expected_failure else None,
                          **{key: firmware[key] for key in ("source_path", "source_sha256", "assets_path",
                                                           "assets_sha256", "firmware_sha256")}}
                candidate["fault_checks"].append(record)
                accepted = accepted and ok
                save()
                print(f"[uart-events] {kind}/{case}: exit {rc}; accepted {ok}; {report}", flush=True)
            return accepted

        failure = None
        baseline = None
        save()
        try:
            baseline = snapshot("baseline")
            candidate["assets_path"] = baseline["assets_path"]
            # All three normal CLI phases used identical assets. Freeze that
            # validated asset set before any isolated source mutant rebuild.
            for record in candidate["checks"]:
                if record["assets_sha256"] != baseline["assets_sha256"]:
                    raise ValueError("Normal CLI asset sets were not identical")
                record["assets_path"] = baseline["assets_path"]
            if not fault_checks("baseline", baseline):
                failure = ("FAULT_HANDLING", "Production UART error handling did not satisfy the fault contract")
            else:
                mutant = original_bytes.decode("utf-8")
                for label in ("UART_FIFO_OVF", "UART_PARITY_ERR", "UART_FRAME_ERR"):
                    mutant, count = re.subn(r"(case " + label + r":).*?(\n\s*break;)",
                                           r"\1\n                    /* Isolated fault-handler removal. */\2", mutant,
                                           flags=re.DOTALL)
                    if count != 1:
                        raise ValueError(f"Pinned mutation requires one {label} branch")
                source.write_text(mutant, encoding="utf-8")
                mutated = build("handler_removal")
                if mutated["firmware_sha256"] == baseline["firmware_sha256"]:
                    raise ValueError("Fault mutation used unchanged/stale firmware")
                if not fault_checks("handler_removal", mutated, mutant=True):
                    failure = ("FAULT_MUTATION", "Handler removal did not fail only the designated fault assertion")
        except (OSError, ValueError, KeyError, TypeError, shutil.Error) as exc:
            failure = ("COLLECTION", str(exc))
        finally:
            try:
                source.write_bytes(original_bytes)
                restored = build("restored")
                if (baseline is None or restored["assets_sha256"] != baseline["assets_sha256"] or
                        restored["source_sha256"] != UPSTREAM_SHA256):
                    raise ValueError("Restored production assets/source differ from the baseline")
                if not fault_checks("restored", restored):
                    raise ValueError("Restored firmware fault handling or same-instance recovery failed")
                candidate["restoration_accepted"] = True
            except (OSError, ValueError, KeyError, TypeError, shutil.Error) as exc:
                candidate["restoration_accepted"] = False
                candidate["restoration_error"] = str(exc)
                if failure is None:
                    failure = ("RESTORATION", str(exc))
            save()
        if self.input_hash(original_app) != original_input or self.input_hash(run_app) != candidate["input_sha256"]:
            failure = ("INPUT_INTEGRITY", "Original UART Events inputs changed during collection")
        if failure:
            candidate.update(status="failed", stage=failure[0], message=failure[1])
        else:
            candidate.update(status="candidate_ready", stage="CANDIDATE", fault_handling_complete=True,
                             message="UART Events fault propagation, handler removal and same-instance recovery accepted")
        save()
        return PipelineResult(app_id, failure is None, candidate["message"], candidate["stage"], candidate_path)
