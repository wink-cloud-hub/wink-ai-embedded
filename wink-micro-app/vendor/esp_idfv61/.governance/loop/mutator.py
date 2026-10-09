# SPDX-License-Identifier: Apache-2.0
"""
Anti-False-Green Canary 3D Mutation Engine
===========================================
Autonomously creates non-equivalent mutants of *.scenario.json across three dimensions:
- Dimension A (Assertion Mutant): Modifies expected matchers to test assertion sensitivity.
- Dimension B (Stimulus Mutant): Corrupts external environment/input payloads.
- Dimension C (Platform Fault Mutant): Injects underlying hardware/protocol faults via probes.

Assertion self-checks and environment sensitivity checks are separate evidence;
neither substitutes for firmware-dependency or business-implementation mutations.
"""
from __future__ import annotations

import copy
import json
import sys
import uuid
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

try:
    from gates.report_contract import file_sha256, is_business_assertion, validate_scenario_report
except ImportError:
    GATES_DIR = Path(__file__).resolve().parent.parent / "gates"
    if str(GATES_DIR) not in sys.path:
        sys.path.insert(0, str(GATES_DIR))
    from report_contract import file_sha256, is_business_assertion, validate_scenario_report


DIM_ASSERTION = "assertion"          # 维度 A: 篡改预期值 (Matcher 活跃度)
DIM_STIMULUS = "stimulus"            # 维度 B: 篡改外部环境与输入激励载荷
DIM_PLATFORM_FAULT = "platform_fault"# 维度 C: 底层硬件受控故障探针


