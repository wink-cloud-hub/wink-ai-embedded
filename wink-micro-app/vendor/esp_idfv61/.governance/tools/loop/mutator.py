# SPDX-License-Identifier: Apache-2.0
"""
Anti-False-Green Canary Mutation Engine
========================================
Autonomously creates non-equivalent mutants of *.scenario.json assertions
and verifies that the simulation engine reliably kills them (Fail-Loud),
proving non-tautology and absence of false greens.
"""
from __future__ import annotations

import copy
import json
from pathlib import Path
from typing import Any, Dict, Optional, Tuple


class CanaryMutator:
    """Detects primary domain assertion steps and injects calibrated defects."""

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
    def mutate_matcher(matcher: Any) -> Tuple[Any, str]:
        """Apply mathematical/semantic mutation operator to a matcher.

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

    def create_mutant_file(self, scenario_path: Path) -> Tuple[Optional[Path], Dict[str, Any]]:
        """Parse scenario, generate mutant, write temporary file.

        Returns (mutant_path, metadata) or (None, metadata) if no assert step found.
        """
        with open(scenario_path, "r", encoding="utf-8") as f:
            data = json.load(f)

        steps = data.get("steps", [])
        target_idx = self.identify_target_step(steps)
        if target_idx is None:
            return None, {"error": "No assertion step found to mutate"}

        target_step = steps[target_idx]
        orig_matcher = target_step.get("matcher")
        mutant_matcher, mutation_desc = self.mutate_matcher(orig_matcher)

        mutated_data = copy.deepcopy(data)
        mutated_data["steps"][target_idx]["matcher"] = mutant_matcher
        mutated_data["steps"][target_idx]["description"] = (
            f"[CANARY MUTANT] {mutation_desc} | {target_step.get('description', '')}"
        )

        mutant_path = scenario_path.parent / f"{scenario_path.stem}.canary_mutant.json"
        with open(mutant_path, "w", encoding="utf-8") as f:
            json.dump(mutated_data, f, indent=2, ensure_ascii=False)

        metadata = {
            "step_index": target_idx,
            "target": target_step.get("target"),
            "original_matcher": orig_matcher,
            "mutant_matcher": mutant_matcher,
            "mutation_desc": mutation_desc,
            "mutant_path": str(mutant_path),
        }
        return mutant_path, metadata

    @staticmethod
    def verify_kill(exit_code: int, runner_output: str, metadata: Dict[str, Any]) -> Tuple[bool, str]:
        """Verify whether the mutant run was successfully and appropriately killed.

        Returns (killed: bool, diagnostic_message: str).
        """
        step_idx = metadata.get("step_index", 0)

        # 1. False Green Check: If runner exited with 0, mutant survived!
        if exit_code == 0:
            return False, (
                f"FALSE GREEN DETECTED! Mutant survived execution with exit code 0. "
                f"The assertion for step #{step_idx} ({metadata.get('target')}) is NOT sensitive to defects."
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
                    f"INFRA_CRASH: Runner failed with infrastructure or build error ('{crash_hint}'), "
                    f"not an assertion defect kill. Exit code {exit_code}: {runner_output[:200]}"
                )

        # 3. Check that the failure specifically mentions the step or matcher mismatch
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
        ]
        has_assertion_fail = any(hint in lower_out for hint in assertion_fail_hints)

        if step_mention or has_assertion_fail:
            return True, f"Canary mutant successfully killed: step #{step_idx} failed as expected ({metadata.get('mutation_desc')})."

        return False, f"Runner failed with exit code {exit_code}, but failure could not be confirmed as assertion kill: {runner_output[:200]}"
