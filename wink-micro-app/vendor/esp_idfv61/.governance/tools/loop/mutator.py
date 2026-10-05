# SPDX-License-Identifier: Apache-2.0
"""
Anti-False-Green Canary 3D Mutation Engine
===========================================
Autonomously creates non-equivalent mutants of *.scenario.json across three dimensions:
- Dimension A (Assertion Mutant): Modifies expected matchers to test assertion sensitivity.
- Dimension B (Stimulus Mutant): Corrupts external environment/input payloads.
- Dimension C (Platform Fault Mutant): Injects underlying hardware/protocol faults via probes.

Verifies that the simulation engine reliably kills them (Fail-Loud), proving non-tautology
and absence of false greens.
"""
from __future__ import annotations

import copy
import json
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

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
        candidate_indices = []
        for i, step in enumerate(steps):
            stype = step.get("type", "")
            if not stype.startswith("ASSERT_"):
                continue
            target = step.get("target", "")
            # Skip pure power assertions if other domain assertions exist
            if not target.startswith("power:"):
                candidate_indices.append(i)

        if candidate_indices:
            # Pick the last domain assertion (representative of end-to-end outcome)
            return candidate_indices[-1]

        # Fallback to any assertion step
        for i, step in reversed(list(enumerate(steps))):
            if step.get("type", "").startswith("ASSERT_"):
                return i
        return None

    @staticmethod
    def identify_stimulus_step(steps: list[dict[str, Any]]) -> Optional[int]:
        """Find the index of the external stimulus or fixture injection step to mutate."""
        for i, step in enumerate(steps):
            stype = step.get("type", "")
            if stype.startswith("INJECT_") or "routes" in step or "busPayload" in step:
                return i
        return None

    @staticmethod
    def mutate_matcher(matcher: Any) -> Tuple[Any, str]:
        """Apply mathematical/semantic mutation operator to a matcher (Dimension A).

        Returns (mutated_matcher, human_readable_mutation_desc).
        """
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

            # Generic dict fallback: append bad key
            mutant_dict["$ne_mutant"] = "__MUTANT__"
            return mutant_dict, "dict matcher corrupted"

        return "__UNEXPECTED_MUTANT__", "generic fallback mutant"

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

        mutant_step["corrupted_stimulus"] = True
        return mutant_step, "general stimulus corrupted"

    def create_mutant_file(
        self,
        scenario_path: Path,
        dimension: str = DIM_ASSERTION,
        fault_domain: str = "i2c",
        fault_type: str = "nack",
        fault_param: int = 0,
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

        steps = data.get("steps", [])
        mutated_data = copy.deepcopy(data)

        if dimension == DIM_STIMULUS:
            stim_idx = self.identify_stimulus_step(steps)
            if stim_idx is not None:
                mut_step, mutation_desc = self.mutate_stimulus(steps[stim_idx])
                mutated_data["steps"][stim_idx] = mut_step
                mutated_data["steps"][stim_idx]["description"] = (
                    f"[CANARY STIMULUS MUTANT] {mutation_desc} | {steps[stim_idx].get('description', '')}"
                )
                target_idx = stim_idx
                target_desc = steps[stim_idx].get("type", "stimulus")
            else:
                # Fallback to assertion mutation if no stimulus step found
                dimension = DIM_ASSERTION

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
            target_idx = 0
            target_desc = f"fault:{fault_domain}:{fault_type}"

        if dimension == DIM_ASSERTION:
            target_idx = self.identify_target_step(steps)
            if target_idx is None:
                return None, {"error": "No assertion step found to mutate"}

            target_step = steps[target_idx]
            orig_matcher = target_step.get("matcher")
            mutant_matcher, mutation_desc = self.mutate_matcher(orig_matcher)

            mutated_data["steps"][target_idx]["matcher"] = mutant_matcher
            mutated_data["steps"][target_idx]["description"] = (
                f"[CANARY ASSERTION MUTANT] {mutation_desc} | {target_step.get('description', '')}"
            )
            target_desc = target_step.get("target")

        # Determine output mutant filename
        if dimension == DIM_ASSERTION:
            mutant_path = scenario_path.parent / f"{scenario_path.stem}.canary_mutant.json"
        else:
            mutant_path = scenario_path.parent / f"{scenario_path.stem}.canary_{dimension}_mutant.json"

        with open(mutant_path, "w", encoding="utf-8") as f:
            json.dump(mutated_data, f, indent=2, ensure_ascii=False)

        metadata = {
            "dimension": dimension,
            "step_index": target_idx,
            "target": target_desc,
            "mutation_desc": mutation_desc,
            "mutant_path": str(mutant_path),
        }
        return mutant_path, metadata

    @staticmethod
    def verify_kill(exit_code: int, runner_output: str, metadata: Dict[str, Any]) -> Tuple[bool, str]:
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

        # 3. Check that the failure specifically mentions the step, matcher mismatch, or expected fault
        step_mention = (
            f"Step #{step_idx + 1}" in runner_output
            or f"stepIndex\": {step_idx}" in runner_output
            or f"step #{step_idx}" in lower_out
        )
        assertion_fail_hints = [
            "assert_point",
            "assert_bus_payload",
            "assert_waveform",
            "assert_sequence",
            "matcher mismatch",
            "outside range",
            "expected",
            "got",
            "assertion failed",
            "step failed",
            "failedsteps",
            "failed_steps",
            "disconnected",
            "timeout",
            "esp_err",
        ]
        has_assertion_fail = any(hint in lower_out for hint in assertion_fail_hints)

        if step_mention or has_assertion_fail:
            return True, f"[KILL-SUCCESS] Canary mutant successfully killed: step #{step_idx} failed as expected ({metadata.get('mutation_desc')})."

        return False, f"[FAIL-UNCERTAIN] Runner failed with exit code {exit_code}, but failure could not be confirmed as assertion kill: {runner_output[:200]}"