class CanaryMutator:
    """Detects primary domain assertion steps and injects calibrated 3D defects."""

    @staticmethod
    def identify_target_step(steps: list[dict[str, Any]]) -> Optional[int]:
        """Find the index of the primary domain assertion step to mutate.

        Prefers the last domain-specific assertion (timer:, i2c:, uart:, http:, wifi:, gpio:).
        Ignores INJECT_* fixtures and non-assertion steps.
        """
        candidate_indices = [i for i, step in enumerate(steps) if is_business_assertion(step)]

        if candidate_indices:
            # Pick the last domain assertion (representative of end-to-end outcome)
            return candidate_indices[-1]

        return None

    @staticmethod
    def identify_stimulus_step(steps: list[dict[str, Any]]) -> Optional[int]:
        """Find the index of the external stimulus or fixture injection step to mutate."""
        for i, step in enumerate(steps):
            stype = step.get("type", "")
            if isinstance(stype, str) and (stype.startswith(("INJECT_", "INPUT_")) or "routes" in step or "busPayload" in step):
                return i
        return None

    @staticmethod
    def mutate_matcher(matcher: Any) -> Tuple[Any, str]:
        """Apply mathematical/semantic mutation operator to a matcher (Dimension A).

        Returns (mutated_matcher, human_readable_mutation_desc).
        """
        if isinstance(matcher, bool):
            return not matcher, "boolean inverted"
        if isinstance(matcher, (int, float)):
            # Scalar numeric (e.g., status_code=200, count=1000000, freq=1000000)
            if matcher == 200:
                return 404, "scalar 200 -> 404"
            if matcher == 0:
                return 1, "scalar 0 -> 1"
            if matcher == 1:
                return 0, "scalar 1 -> 0"
            return matcher + 999999, f"scalar {matcher} -> {matcher + 999999}"

        if isinstance(matcher, str):
            return f"__MUTANT_FAIL_{matcher}__", f"string '{matcher}' corrupted"

        if isinstance(matcher, dict):
            # Compound matcher objects
            mutant_dict = copy.deepcopy(matcher)
            if "$between" in mutant_dict and isinstance(mutant_dict["$between"], (list, tuple)):
                bounds = mutant_dict["$between"]
                if len(bounds) == 2:
                    upper = bounds[1]
                    shift = 1000000
                    mutant_dict["$between"] = [upper + shift, upper + shift * 2]
                    return mutant_dict, f"$between {bounds} shifted to {mutant_dict['$between']}"

            if "$near" in mutant_dict and isinstance(mutant_dict["$near"], dict):
                target = mutant_dict["$near"].get("target", 0)
                tol = mutant_dict["$near"].get("tolerance", 1)
                mutant_dict["$near"]["target"] = target + tol * 10 + 100
                return mutant_dict, f"$near target {target} shifted out of tolerance"

            if "$gte" in mutant_dict and isinstance(mutant_dict["$gte"], (int, float)):
                mutant_dict["$gte"] += 1000000
                return mutant_dict, f"$gte increased by 1000000"

            if "$lte" in mutant_dict and isinstance(mutant_dict["$lte"], (int, float)):
                mutant_dict["$lte"] -= 1000000
                return mutant_dict, f"$lte decreased by 1000000"

            if "$regex" in mutant_dict:
                mutant_dict["$regex"] = "^__MUTANT_IMPOSSIBLE_UNMATCHED_PATTERN__$"
                return mutant_dict, "$regex mutated to impossible pattern"

        raise ValueError("No valid mutation operator for this matcher")

    @staticmethod
    def mutate_stimulus(step: dict[str, Any]) -> Tuple[dict[str, Any], str]:
        """Apply defect to an external stimulus step (Dimension B)."""
        mutant_step = copy.deepcopy(step)
        if "routes" in mutant_step and isinstance(mutant_step["routes"], list) and len(mutant_step["routes"]) > 0:
            route = mutant_step["routes"][0]
            orig_status = route.get("status_code", 200)
            route["status_code"] = 500 if orig_status == 200 else 503
            route["body"] = '{"error": "injected_server_defect"}'
            return mutant_step, f"route status {orig_status} -> {route['status_code']} (500 Internal Error)"

        if "hex" in mutant_step:
            mutant_step["hex"] = "DEADBEEF"
            return mutant_step, "injected hex payload corrupted to DEADBEEF"

        if "password" in mutant_step:
            mutant_step["password"] = "__CORRUPTED_WRONG_PASSWORD__"
            return mutant_step, "Wi-Fi password corrupted"

        payload = mutant_step.get("payload")
        if isinstance(payload, dict) and isinstance(payload.get("text"), str):
            payload["text"] = "__CORRUPTED_INPUT__"
            return mutant_step, "UART input payload changed"
        if isinstance(payload, dict) and isinstance(payload.get("hex"), str):
            payload["hex"] = "DEADBEEF"
            return mutant_step, "Binary input payload changed"
        access_points = mutant_step.get("accessPoints")
        if isinstance(access_points, list) and access_points and "password" in access_points[0]:
            access_points[0]["password"] = "__CORRUPTED_WRONG_PASSWORD__"
            return mutant_step, "AP password changed"
        raise ValueError("No valid mutation operator for this stimulus")

    def create_mutant_file(
        self,
        scenario_path: Path,
        dimension: str = DIM_ASSERTION,
        fault_domain: str = "i2c",
        fault_type: str = "nack",
        fault_param: int = 0,
        output_dir: Optional[Path] = None,
    ) -> Tuple[Optional[Path], Dict[str, Any]]:
        """Parse scenario, generate 3D mutant, write temporary file.

        Supported dimensions:
        - 'assertion' (Dimension A): Mutates matcher in primary assertion step.
        - 'stimulus' (Dimension B): Corrupts external environment/fixture inputs.
        - 'platform_fault' (Dimension C): Injects controlled platform fault probe step.

        Returns (mutant_path, metadata) or (None, metadata).
        """
        with open(scenario_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        if not isinstance(data, dict) or not isinstance(data.get("steps"), list) or not all(isinstance(step, dict) for step in data["steps"]):
            return None, {"error": "Scenario must be an object with a valid step array"}
        if "header" in data and not isinstance(data["header"], dict):
            return None, {"error": "Scenario header must be an object"}
        steps = data.get("steps", [])
        mutated_data = copy.deepcopy(data)
        if dimension not in (DIM_ASSERTION, DIM_STIMULUS, DIM_PLATFORM_FAULT):
            return None, {"error": f"Unknown mutation dimension: {dimension}"}
        target_idx = self.identify_target_step(steps)
        if target_idx is None:
            return None, {"error": "No business assertion step found to mutate"}
        mutation_idx = target_idx

        if dimension == DIM_STIMULUS:
            stim_idx = self.identify_stimulus_step(steps)
            if stim_idx is not None:
                try:
                    mut_step, mutation_desc = self.mutate_stimulus(steps[stim_idx])
                except ValueError as exc:
                    return None, {"error": str(exc)}
                mutated_data["steps"][stim_idx] = mut_step
                mutated_data["steps"][stim_idx]["description"] = (
                    f"[CANARY STIMULUS MUTANT] {mutation_desc} | {steps[stim_idx].get('description', '')}"
                )
                mutation_idx = stim_idx
            else:
                return None, {"error": "No supported stimulus step; no fallback to assertion self-check"}

        if dimension == DIM_PLATFORM_FAULT:
            fault_step = {
                "type": "INJECT_PLATFORM_FAULT",
                "timeUs": "0ms",
                "domain": fault_domain,
                "fault": fault_type,
                "param": fault_param,
                "description": f"[CANARY PLATFORM FAULT MUTANT] Injected {fault_domain}:{fault_type} (param={fault_param})",
            }
            mutated_data["steps"].insert(0, fault_step)
            mutation_desc = f"injected platform fault probe {fault_domain}:{fault_type}"
            target_idx += 1
            mutation_idx = 0

        if dimension == DIM_ASSERTION:
            target_step = steps[target_idx]
            orig_matcher = target_step.get("matcher")
            try:
                mutant_matcher, mutation_desc = self.mutate_matcher(orig_matcher)
            except ValueError as exc:
                return None, {"error": str(exc)}

            mutated_data["steps"][target_idx]["matcher"] = mutant_matcher
            mutated_data["steps"][target_idx]["description"] = (
                f"[CANARY ASSERTION MUTANT] {mutation_desc} | {target_step.get('description', '')}"
            )
        target_step = mutated_data["steps"][target_idx]
        target_desc = target_step.get("target") or target_step["type"]
        token = uuid.uuid4().hex
        header = mutated_data.setdefault("header", {})
        header["name"] = f"{header.get('name', scenario_path.stem)} [canary {dimension} {token}]"
        header["templateId"] = f"{header.get('templateId', scenario_path.stem)}_canary_{token}"
        mutant_dir = output_dir or scenario_path.parent
        mutant_dir.mkdir(parents=True, exist_ok=True)
        # Keep Windows paths short; input identity/dimension live in the header.
        mutant_path = mutant_dir / f"{token}.json"

        with open(mutant_path, "w", encoding="utf-8") as f:
            json.dump(mutated_data, f, indent=2, ensure_ascii=False)

        metadata = {
            "dimension": dimension,
            "step_index": target_idx,
            "step_type": target_step["type"],
            "mutation_step_index": mutation_idx,
            "target": target_desc,
            "mutation_desc": mutation_desc,
            "mutant_path": str(mutant_path),
            "mutant_sha256": file_sha256(mutant_path),
            "evidence_kind": "assertion_self_check" if dimension == DIM_ASSERTION else "environment_sensitivity",
        }
        return mutant_path, metadata

    @staticmethod
    def verify_kill(
        exit_code: int, runner_output: str, metadata: Dict[str, Any],
        report_path: Optional[Path] = None,
    ) -> Tuple[bool, str]:
        """Verify whether the mutant run was successfully and appropriately killed.

        Enforces the 3-state decision machine (Pillar 4):
        - exit_code == 0: [FAIL-GREEN] Fatal false green: mutant survived, block immediately.
        - exit_code != 0 && infra_crash: [INFRA_CRASH] Infrastructure/compiler error: reject.
        - exit_code != 0 && assertion kill: [KILL-SUCCESS] Accurate kill: mutant killed as expected.

        Returns (killed: bool, diagnostic_message: str).
        """
        step_idx = metadata.get("step_index", 0)
        target = metadata.get("target", "unknown")
        dim = metadata.get("dimension", "assertion")

        # 1. False Green Check: If runner exited with 0, mutant survived!
        if exit_code == 0:
            return False, (
                f"[FAIL-GREEN] FALSE GREEN DETECTED! Mutant ({dim}) survived execution with exit code 0. "
                f"The assertion for step #{step_idx} ({target}) is NOT sensitive to defects."
            )

        # 2. Infra Crash Check (E-2 fix): reject compiler, asset, loader, syntax failures
        infra_crash_indicators = [
            "failed to build wasm",
            "failed to build",
            "failed to load runtime",
            "failed to load",
            "compilation error",
            "syntaxerror",
            "syntax error",
            "linker error",
            "segmentation fault",
            "sigsegv",
            "out of memory",
        ]
        lower_out = runner_output.lower()
        for crash_hint in infra_crash_indicators:
            if crash_hint in lower_out:
                return False, (
                    f"[INFRA_CRASH] INFRA_CRASH: Runner failed with infrastructure or build error ('{crash_hint}'), "
                    f"not an assertion defect kill. Exit code {exit_code}: {runner_output[:200]}"
                )

        if exit_code != 1:
            return False, f"[INFRA_CRASH] Unexpected runner exit code {exit_code}"
        bound_report = metadata.get("report_path")
        mutant_file = metadata.get("mutant_path")
        if not bound_report or not mutant_file or not metadata.get("report_sha256"):
            return False, "[FAIL-UNCERTAIN] No run-bound structured report and mutant input"
        selected_report = Path(report_path or bound_report)
        scenario_path = Path(mutant_file)
        try:
            if selected_report.resolve() != Path(bound_report).resolve():
                return False, "[FAIL-UNCERTAIN] Report path is not bound to this run"
            if file_sha256(selected_report) != metadata["report_sha256"]:
                return False, "[FAIL-UNCERTAIN] Report hash changed after collection"
            if file_sha256(scenario_path) != metadata.get("mutant_sha256"):
                return False, "[FAIL-UNCERTAIN] Mutant input hash changed after collection"
            scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
            target_step = scenario["steps"][step_idx]
            if not is_business_assertion(target_step) or target_step.get("type") != metadata.get("step_type"):
                return False, "[FAIL-UNCERTAIN] Metadata does not identify the selected business assertion"
            if (target_step.get("target") or target_step["type"]) != target:
                return False, "[FAIL-UNCERTAIN] Metadata target differs from the selected input"
        except (OSError, ValueError, KeyError, IndexError, TypeError) as exc:
            return False, f"[FAIL-UNCERTAIN] Invalid bound input/report: {exc}"
        ok, reason = validate_scenario_report(selected_report, scenario_path, failure_index=step_idx)
        if not ok:
            return False, f"[FAIL-UNCERTAIN] {reason}"
        return True, f"[KILL-SUCCESS] Canary mutant successfully killed: evaluated step #{step_idx} failed as expected ({metadata.get('mutation_desc')}); evidence kind={metadata.get('evidence_kind')}"
