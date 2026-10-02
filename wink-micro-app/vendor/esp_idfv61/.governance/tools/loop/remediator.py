# SPDX-License-Identifier: Apache-2.0
"""
Autonomous Self-Healing Remediator Engine
==========================================
Drives autonomous investigation, double-blind adversarial review, transactional
patch application, and tiered zero-regression verification for ESP-IDF examples.
Enforces 12-state deterministic lifecycle and ADR-0092 PAL additive evolution.
"""
from __future__ import annotations

import datetime
import json
import os
import re
import subprocess
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

from .agent import AgentSynthesizer
from .safety_checker import HeuristicSafetyChecker


class RemediatorState(str, Enum):
    INIT = "INIT"
    RCA_AUTHORED = "RCA_AUTHORED"
    PLAN_AUTHORED = "PLAN_AUTHORED"
    ADVERSARIAL_REVIEWING = "ADVERSARIAL_REVIEWING"
    REVISE_REQUIRED = "REVISE_REQUIRED"
    PLAN_SYNTHESIZED = "PLAN_SYNTHESIZED"
    HEURISTIC_PRECHECK = "HEURISTIC_PRECHECK"
    PATCH_APPLYING = "PATCH_APPLYING"
    PATCH_APPLIED = "PATCH_APPLIED"
    DUAL_TARGET_COMPILING = "DUAL_TARGET_COMPILING"
    L1_REGRESSION = "L1_REGRESSION"
    L2_REGRESSION = "L2_REGRESSION"
    CANARY_KILL = "CANARY_KILL"
    COMPLETED = "COMPLETED"
    ROLLED_BACK = "ROLLED_BACK"
    CIRCUIT_BREAKER_ESCALATED = "CIRCUIT_BREAKER_ESCALATED"


# Domain mapping statically driven per implementation plan
DOMAIN_FILE_MAP = {
    "network": ["esp_mqtt", "esp_http", "wifi", "tcpip", "sim_net_responder"],
    "peripheral": ["i2c", "spi", "uart", "gpio", "pwm", "adc", "dac"],
    "timer": ["gptimer", "hw_timer", "mcpwm"],
}

# Domain to verified app ID mapping
DOMAIN_APP_MAP = {
    "network": ["esp.protocols.esp_http_client", "esp.wifi.getting_started.station"],
    "peripheral": ["esp.get_started.blink", "esp.peripherals.ledc.ledc_basic", "esp.peripherals.uart.uart_echo"],
    "timer": ["esp.peripherals.timer_group.gptimer"],
}


