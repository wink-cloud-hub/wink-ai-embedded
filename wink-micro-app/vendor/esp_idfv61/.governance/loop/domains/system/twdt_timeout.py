# SPDX-License-Identifier: GPL-3.0-only
"""Isolated TWDT task/user timeout candidates; no audit or formal promotion."""
from __future__ import annotations
import difflib
import json
import shutil
import subprocess
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

from ..base import DomainPlugin

class TwdtDomainPlugin:
    """Standardized TWDT / Watchdog Domain Plugin (GAP-01)."""
    domain_id: str = "system"
    archetype_ref: str = "archetype_system_twdt"
    required_ctx_fields: list[str] = ["app_id", "target_soc", "backend"]

    def build_fixture(self, ctx: Any) -> Any:
        return {"domain_id": self.domain_id, "ctx": ctx}

    def get_causality_graph(self) -> Any:
        return {
            "domain_id": self.domain_id,
            "nodes": [{"id": "task_watchdog"}, {"id": "timeout_interrupt"}, {"id": "panic_handler"}],
            "edges": [
                {"from": "task_watchdog", "to": "timeout_interrupt"},
                {"from": "timeout_interrupt", "to": "panic_handler"},
            ],
        }

    def list_supported_injection_modes(self) -> list[str]:
        return ["task_starve", "func_a_timeout", "func_b_timeout"]


SOURCE = "task_watchdog_example_main.c"
UPSTREAM = "5855b893bf480e818fb8421b23bcbdd50480c9d21a013725de9df14e4e56dc96"
CASES = {"task": "        esp_task_wdt_reset();", "func_a": "        func_a();", "func_b": "        func_b();"}


def make_contract(case):
    if case not in ("control", *CASES):
        raise ValueError("Unknown TWDT case")
    return {"format_version": 1, "kind": "twdt_timeout_contract", "case": case,
            "timeout_us": 3000000, "window_us": 16000000, "tolerance_us": 50000}


def validate_timeout_report(report_path, contract_path, assets, harness, run_id, expected_missing=False):
    """Recompute deadline, culprit and cleanup from timestamped public outputs."""
    try:
        contract = json.loads(contract_path.read_text(encoding="utf-8"))
        canonical = make_contract(contract["case"])
        if contract != canonical or any(type(contract[k]) is not type(v) for k, v in canonical.items()):
            raise ValueError("Contract differs from the pinned TWDT contract")
        report = json.loads(report_path.read_text(encoding="utf-8"))
        identity = {"format_version": 1, "kind": "twdt_timeout_report", "run_id": run_id,
                    "backend": "wasm_node_abi_harness", "case": contract["case"], "same_instance": True,
                    "contract_sha256": file_sha256(contract_path), "harness_sha256": file_sha256(harness),
                    "firmware_sha256": file_sha256(assets / "wink_simulator.wasm"),
                    "glue_sha256": file_sha256(assets / "wink_simulator.js"),
                    "device_tree_sha256": file_sha256(assets / "device-tree.json"),
                    "runtime_errors": [], "reset_requested": False, "exit_code": 1 if expected_missing else 0}
        if any(report.get(k) != v or type(report.get(k)) is not type(v) for k, v in identity.items()):
            raise ValueError("Report identity, runtime, reset or exit mismatch")
        start, end = report["start_us"], report["end_us"]
        if type(start) is not int or type(end) is not int or not start + 16000000 <= end <= start + 16050000:
            raise ValueError("Full bounded observation window is required")
        frames = report["frames"]
        previous = start
        for f in frames:
            if (f["channel"] not in ("uart", "log") or type(f["time_us"]) is not int or
                    not previous <= f["time_us"] <= end or not isinstance(f["text"], str)):
                raise ValueError("Malformed or unordered public output frame")
            previous = f["time_us"]
        subscribed = [f for f in frames if "Subscribed to TWDT" in f["text"]]
        done = [f for f in frames if "Example complete" in f["text"]]
        if len(subscribed) != 1 or len(done) != 1 or done[0]["time_us"] <= subscribed[0]["time_us"]:
            raise ValueError("Unique original subscription and completion outputs are required")
        if not all(any(text in f["text"] for f in frames) for text in ("TWDT initialized", "Unsubscribed from TWDT", "TWDT deinitialized")):
            raise ValueError("Original lifecycle did not complete")
        alarms = [f for f in frames if "Task watchdog got triggered." in f["text"]]
        failed = [f for f in frames if "did not reset:" in f["text"]]
        case = contract["case"]
        if expected_missing and (case == "control" or alarms or failed):
            raise ValueError("RED requires only an absent automatic timeout for a missed feeder")
        if case == "control":
            if alarms or failed:
                raise ValueError("Healthy control reported a TWDT timeout")
        elif not expected_missing:
            if not alarms or not subscribed[0]["time_us"] + 3000000 <= alarms[0]["time_us"] <= subscribed[0]["time_us"] + 3050000:
                raise ValueError("Automatic timeout absent, early or late")
            culprit = f"did not reset: {'task' if case == 'task' else 'user'} '{case}' (CPU 0)"
            if len(failed) != len(alarms) or any(culprit not in f["text"] for f in failed):
                raise ValueError("Timeout did not identify only the missed feeder")
            for alarm, failure in zip(alarms, failed):
                if alarm["time_us"] != failure["time_us"]:
                    raise ValueError("Culprit does not belong to the reported timeout")
        if end < done[0]["time_us"] + 3000000 or any(f["time_us"] >= done[0]["time_us"] for f in alarms):
            raise ValueError("Watchdog callback survived unsubscribe/deinit")
        expected = [{"index": 0, "type": "automatic_timeout", "status": "failed" if expected_missing else "passed"},
                    {"index": 1, "type": "unsubscribe_recovery", "status": "passed"}]
        if report["step_results"] != expected or any(type(step["index"]) is not int for step in report["step_results"]):
            raise ValueError("Incomplete or contradictory step results")
        return True, "Missing-timeout RED with live original lifecycle accepted" if expected_missing else \
            "Automatic timeout, exact missed feeder and unsubscribe recovery accepted"
    except (OSError, ValueError, KeyError, TypeError, IndexError) as exc:
        return False, str(exc)


