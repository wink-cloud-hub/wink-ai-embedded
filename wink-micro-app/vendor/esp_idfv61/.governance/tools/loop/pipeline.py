# SPDX-License-Identifier: Apache-2.0
"""
Loop Pipeline Orchestrator & Objective Audit Engine
===================================================
Collects isolated candidate evidence: baseline -> assertion self-check -> recovery.
Independent audit and formal delivery are separate operations.
"""
from __future__ import annotations

import datetime
import json
import shutil
import subprocess
import sys
import uuid
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

from .agent import AgentSynthesizer
from .mutator import CanaryMutator
from .remediator import Remediator
from report_contract import file_sha256, is_business_assertion, validate_scenario_report
from evidence_verifier import compute_assets_composite_sha256


class PipelineResult:
    def __init__(self, app_id: str, success: bool, message: str, stage: str, candidate_path: Optional[Path] = None):
        self.app_id = app_id
        self.success = success
        self.message = message
        self.stage = stage
        self.candidate_path = candidate_path

    def __repr__(self) -> str:
        tag = "SUCCESS" if self.success else "FAILED"
        return f"[{tag}] {self.app_id} (stage: {self.stage}): {self.message}"


class LoopPipeline:
    """Candidate-only runner; never signs audits or writes formal delivery data."""

    def __init__(
        self,
        workspace_root: Path,
        custom_agent_cmd: Optional[str] = None,
        custom_agent_a_cmd: Optional[str] = None,
        custom_agent_b_cmd: Optional[str] = None,
        qoder_model: str = "Qwen3.8-Flash",
        auto_heal: bool = False,
        max_heal_attempts: int = 2,
        dry_run: bool = False,
    ):
        self.ws_root = workspace_root
        self.dry_run = dry_run
        self.auto_heal = auto_heal
        self.vendor_root = workspace_root / "wink-micro-app" / "vendor" / "esp_idfv61"
        self.governance_dir = self.vendor_root / ".governance"
        self.manifest_path = self.governance_dir / "data" / "checklist.data.json"
        self.runner_script = (
            workspace_root
            / "wink-micro-os"
            / "frameworks"
            / "esp_idf"
            / "tools"
            / "run_esp32_headless_evidence.ps1"
        )
        self.mutator = CanaryMutator()
        self.agent = AgentSynthesizer(
            workspace_root,
            custom_agent_cmd=custom_agent_cmd,
            custom_agent_a_cmd=custom_agent_a_cmd,
            custom_agent_b_cmd=custom_agent_b_cmd,
            qoder_model=qoder_model,
        )
        self.remediator = Remediator(
            workspace_root=workspace_root,
            agent_synthesizer=self.agent,
            max_attempts=max_heal_attempts,
        )

    def run_powershell(self, cmd_args: list[str], timeout_sec: int = 120) -> Tuple[int, str]:
        """Execute a PowerShell command string reliably across platforms."""
        full_cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass"] + cmd_args
        try:
            res = subprocess.run(
                full_cmd,
                cwd=str(self.ws_root),
                capture_output=True,
                text=True,
                timeout=timeout_sec,
                encoding="utf-8",
                errors="replace",
            )
            return res.returncode, res.stdout + "\n" + res.stderr
        except subprocess.TimeoutExpired:
            return 124, f"Execution timed out after {timeout_sec}s"
        except Exception as e:
            return 1, f"Execution failed: {e}"

    def run_python(self, script_path: Path, args: list[str]) -> Tuple[int, str]:
        """Run a Python script with UTF-8 mode."""
        cmd = ["python", "-X", "utf8", "-B", str(script_path)] + args
        try:
            res = subprocess.run(
                cmd,
                cwd=str(self.ws_root),
                capture_output=True,
                text=True,
                timeout=120,
                encoding="utf-8",
                errors="replace",
            )
            return res.returncode, res.stdout + "\n" + res.stderr
        except Exception as e:
            return 1, f"Python execution error: {e}"

    def compile_peripheral_plugin(self, peripheral_type: str) -> Tuple[bool, str]:
        """Compile a peripheral plugin into dist/manifest.json and dist/simulation.js.

        Includes Build Pre-flight Check for node, npm, and vite dependencies.
        Returns (success: bool, diagnosis_message: str).
        """
        plugin_dir = self.ws_root / "wink-plugin-peripherals" / "builtin" / peripheral_type
        if not plugin_dir.is_dir():
            return False, f"Peripheral plugin directory not found: {plugin_dir}"

        # 1. Pre-flight Check: node & npm availability
        node_bin = shutil.which("node") or shutil.which("node.exe")
        npm_bin = shutil.which("npm") or shutil.which("npm.cmd")
        if not node_bin or not npm_bin:
            return False, (
                "[BUILD_ENV_ERROR] Node.js or npm is missing in host environment. "
                "Cannot build TypeScript simulation plugin for peripheral."
            )

        # 2. Check for build script in package.json or plugin directory
        pkg_file = plugin_dir / "package.json"
        if not pkg_file.is_file():
            return False, f"[BUILD_ENV_ERROR] Missing package.json in peripheral plugin: {plugin_dir}"

        # 3. Execute build:sim
        try:
            build_res = subprocess.run(
                [npm_bin, "run", "build:sim"],
                cwd=str(plugin_dir),
                capture_output=True,
                text=True,
                timeout=120,
                shell=True if sys.platform == "win32" else False,
            )
            if build_res.returncode != 0:
                return False, f"[BUILD_ENV_ERROR] Plugin build failed:\n{build_res.stderr or build_res.stdout}"
        except subprocess.TimeoutExpired:
            return False, "[BUILD_ENV_ERROR] Plugin build timed out after 120s"
        except Exception as e:
            return False, f"[BUILD_ENV_ERROR] Unexpected error compiling plugin: {e}"

        # 4. Verify outputs: dist/simulation.js and dist/manifest.json
        manifest_f = plugin_dir / "1.0.0" / "dist" / "manifest.json"
        if not manifest_f.is_file():
            manifest_f = plugin_dir / "dist" / "manifest.json"

        if not manifest_f.is_file():
            return False, f"Build succeeded but expected output manifest.json not found in {plugin_dir}"

        return True, f"Successfully compiled peripheral plugin '{peripheral_type}'."

    def update_app_peripheral_topology(
        self,
        app_dir: Path,
        peripheral_type: str,
        variant: str,
        address: Optional[int] = None,
    ) -> Tuple[bool, str]:
        """Update or inject peripheral topology into wink-app.json."""
        wink_app_file = app_dir / "wink-app.json"
        if not wink_app_file.is_file():
            return False, f"wink-app.json not found in {app_dir}"

        try:
            app_data = json.loads(wink_app_file.read_text(encoding="utf-8"))
            peripherals = app_data.setdefault("peripherals", [])
            existing = next((p for p in peripherals if p.get("type") == peripheral_type), None)
            if existing:
                existing["variant"] = variant
                if address is not None:
                    existing["address"] = address
            else:
                new_entry: Dict[str, Any] = {
                    "type": peripheral_type,
                    "variant": variant,
                }
                if address is not None:
                    new_entry["address"] = address
                peripherals.append(new_entry)

            wink_app_file.write_text(json.dumps(app_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            return True, f"Updated {wink_app_file} with peripheral {peripheral_type} ({variant})."
        except Exception as e:
            return False, f"Failed updating wink-app.json: {e}"

    @staticmethod
    def input_hash(app_dir: Path) -> str:
        """Bind authored source/configuration/scenario inputs, excluding build outputs."""
        import hashlib
        parts = []
        for path in sorted(app_dir.rglob("*")):
            if not path.is_file() or any(part in ("unisim-assets", "__pycache__", "build") for part in path.relative_to(app_dir).parts):
                continue
            parts.append(f"{path.relative_to(app_dir).as_posix()}:{file_sha256(path)}")
        return hashlib.sha256("\n".join(parts).encode("utf-8")).hexdigest()

    @staticmethod
    def select_execution(app_entry: Dict[str, Any], config_id: Optional[str]) -> Dict[str, Any]:
        executions = app_entry.get("executions", [])
        matches = [
            ex for ex in executions
            if isinstance(ex, dict)
            and isinstance(ex.get("acceptance"), dict)
            and ex["acceptance"].get("type") == "wasm_simulation"
            and (config_id is None or ex.get("config_id") == config_id)
        ]
        if len(matches) != 1 or not matches[0].get("config_id"):
            raise ValueError("Exactly one registered Wasm configuration is required; specify config_id when ambiguous")
        execution = matches[0]
        if execution.get("backend") not in ("wasm_browser", "wasm_node") or execution.get("profile") != "standard" or not execution.get("target_soc"):
            raise ValueError("This collector supports only registered Wasm backends with the CLI standard profile and an explicit target_soc")
        return execution

    def execute_app(self, app_entry: Dict[str, Any], config_id: Optional[str] = None) -> PipelineResult:
        """Collect a candidate in runs/ without modifying formal evidence or Git."""
        app_id = app_entry.get("id", "app")
        run_root = None
        candidate_path = None
        candidate: Dict[str, Any] = {}

        def save_candidate():
            if candidate_path is not None:
                temporary = candidate_path.with_suffix(".json.tmp")
                temporary.write_text(json.dumps(candidate, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
                temporary.replace(candidate_path)

        def fail(stage: str, message: str) -> PipelineResult:
            candidate.update(status="failed", stage=stage, message=message)
            save_candidate()
            return PipelineResult(app_id, False, message, stage, candidate_path)

        try:
            execution = self.select_execution(app_entry, config_id)
            target_dir = app_entry.get("target_app_dir")
            if not isinstance(target_dir, str) or not target_dir:
                return fail("INIT", "No target_app_dir defined")
            original_app = (self.vendor_root / target_dir).resolve()
            if not original_app.is_relative_to(self.vendor_root.resolve()) or not original_app.is_dir():
                return fail("INIT", "Application directory is outside the vendor root or missing")
            declared_scenario = (execution.get("acceptance") or {}).get("scenario_path")
            if not isinstance(declared_scenario, str) or not declared_scenario:
                return fail("CONFIGURATION", "Configuration has no explicit acceptance scenario")
            original_scenario = (self.vendor_root / declared_scenario).resolve()
            if not original_scenario.is_relative_to(original_app / "unisim-scenarios"):
                return fail("CONFIGURATION", "Acceptance scenario is outside the selected application")
            if self.auto_heal:
                return fail("CONFIGURATION", "Candidate collection cannot auto-heal shared runtime sources; an isolated runtime workspace is required")
            if self.dry_run:
                return PipelineResult(app_id, True, f"Candidate plan for {execution['config_id']}: baseline, assertion self-check, recovery", "PLANNED")

            manifest = json.loads(self.manifest_path.read_text(encoding="utf-8"))
            registered = [entry for entry in manifest.get("entries", []) if entry.get("id") == app_id]
            if len(registered) != 1 or registered[0] != app_entry:
                return fail("CONFIGURATION", "Checklist entry changed or is not uniquely registered")

            run_id = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex
            run_root = self.governance_dir / "runs" / run_id
            run_root.mkdir(parents=True, exist_ok=False)
            candidate_path = run_root / "candidate_evidence.json"
            run_app = run_root / "app" / original_app.name
            original_input_hash = self.input_hash(original_app)
            shutil.copytree(
                original_app, run_app,
                ignore=shutil.ignore_patterns("unisim-assets", "build", "build-*", "__pycache__", ".git"),
            )
            scenario_file = run_app / original_scenario.relative_to(original_app)
            candidate = {
                "format_version": 1, "kind": "candidate_evidence", "status": "collecting",
                "run_id": run_id, "app_id": app_id, "target_app_dir": target_dir,
                "config_id": execution["config_id"],
                "execution": {key: execution.get(key) for key in ("backend", "target_soc", "profile")},
                "runner_mode": "headless", "runner_sha256": file_sha256(self.runner_script),
                "original_input_sha256": original_input_hash, "checks": [],
                "limitations": [
                    "Assertion self-check only; firmware dependency, business mutation and fault handling need separate evidence.",
                    "Candidate only; independent audit and formal promotion have not been performed.",
                    "Only application inputs are copied; runtime sources and toolchain build caches remain shared.",
                ],
            }
            (run_root / "checklist-entry.json").write_text(json.dumps(app_entry, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            save_candidate()

            needs_authoring = True
            if scenario_file.is_file():
                scene = json.loads(scenario_file.read_text(encoding="utf-8"))
                if not isinstance(scene, dict) or not isinstance(scene.get("steps"), list):
                    return fail("AUTHORING", "Acceptance scenario must be an object with a step array")
                needs_authoring = not any(is_business_assertion(step) for step in scene.get("steps", []))
            if needs_authoring:
                ok, reason = self.agent.synthesize(app_entry, run_app)
                if not ok or not scenario_file.is_file():
                    return fail("AUTHORING", f"Isolated scenario authoring failed: {reason}")
            scene = json.loads(scenario_file.read_text(encoding="utf-8"))
            if not isinstance(scene, dict) or not isinstance(scene.get("steps"), list) or not all(isinstance(step, dict) for step in scene["steps"]):
                return fail("AUTHORING", "Authored scenario must contain a valid step array")
            if not any(is_business_assertion(step) for step in scene["steps"]):
                return fail("AUTHORING", "Authored scenario has no business assertion")
            input_hash = self.input_hash(run_app)
            candidate["input_sha256"] = input_hash

            gate_script = self.governance_dir / "gates" / "run_gates.py"
            gate_rc, gate_output = self.run_python(gate_script, ["--gate", "1"])
            (run_root / "gate1.log").write_text(gate_output, encoding="utf-8")
            candidate["workspace_gate1_exit_code"] = gate_rc
            if gate_rc != 0:
                return fail("GATE_1_PRE", "Workspace Gate 1 failed; see isolated gate1.log")

            # Use the copied app's normal output layout. The current CLI treats
            # --out as the engine's app directory, which loses app identity.
            assets_dir = run_app / "unisim-assets"
            candidate["assets_path"] = str(assets_dir)

            def collect_phase(kind: str, scenario: Path) -> Dict[str, Any]:
                phase_dir = run_root / "reports" / kind
                phase_dir.mkdir(parents=True, exist_ok=False)
                report_path = phase_dir / "run-report.json"
                args = [
                    "-File", str(self.runner_script), "-App", str(run_app),
                    "-Scenario", str(scenario), "-ArtifactsDir", str(phase_dir),
                    "-Reporter", "json",
                ]
                rc, output = self.run_powershell(args)
                (phase_dir / "runner.log").write_text(output, encoding="utf-8")
                record = {
                    "kind": kind, "run_id": f"{run_id}-{kind}",
                    "config_id": execution["config_id"],
                    **{key: execution[key] for key in ("backend", "target_soc", "profile")},
                    "input_sha256": input_hash, "exit_code": rc, "command": args,
                    "assets_path": str(assets_dir),
                    "scenario_path": str(scenario), "scenario_sha256": file_sha256(scenario),
                    "report_path": str(report_path),
                    "report_sha256": file_sha256(report_path) if report_path.is_file() else None,
                    "assets_sha256": None,
                }
                try:
                    record["assets_sha256"] = compute_assets_composite_sha256(assets_dir)
                except OSError as exc:
                    record["asset_binding_error"] = str(exc)
                candidate["checks"].append(record)
                save_candidate()
                return record

            def positive_ok(record: Dict[str, Any]) -> Tuple[bool, str]:
                if record["exit_code"] != 0 or not record["assets_sha256"]:
                    return False, "Runner did not finish successfully with bound assets"
                ok, reason = validate_scenario_report(Path(record["report_path"]), Path(record["scenario_path"]))
                if not ok:
                    return False, reason
                tree = json.loads((assets_dir / "device-tree.json").read_text(encoding="utf-8"))
                if tree.get("mcu") != execution["target_soc"]:
                    return False, "Actual asset target_soc differs from the selected configuration"
                return True, reason

            baseline = collect_phase("baseline", scenario_file)
            ok, reason = positive_ok(baseline)
            baseline.update(accepted=ok, verdict=reason)
            if not ok:
                return fail("BASELINE", reason)
            candidate["assets_sha256"] = baseline["assets_sha256"]

            killed, kill_reason = False, "Canary was not executed"
            recovery_ok, recovery_reason = False, "Recovery was not executed"
            try:
                mutant_path, meta = self.mutator.create_mutant_file(scenario_file, output_dir=run_root / "mutants")
                if mutant_path is None:
                    kill_reason = meta.get("error", "No valid assertion self-check")
                else:
                    mutant = collect_phase("assertion_self_check", mutant_path)
                    meta.update(report_path=mutant["report_path"], report_sha256=mutant["report_sha256"])
                    candidate["mutation"] = meta
                    if mutant["assets_sha256"] != baseline["assets_sha256"]:
                        kill_reason = "Canary asset hash differs from the positive baseline"
                    else:
                        log = Path(mutant["report_path"]).parent / "runner.log"
                        killed, kill_reason = self.mutator.verify_kill(mutant["exit_code"], log.read_text(encoding="utf-8"), meta)
                    mutant.update(accepted=killed, verdict=kill_reason)
            finally:
                recovery = collect_phase("recovery", scenario_file)
                recovery_ok, recovery_reason = positive_ok(recovery)
                if recovery_ok and recovery["assets_sha256"] != baseline["assets_sha256"]:
                    recovery_ok, recovery_reason = False, "Recovery asset hash differs from the positive baseline"
                recovery.update(accepted=recovery_ok, verdict=recovery_reason)
                save_candidate()

            if not killed:
                return fail("CANARY_KILL", kill_reason)
            if not recovery_ok:
                return fail("RECOVERY", recovery_reason)
            if self.input_hash(run_app) != input_hash or self.input_hash(original_app) != original_input_hash:
                return fail("INPUT_INTEGRITY", "Source/configuration inputs changed during candidate collection")

            candidate.update(status="candidate_ready", stage="CANDIDATE", message="Baseline, assertion self-check and recovery accepted")
            save_candidate()
            return PipelineResult(app_id, True, f"Candidate evidence ready for independent review: {candidate_path}", "CANDIDATE", candidate_path)
        except (OSError, ValueError, KeyError, TypeError, IndexError) as exc:
            return fail("COLLECTION", f"Candidate collection failed: {exc}")
