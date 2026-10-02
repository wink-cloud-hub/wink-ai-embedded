# SPDX-License-Identifier: Apache-2.0
"""
Loop Pipeline Orchestrator & Objective Audit Engine
===================================================
Drives the 8-step deterministic delivery workflow for an application:
synthesis -> semantic gate -> positive baseline -> canary kill -> audit -> commit.
"""
from __future__ import annotations

import datetime
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

from .agent import AgentSynthesizer
from .mutator import CanaryMutator
from .remediator import Remediator


class PipelineResult:
    def __init__(self, app_id: str, success: bool, message: str, stage: str):
        self.app_id = app_id
        self.success = success
        self.message = message
        self.stage = stage

    def __repr__(self) -> str:
        tag = "SUCCESS" if self.success else "FAILED"
        return f"[{tag}] {self.app_id} (stage: {self.stage}): {self.message}"


class LoopPipeline:
    """End-to-end autonomous runner for individual checklist items."""

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

    def rollback_app(self, app_dir: Path):
        """Restore workspace changes on failure to avoid contaminating repo."""
        subprocess.run(
            ["git", "checkout", "--", str(app_dir)],
            cwd=str(self.ws_root),
            capture_output=True,
        )

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

    def execute_app(self, app_entry: Dict[str, Any]) -> PipelineResult:
        """Run the full autonomous SOP workflow on a single checklist entry."""
        app_id = app_entry.get("id", "app")
        target_dir = app_entry.get("target_app_dir", "")
        if not target_dir:
            return PipelineResult(app_id, False, "No target_app_dir defined", "INIT")

        app_dir = self.vendor_root / target_dir
        if not app_dir.is_dir():
            return PipelineResult(app_id, False, f"Directory not found on disk: {target_dir}", "INIT")

        app_name = app_dir.name
        scenario_dir = app_dir / "unisim-scenarios"
        scenario_file = scenario_dir / f"{app_name}.scenario.json"

        # --- Phase 1: Scenario Authoring (if missing or only power assertions) ---
        needs_authoring = True
        if scenario_file.is_file():
            try:
                scen_data = json.loads(scenario_file.read_text(encoding="utf-8"))
                steps = scen_data.get("steps", [])
                non_power_asserts = [
                    s for s in steps
                    if s.get("type", "").startswith("ASSERT_") and not s.get("target", "").startswith("power:")
                ]
                if len(non_power_asserts) >= 1:
                    needs_authoring = False
            except Exception:
                needs_authoring = True

        if needs_authoring:
            ok, msg = self.agent.synthesize(app_entry, app_dir)
            if not ok:
                return PipelineResult(app_id, False, f"Agent authoring failed: {msg}", "AUTHORING")

        # --- Phase 2: Gate 1 Semantic Integrity Pre-Check ---
        gate_script = self.governance_dir / "gates" / "run_gates.py"
        rc, out = self.run_python(gate_script, ["--gate", "1"])
        # Check if our specific app has errors
        app_errors = [
            line for line in out.splitlines()
            if "[ERROR]" in line and f"({app_id})" in line
        ]
        if app_errors:
            self.rollback_app(app_dir)
            err_summary = "\n".join(app_errors[:3])
            return PipelineResult(app_id, False, f"Gate 1 semantic check failed:\n{err_summary}", "GATE_1_PRE")

        # --- Phase 3: Positive Baseline Execution ---
        rc, out = self.run_powershell(["-File", str(self.runner_script), "-App", app_name, "-Reporter", "json"])
        if rc != 0 and self.auto_heal:
            print(f"[heal] Positive baseline failed (code {rc}) for {app_id}. Triggering autonomous self-healing loop...", flush=True)
            heal_ok, heal_msg = self.remediator.remediate_app(app_entry, app_dir, out)
            if heal_ok:
                print(f"[heal] Self-healing succeeded: {heal_msg}. Re-executing baseline simulation...", flush=True)
                rc, out = self.run_powershell(["-File", str(self.runner_script), "-App", app_name, "-Reporter", "json"])
            else:
                self.rollback_app(app_dir)
                return PipelineResult(app_id, False, f"Autonomous self-healing failed:\n{heal_msg}", "BASELINE_HEAL_FAILED")

        if rc != 0:
            self.rollback_app(app_dir)
            return PipelineResult(app_id, False, f"Positive baseline simulation failed (code {rc}):\n{out[:300]}", "BASELINE")

        # --- Phase 4: Anti-False-Green Canary Mutation & Kill Test ---
        mutant_path, meta = self.mutator.create_mutant_file(scenario_file)
        if not mutant_path:
            self.rollback_app(app_dir)
            return PipelineResult(app_id, False, f"Could not create canary mutant: {meta.get('error')}", "CANARY_CREATE")

        try:
            rc_mutant, out_mutant = self.run_powershell(
                ["-File", str(self.runner_script), "-App", app_name, "-Scenario", str(mutant_path), "-Reporter", "json"]
            )
            killed, kill_msg = self.mutator.verify_kill(rc_mutant, out_mutant, meta)
            if not killed:
                self.rollback_app(app_dir)
                return PipelineResult(app_id, False, f"Anti-False-Green Check FAILED: {kill_msg}", "CANARY_KILL")
        finally:
            if mutant_path.is_file():
                mutant_path.unlink()

        # --- Phase 5: Evidence Recording & Objective Audit Signing ---
        if not self.dry_run:
            rc, out = self.run_powershell(
                ["-File", str(self.runner_script), "-App", app_name, "-WriteEvidence", "-Reporter", "json"]
            )
            if rc != 0:
                self.rollback_app(app_dir)
                return PipelineResult(app_id, False, f"Evidence recording failed: {out[:300]}", "EVIDENCE_WRITE")

            # Apply Objective Audit Sign-Off
            now_iso = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
            with open(self.manifest_path, "r", encoding="utf-8") as f:
                mdata = json.load(f)

            for e in mdata.get("entries", []):
                if e.get("id") == app_id:
                    e["audit"] = {
                        "verdict": "audited",
                        "auditor": "loop_sop_daemon",
                        "audited_at": now_iso,
                        "audited_configs": ["wasm_sim_standard"],
                        "dispute_ref": None,
                        "ruling_path": None,
                    }
                    break

            if "summary" in mdata:
                mdata["summary"]["audited"] = sum(1 for e in mdata["entries"] if e.get("audit", {}).get("verdict") == "audited")
                mdata["summary"]["verified_configs"] = sum(
                    1 for e in mdata["entries"] for ex in e.get("executions", []) if ex.get("delivery_state") == "verified"
                )

            with open(self.manifest_path, "w", encoding="utf-8") as f:
                json.dump(mdata, f, indent=2, ensure_ascii=False)
                f.write("\n")

        # --- Phase 6: Post-Audit Gate 1 Check ---
        rc, out = self.run_python(gate_script, ["--gate", "1"])
        post_app_errors = [
            line for line in out.splitlines()
            if "[ERROR]" in line and f"({app_id})" in line
        ]
        if rc != 0 and post_app_errors:
            self.rollback_app(app_dir)
            err_summary = "\n".join(post_app_errors[:3])
            return PipelineResult(app_id, False, f"Post-audit Gate 1 failed:\n{err_summary}", "GATE_1_POST")

        # --- Phase 7: Re-render Checklist & Git Commit ---
        if not self.dry_run:
            gen_script = self.governance_dir / "tools" / "generate_checklist_v1_1.py"
            self.run_python(gen_script, [])

            # Atomic Git Commit
            commit_msg = (
                f"feat(vendor/esp_idfv61): [loop] auto-verify {app_name} with Canary kill evidence\n\n"
                f"- Autonomously authored domain assertions complying with governance-sop-esp\n"
                f"- Successfully verified defect sensitivity via Canary mutant kill\n"
                f"- Passed Gate 1 static semantics and evidence verifier\n"
                f"- Machine audit sign-off by loop_sop_daemon for {app_id}"
            )
            subprocess.run(
                ["git", "add", str(app_dir), str(self.manifest_path), str(self.vendor_root / "CHECKLIST.md")],
                cwd=str(self.ws_root),
                capture_output=True,
            )
            report_dir = self.governance_dir / "reports" / target_dir
            if report_dir.is_dir():
                subprocess.run(["git", "add", str(report_dir)], cwd=str(self.ws_root), capture_output=True)

            c_res = subprocess.run(
                ["git", "commit", "-m", commit_msg],
                cwd=str(self.ws_root),
                capture_output=True,
                text=True,
            )
            if c_res.returncode != 0 and "nothing to commit" not in c_res.stdout:
                return PipelineResult(app_id, False, f"Git commit failed: {c_res.stderr}", "GIT_COMMIT")

        return PipelineResult(app_id, True, f"Successfully delivered and committed {app_name} with Canary proof!", "COMPLETE")