class InvestigationWorkspace:
    """Manages files, state snapshot, and evidence for an investigation entry."""

    def __init__(self, workspace_root: Path, entry_id: str):
        self.ws_root = workspace_root
        self.entry_id = entry_id
        self.investigation_dir = (
            workspace_root
            / "wink-micro-app"
            / "vendor"
            / "esp_idfv61"
            / ".governance"
            / "investigations"
            / entry_id
        )
        self.state_file = self.investigation_dir / "session_state.json"
        self.raw_failure_file = self.investigation_dir / "raw_failure.log"
        self.rca_file = self.investigation_dir / "01-ROOT-CAUSE-ANALYSIS.md"
        self.plan_file = self.investigation_dir / "02-REMEDIATION-PLAN.md"
        self.review_file = self.investigation_dir / "03-ADVERSARIAL-REVIEW.md"
        self.patch_file = self.investigation_dir / "patch.diff"
        self.regression_log_file = self.investigation_dir / "regression_run.log"

    def initialize(self, raw_failure_log: str, max_attempts: int = 2) -> Dict[str, Any]:
        """Create workspace directory and initialize session_state.json."""
        self.investigation_dir.mkdir(parents=True, exist_ok=True)
        self.raw_failure_file.write_text(raw_failure_log, encoding="utf-8")

        if self.state_file.is_file():
            try:
                state_data = json.loads(self.state_file.read_text(encoding="utf-8"))
                if state_data.get("schema_version") == 2:
                    return state_data
            except Exception:
                pass

        now_iso = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        initial_state = {
            "schema_version": 2,
            "app_id": self.entry_id,
            "attempt": 1,
            "max_attempts": max_attempts,
            "current_state": RemediatorState.INIT.value,
            "history": [{"state": RemediatorState.INIT.value, "timestamp": now_iso}],
            "modified_files": [],
            "created_files": [],
            "triggered_domain": None,
        }
        self.save_state(initial_state)
        return initial_state

    def load_state(self) -> Dict[str, Any]:
        if not self.state_file.is_file():
            return {}
        return json.loads(self.state_file.read_text(encoding="utf-8"))

    def save_state(self, state_data: Dict[str, Any]):
        self.state_file.write_text(json.dumps(state_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    def transition_to(self, new_state: RemediatorState, **extra) -> Dict[str, Any]:
        """Update current_state and record history transition."""
        state = self.load_state()
        state["current_state"] = new_state.value
        now_iso = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        hist_entry = {"state": new_state.value, "timestamp": now_iso}
        hist_entry.update(extra)
        state.setdefault("history", []).append(hist_entry)
        for k, v in extra.items():
            state[k] = v
        self.save_state(state)
        return state

    def verify_synthesis_not_mere_append(self, original_plan: str, synthesized_plan: str) -> Tuple[bool, str]:
        """Ensure synthesized plan modified the body rather than merely appending at the end."""
        orig_body = original_plan.strip()
        syn_body = synthesized_plan.strip()

        if syn_body == orig_body:
            return False, "Synthesized plan is identical to original plan; no edits made."

        # Check if synthesized plan is just original plan + trailing text
        if syn_body.startswith(orig_body):
            return False, "Synthesis rejected: Agent only appended text to the end without reconstructing plan body."

        # Check for Synthesis Log section
        if "## 4. 评审建议融合记录" not in syn_body and "Synthesis Log" not in syn_body:
            return False, "Synthesized plan is missing mandatory section: '## 4. 评审建议融合记录（Synthesis Log）'."

        return True, "Synthesis protocol passed."


class TransactionalGitTracker:
    """Safely manages patch application, pre-flight clean checks, and dual-track rollback."""

    def __init__(self, workspace_root: Path):
        self.ws_root = workspace_root

    def pre_flight_check(self, candidate_files: List[Path]) -> Tuple[bool, str]:
        """Ensure candidate files have no uncommitted dirty modifications before healing starts.

        Per plan: If dirty modifications are found on target files, immediately terminate and
        ask user to git stash or git commit.
        """
        try:
            res = subprocess.run(
                ["git", "status", "--porcelain"],
                cwd=str(self.ws_root),
                capture_output=True,
                text=True,
                check=True,
            )
        except Exception as e:
            return False, f"Git status pre-flight check failed: {e}"

        dirty_files = set()
        for line in res.stdout.splitlines():
            line = line.strip()
            if not line:
                continue
            # line format: "XY filename"
            parts = line.split(maxsplit=1)
            if len(parts) == 2:
                dirty_files.add(parts[1].replace("\\", "/"))

        for cf in candidate_files:
            rel_str = cf.relative_to(self.ws_root).as_posix()
            if rel_str in dirty_files:
                return False, (
                    f"[DIRTY WORKSPACE PRE-FLIGHT BLOCKED] Target candidate file '{rel_str}' has uncommitted "
                    f"local modifications. Please 'git stash' or 'git commit' your changes before running --auto-heal."
                )

        return True, "Pre-flight clean check passed."

    def apply_patch(self, patch_file: Path) -> Tuple[bool, str, List[Path], List[Path]]:
        """Apply unified diff patch with git apply --check pre-validation.

        Returns (success: bool, msg: str, modified_files: List[Path], created_files: List[Path]).
        """
        # Step 1: Pre-check with git apply --check
        check_res = subprocess.run(
            ["git", "apply", "--check", str(patch_file)],
            cwd=str(self.ws_root),
            capture_output=True,
            text=True,
        )
        if check_res.returncode != 0:
            return False, f"git apply --check failed:\n{check_res.stderr}", [], []

        # Analyze which files exist before applying to differentiate modified vs created
        patch_text = patch_file.read_text(encoding="utf-8")
        touched = HeuristicSafetyChecker.extract_touched_files(patch_text)

        created: List[Path] = []
        modified: List[Path] = []

        for rel_path in touched:
            abs_p = self.ws_root / rel_path
            if abs_p.is_file():
                modified.append(abs_p)
            else:
                created.append(abs_p)

        # Step 2: Actually apply patch
        apply_res = subprocess.run(
            ["git", "apply", str(patch_file)],
            cwd=str(self.ws_root),
            capture_output=True,
            text=True,
        )
        if apply_res.returncode != 0:
            return False, f"git apply execution failed:\n{apply_res.stderr}", [], []

        return True, "Patch applied successfully.", modified, created

    def rollback(self, modified_files: List[Path], created_files: List[Path]):
        """Dual-track precise rollback:

        1. Tracked modified: git checkout -- <file>
        2. Untracked created: file.unlink()
        """
        for mf in modified_files:
            if mf.is_file():
                subprocess.run(
                    ["git", "checkout", "--", str(mf)],
                    cwd=str(self.ws_root),
                    capture_output=True,
                )

        for cf in created_files:
            try:
                if cf.is_file():
                    cf.unlink(missing_ok=True)
            except Exception:
                pass


class ZeroRegressionRunner:
    """Executes tiered zero-regression: L1 domain fast-check + L2 full golden suite."""

    def __init__(self, workspace_root: Path, runner_script: Path):
        self.ws_root = workspace_root
        self.runner_script = runner_script
        self.vendor_root = workspace_root / "wink-micro-app" / "vendor" / "esp_idfv61"
        self.manifest_path = self.vendor_root / ".governance" / "data" / "checklist.data.json"

    def detect_domain(self, touched_files: List[Path]) -> Optional[str]:
        """Detect triggered domain based on touched file path keywords."""
        for p in touched_files:
            p_str = p.as_posix().lower()
            for domain, keywords in DOMAIN_FILE_MAP.items():
                if any(kw in p_str for kw in keywords):
                    return domain
        return None

    def run_app_sim(self, app_name: str) -> Tuple[bool, str]:
        """Run single app simulation through headless script."""
        full_cmd = [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(self.runner_script),
            "-App",
            app_name,
            "-Reporter",
            "json",
        ]
        try:
            res = subprocess.run(
                full_cmd,
                cwd=str(self.ws_root),
                capture_output=True,
                text=True,
                timeout=120,
                encoding="utf-8",
                errors="replace",
            )
            return res.returncode == 0, res.stdout + "\n" + res.stderr
        except Exception as e:
            return False, f"Execution failed: {e}"

    def run_l1_domain_regression(self, domain: str) -> Tuple[bool, str]:
        """Run verified apps in the same domain."""
        target_app_ids = DOMAIN_APP_MAP.get(domain, [])
        if not target_app_ids:
            return True, f"No verified baseline apps registered for domain '{domain}'."

        for app_id in target_app_ids:
            # Map app_id to directory name
            app_name = app_id.split(".")[-1]
            ok, out = self.run_app_sim(app_name)
            if not ok:
                return False, f"L1 Domain Regression FAILED on verified baseline '{app_id}':\n{out[:300]}"

        return True, f"L1 Domain Regression passed ({len(target_app_ids)} apps)."

    def run_l2_golden_regression(self) -> Tuple[bool, str]:
        """Run all 6 golden verified benchmarks in checklist.data.json."""
        if not self.manifest_path.is_file():
            return True, "No manifest found; skipping L2 golden regression."

        try:
            mdata = json.loads(self.manifest_path.read_text(encoding="utf-8"))
            verified_entries = [
                e for e in mdata.get("entries", [])
                if any(ex.get("delivery_state") == "verified" for ex in e.get("executions", []))
            ]
        except Exception as e:
            return False, f"Could not parse manifest for L2 regression: {e}"

        for entry in verified_entries:
            target_dir = entry.get("target_app_dir", "")
            app_name = Path(target_dir).name
            ok, out = self.run_app_sim(app_name)
            if not ok:
                return False, f"L2 Golden Regression FAILED on benchmark '{entry.get('id')}':\n{out[:300]}"

        return True, f"L2 Golden Suite passed ({len(verified_entries)} benchmarks verified)."


class Remediator:
    """Central autonomous remediation controller orchestrating RCA, review, fix, and gates."""

    def __init__(
        self,
        workspace_root: Path,
        agent_synthesizer: AgentSynthesizer,
        max_attempts: int = 2,
    ):
        self.ws_root = workspace_root
        self.agent = agent_synthesizer
        self.max_attempts = max_attempts
        self.safety_checker = HeuristicSafetyChecker(workspace_root)
        self.git_tracker = TransactionalGitTracker(workspace_root)
        runner_script = (
            workspace_root
            / "wink-micro-os"
            / "frameworks"
            / "esp_idf"
            / "tools"
            / "run_esp32_headless_evidence.ps1"
        )
        self.regression_runner = ZeroRegressionRunner(workspace_root, runner_script)

    def remediate_app(
        self,
        app_entry: Dict[str, Any],
        app_dir: Path,
        failure_log: str,
    ) -> Tuple[bool, str]:
        """Run full autonomous self-healing cycle for a failing application.

        Returns (success: bool, summary_message: str).
        """
        app_id = app_entry.get("id", "app")
        ws = InvestigationWorkspace(self.ws_root, app_id)
        state_data = ws.initialize(failure_log, max_attempts=self.max_attempts)

        attempt = state_data.get("attempt", 1)
        if attempt > self.max_attempts:
            ws.transition_to(RemediatorState.CIRCUIT_BREAKER_ESCALATED, reason="Exceeded maximum self-healing attempts.")
            return False, f"[BLOCKED_ON_INFRA_HUMAN_TRIAGE] Maximum attempts ({self.max_attempts}) reached for {app_id}."

        print(f"  [heal] Starting autonomous investigation for {app_id} (Attempt {attempt}/{self.max_attempts})...", flush=True)

        # ---------------------------------------------------------------------
        # Step 1: Root Cause Analysis & Initial Proposal (Role A)
        # ---------------------------------------------------------------------
        rca_prompt = self.agent.build_root_cause_prompt(app_entry, app_dir, failure_log, ws.investigation_dir)
        rc_code, rc_out = self.agent.invoke_agent(rca_prompt, role="A")

        if not ws.rca_file.is_file() or not ws.plan_file.is_file():
            # If agent printed content in output, write it to disk
            if "01-ROOT-CAUSE" in rc_out or "# Root Cause" in rc_out:
                ws.rca_file.write_text(rc_out, encoding="utf-8")
            if not ws.rca_file.is_file():
                return False, f"Agent Role A failed to produce 01-ROOT-CAUSE-ANALYSIS.md: {rc_out[:200]}"

        orig_plan_content = ws.plan_file.read_text(encoding="utf-8") if ws.plan_file.is_file() else ""
        ws.transition_to(RemediatorState.PLAN_AUTHORED)

        # Extract patch.diff
        patch_text = ""
        if ws.patch_file.is_file():
            patch_text = ws.patch_file.read_text(encoding="utf-8")
        else:
            extracted = self.agent.extract_patch_diff(orig_plan_content + "\n" + rc_out)
            if extracted:
                patch_text = extracted
                ws.patch_file.write_text(patch_text, encoding="utf-8")

        if not patch_text:
            return False, "Agent A failed to produce a valid Unified Diff patch.diff."

        # ---------------------------------------------------------------------
        # Step 2: Double-Blind Adversarial Review (Role B)
        # ---------------------------------------------------------------------
        print(f"  [heal] Spawning Role B (Adversarial Red-Team Auditor) for double-blind review...", flush=True)
        ws.transition_to(RemediatorState.ADVERSARIAL_REVIEWING)
        review_prompt = self.agent.build_adversarial_review_prompt(
            app_entry=app_entry,
            raw_failure_log=failure_log,
            plan_content=orig_plan_content,
            patch_diff=patch_text,
        )
        r_code, r_out = self.agent.invoke_agent(review_prompt, role="B")
        if not ws.review_file.is_file():
            ws.review_file.write_text(r_out, encoding="utf-8")

        review_text = ws.review_file.read_text(encoding="utf-8")
        verdict, he_passed, blocking_cnt, recs = self.agent.parse_review_verdict(review_text)
        print(f"  [heal] Role B Review Verdict: {verdict} (blocking_issues: {blocking_cnt})", flush=True)

        if verdict == "REJECTED":
            ws.transition_to(RemediatorState.ROLLED_BACK, reason="Adversarial reviewer rejected proposal.")
            return False, f"Proposal rejected by Role B adversarial reviewer with {blocking_cnt} blocking issues."

        # ---------------------------------------------------------------------
        # Step 3: Deep Synthesis & Body Reconstruction (if revision requested)
        # ---------------------------------------------------------------------
        if verdict == "REVISE_REQUIRED" or blocking_cnt > 0:
            print(f"  [heal] Triggering Deep Synthesis to absorb reviewer feedback...", flush=True)
            synth_prompt = self.agent.build_synthesis_prompt(
                app_entry=app_entry,
                original_plan=orig_plan_content,
                review_content=review_text,
                investigation_dir=ws.investigation_dir,
            )
            s_code, s_out = self.agent.invoke_agent(synth_prompt, role="A")

            synthesized_plan = ws.plan_file.read_text(encoding="utf-8") if ws.plan_file.is_file() else ""
            valid_synth, synth_msg = ws.verify_synthesis_not_mere_append(orig_plan_content, synthesized_plan)
            if not valid_synth:
                ws.transition_to(RemediatorState.ROLLED_BACK, reason=synth_msg)
                return False, f"Deep synthesis protocol failed: {synth_msg}"

            # Refresh patch.diff if updated
            if ws.patch_file.is_file():
                patch_text = ws.patch_file.read_text(encoding="utf-8")
            ws.transition_to(RemediatorState.PLAN_SYNTHESIZED)

        # ---------------------------------------------------------------------
        # Step 4: Machine Heuristic Safety Check (H-1 to H-8)
        # ---------------------------------------------------------------------
        print(f"  [heal] Running machine heuristic safety check (H-1 to H-8)...", flush=True)
        ws.transition_to(RemediatorState.HEURISTIC_PRECHECK)
        he_ok, he_errors = self.safety_checker.validate_all(patch_text)
        if not he_ok:
            err_msg = "; ".join(he_errors[:3])
            ws.transition_to(RemediatorState.ROLLED_BACK, reason=f"Heuristic safety check failed: {err_msg}")
            return False, f"Heuristic safety check intercepted {len(he_errors)} violation(s): {err_msg}"

        # ---------------------------------------------------------------------
        # Step 5: Pre-Flight Clean Check & Transactional Patch Application
        # ---------------------------------------------------------------------
        touched_files = [self.ws_root / f for f in HeuristicSafetyChecker.extract_touched_files(patch_text)]
        pre_ok, pre_msg = self.git_tracker.pre_flight_check(touched_files)
        if not pre_ok:
            ws.transition_to(RemediatorState.ROLLED_BACK, reason=pre_msg)
            return False, pre_msg

        print(f"  [heal] Applying candidate patch transactionally...", flush=True)
        ws.transition_to(RemediatorState.PATCH_APPLYING)
        apply_ok, apply_msg, mod_files, new_files = self.git_tracker.apply_patch(ws.patch_file)
        if not apply_ok:
            ws.transition_to(RemediatorState.ROLLED_BACK, reason=apply_msg)
            return False, f"Transactional patch application failed: {apply_msg}"

        ws.transition_to(
            RemediatorState.PATCH_APPLIED,
            modified_files=[f.relative_to(self.ws_root).as_posix() for f in mod_files],
            created_files=[f.relative_to(self.ws_root).as_posix() for f in new_files],
        )

        # ---------------------------------------------------------------------
        # Step 6: Tiered Zero-Regression Verification (L1 + L2)
        # ---------------------------------------------------------------------
        triggered_domain = self.regression_runner.detect_domain(touched_files)
        print(f"  [heal] Detected triggered domain: {triggered_domain or 'general'}", flush=True)
        ws.transition_to(RemediatorState.L1_REGRESSION, triggered_domain=triggered_domain)

        try:
            if triggered_domain:
                l1_ok, l1_msg = self.regression_runner.run_l1_domain_regression(triggered_domain)
                if not l1_ok:
                    self.git_tracker.rollback(mod_files, new_files)
                    ws.transition_to(RemediatorState.ROLLED_BACK, reason=l1_msg)
                    return False, f"L1 Zero-Regression check failed: {l1_msg}"

            print(f"  [heal] Running L2 full golden regression...", flush=True)
            ws.transition_to(RemediatorState.L2_REGRESSION)
            l2_ok, l2_msg = self.regression_runner.run_l2_golden_regression()
            if not l2_ok:
                self.git_tracker.rollback(mod_files, new_files)
                ws.transition_to(RemediatorState.ROLLED_BACK, reason=l2_msg)
                return False, f"L2 Golden Suite zero-regression failed: {l2_msg}"

            # Self-healing succeeded at foundation level!
            ws.transition_to(RemediatorState.COMPLETED)
            print(f"  [heal] Foundation self-healing verified cleanly!", flush=True)
            return True, "Autonomous self-healing patch applied and verified with zero regression."

        except Exception as e:
            self.git_tracker.rollback(mod_files, new_files)
            ws.transition_to(RemediatorState.ROLLED_BACK, reason=str(e))
            return False, f"Unexpected error during regression testing: {e}"