class TwdtTimeoutPipeline(LoopPipeline):
    def execute_app(self, app_entry, config_id=None):
        original = self.vendor_root / "system/task_watchdog"
        try:
            execution = self.select_execution(app_entry, config_id)
            if (app_entry.get("id") != "esp.system.task_watchdog" or
                    app_entry.get("target_app_dir") != "system/task_watchdog" or
                    execution["config_id"] != "wasm_sim_standard" or execution["target_soc"] != "esp32" or self.auto_heal):
                raise ValueError("TWDT requires the registered standard ESP32 configuration")
            manifest = json.loads((original / "wink-app.json").read_text(encoding="utf-8"))
            if manifest["upstream"]["files"].get(SOURCE) != UPSTREAM or file_sha256(original / SOURCE) != UPSTREAM:
                raise ValueError("Pinned original TWDT source differs")
            if self.dry_run:
                return PipelineResult(app_entry["id"], True, "TWDT normal, three feed omissions and restored cleanup", "PLANNED")
        except (ValueError, KeyError, TypeError, OSError) as exc:
            return PipelineResult(app_entry.get("id", "app"), False, str(exc), "CONFIGURATION")
        base = super().execute_app(app_entry, config_id)
        if not base.success:
            return base
        candidate_path, original_input = base.candidate_path, self.input_hash(original)
        root = candidate_path.parent
        candidate = json.loads(candidate_path.read_text(encoding="utf-8"))
        app = root / "app/task_watchdog"
        source = app / SOURCE
        original_bytes = source.read_bytes()
        scene = app / "unisim-scenarios" / Path(execution["acceptance"]["scenario_path"]).name
        harness = root / "twdt_timeout_harness.cjs"
        shutil.copyfile(Path(__file__).with_name(harness.name), harness)
        candidate.update(proof_profile="twdt-timeout", status="collecting", stage="TWDT_TIMEOUT",
                         timeout_builds=[], timeout_checks=[], automatic_timeout_complete=False,
                         formal_contract_gaps=["Idle-core monitoring and CPU-hog preemption are not modeled",
                                               "Native timeout scenario and full firmware-dependency proof are pending",
                                               "Independent audit is required for formal delivery"])

        def save():
            temp = candidate_path.with_suffix(".json.tmp")
            temp.write_text(json.dumps(candidate, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            temp.replace(candidate_path)

        def snapshot(kind):
            directory = root / "timeout-builds" / kind
            directory.mkdir(parents=True)
            shutil.copytree(app / "unisim-assets", directory / "assets")
            shutil.copyfile(source, directory / SOURCE)
            shutil.copyfile(scene, directory / "scenario.json")
            tree = json.loads((directory / "assets/device-tree.json").read_text(encoding="utf-8"))
            if tree.get("mcu") != execution["target_soc"]:
                raise ValueError("Actual TWDT asset target differs from the registered ESP32 configuration")
            record = {"kind": kind, "source_path": str(directory / SOURCE), "source_sha256": file_sha256(source),
                      "input_sha256": self.input_hash(app), "scenario_path": str(directory / "scenario.json"),
                      "scenario_sha256": file_sha256(directory / "scenario.json"),
                      "assets_path": str(directory / "assets"),
                      "assets_sha256": compute_assets_composite_sha256(directory / "assets"),
                      "firmware_sha256": file_sha256(directory / "assets/wink_simulator.wasm")}
            if source.read_bytes() != original_bytes:
                patch = "".join(difflib.unified_diff(original_bytes.decode().splitlines(True),
                                                       source.read_text(encoding="utf-8").splitlines(True),
                                                       fromfile=SOURCE, tofile=SOURCE))
                (directory / "source.patch").write_text(patch, encoding="utf-8")
                record.update(patch_path=str(directory / "source.patch"), patch_sha256=file_sha256(directory / "source.patch"))
            candidate["timeout_builds"].append(record)
            save()
            return record

        def build(kind):
            directory = root / "reports" / f"twdt_{kind}"
            directory.mkdir(parents=True)
            before = self.input_hash(app)
            args = ["-File", str(self.runner_script), "-App", str(app), "-Scenario", str(scene),
                    "-ArtifactsDir", str(directory), "-Reporter", "json"]
            rc, output = self.run_powershell(args)
            (directory / "runner.log").write_text(output, encoding="utf-8")
            ok, reason = validate_scenario_report(directory / "run-report.json", scene)
            if rc != 0 or not ok or before != self.input_hash(app):
                raise ValueError(f"{kind} original lifecycle failed: exit {rc}; {reason}")
            record = snapshot(kind)
            record.update(normal_report_path=str(directory / "run-report.json"),
                          normal_report_sha256=file_sha256(directory / "run-report.json"))
            save()
            return record

        def observe(kind, firmware, case):
            directory = root / "timeout-reports" / kind
            directory.mkdir(parents=True)
            contract, report = directory / "contract.json", directory / "report.json"
            contract.write_text(json.dumps(make_contract(case), indent=2) + "\n", encoding="utf-8")
            run_id = f"{candidate['run_id']}-{kind}"
            args = ["node", str(harness), firmware["assets_path"], str(contract), str(report), run_id]
            try:
                proc = subprocess.run(args, cwd=self.ws_root, capture_output=True, text=True,
                                      encoding="utf-8", errors="replace", timeout=40)
                rc, output = proc.returncode, proc.stdout + proc.stderr
            except (OSError, subprocess.TimeoutExpired) as exc:
                rc, output = 124, str(exc)
            (directory / "runner.log").write_text(output, encoding="utf-8")
            ok, reason = validate_timeout_report(report, contract, Path(firmware["assets_path"]), harness, run_id)
            ok = ok and rc == 0
            candidate["timeout_checks"].append({"kind": kind, "case": case, "run_id": run_id,
                "accepted": ok, "verdict": reason, "exit_code": rc, "command": args,
                "report_path": str(report), "report_sha256": file_sha256(report) if report.is_file() else None,
                "contract_path": str(contract), "contract_sha256": file_sha256(contract),
                "harness_path": str(harness), "harness_sha256": file_sha256(harness), **firmware})
            save()
            print(f"[twdt] {kind}: exit {rc}; accepted {ok}; {report}", flush=True)
            return ok

        accepted, failure, baseline = True, None, None
        save()
        try:
            baseline = snapshot("baseline")
            candidate["assets_path"] = baseline["assets_path"]
            for record in candidate["checks"]:
                if record["assets_sha256"] != baseline["assets_sha256"]:
                    raise ValueError("Normal CLI assets changed between phases")
                record["assets_path"] = baseline["assets_path"]
            accepted = observe("baseline", baseline, "control")
            for case, statement in CASES.items():
                text = original_bytes.decode("utf-8")
                if text.count(statement) != 1:
                    raise ValueError(f"Expected exactly one original {case} feed statement")
                source.write_text(text.replace(statement, "        /* Isolated missed-feed stimulus. */"), encoding="utf-8")
                firmware = build(case)
                if firmware["firmware_sha256"] == baseline["firmware_sha256"]:
                    raise ValueError("Missed-feed firmware is equivalent to the baseline")
                accepted = observe(case, firmware, case) and accepted
        except (OSError, ValueError, KeyError, TypeError) as exc:
            failure = str(exc)
        finally:
            try:
                source.write_bytes(original_bytes)
                restored = build("restored")
                accepted = observe("restored", restored, "control") and accepted
                if baseline and baseline["assets_sha256"] != restored["assets_sha256"]:
                    raise ValueError("Restored production assets differ from baseline")
                if self.input_hash(original) != original_input or file_sha256(original / SOURCE) != UPSTREAM:
                    raise ValueError("Original TWDT input changed")
            except (OSError, ValueError, KeyError, TypeError) as exc:
                failure = f"Recovery failed: {exc}"
        complete = accepted and failure is None
        candidate.update(automatic_timeout_complete=complete, status="candidate_ready" if complete else "candidate_incomplete",
                         stage="TWDT_TIMEOUT", message=failure or ("Automatic timeout candidates collected" if complete else
                                                                  "Automatic timeout behavior did not satisfy the pinned contract"))
        save()
        return PipelineResult(app_entry["id"], complete, candidate["message"], "TWDT_TIMEOUT", candidate_path)
