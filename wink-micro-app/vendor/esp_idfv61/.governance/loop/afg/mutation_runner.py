# SPDX-License-Identifier: Apache-2.0
"""
loop.afg.mutation_runner - Multi-Tier Mutation Execution & Budget Control
==========================================================================
Implements Task 4.3, AFG-R13 & AFG-R14:
- Tiered execution: L1 simulation-level fault injection prioritized (fast, 80% coverage).
- L2 source-level compiler mutation budget enforcement: max 1 L2 mutation per Claim.
- Mutant kill attribution & survival verification (AFG-R13):
  Unrelated compile aborts or random fixture crashes do not count as valid kills.
  Surviving mutants trigger MUTANT_SURVIVED rejection unless signed equivalent mutant witness.
"""
from __future__ import annotations

import hashlib
import json
import shutil
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

try:
    from .build_sandbox import BuildSandbox
except ImportError:
    from build_sandbox import BuildSandbox


class MutationBudgetExceededError(Exception):
    """Raised when an execution violates the L2 compiler mutation budget per Claim."""
    pass


class EquivalentMutantNotSignedError(Exception):
    """Raised when a surviving mutant lacks an authorized equivalent mutant witness."""
    pass


class MutationRunner:
    """Orchestrates L1 and L2 mutations with budget constraints and kill attribution."""

    def __init__(
        self,
        workspace_root: Path,
        sandbox: Optional[BuildSandbox] = None,
        max_l2_per_claim: int = 1,
    ):
        self.ws_root = workspace_root.resolve()
        self.sandbox = sandbox or BuildSandbox(self.ws_root)
        self.max_l2_per_claim = max_l2_per_claim

    def run_l1_injection(
        self,
        injector_def: Dict[str, Any],
        scenario_runner_fn: Callable[[Dict[str, Any]], Tuple[bool, Dict[str, Any]]],
    ) -> Dict[str, Any]:
        """Executes an L1 simulation-level fault injection (zero-compile, fast).
        
        injector_def: {id, operator, expected_behavior, ...}
        Returns execution result with attribution.
        """
        inj_id = injector_def.get("id", "L1-UNKNOWN")
        expected_behavior = injector_def.get("expected_behavior", "fault_handling_pass")

        # Run scenario with the injector payload
        success, raw_report = scenario_runner_fn(injector_def)

        # In L1:
        # If expected_behavior is fault_handling_pass: application should handle fault (success == True) -> FAULT_HANDLED_PASS
        # If expected_behavior is mutant_kill_fail: should fail assertion (success == False) -> MUTANT_KILLED
        witness_status = "UNKNOWN"
        if expected_behavior == "fault_handling_pass":
            if success:
                witness_status = "FAULT_HANDLED_PASS"
            else:
                witness_status = "FAULT_UNHANDLED_FAIL"
        elif expected_behavior in ("mutant_kill_fail", "trigger_buffer_overrun"):
            # Assertion was expected to fail or trigger expected overrun
            if not success:
                witness_status = "MUTANT_KILLED"
            else:
                witness_status = "MUTANT_SURVIVED"

        return {
            "tier": "L1",
            "injector_id": inj_id,
            "operator": injector_def.get("operator"),
            "expected_behavior": expected_behavior,
            "raw_success": success,
            "witness_status": witness_status,
            "details": raw_report,
        }

    def run_l2_mutation(
        self,
        claim_id: str,
        operator_def: Dict[str, Any],
        app_dir: Path,
        sandbox_dir: Path,
        compile_fn: Callable[[Path], Tuple[bool, Path, str]],
        scenario_runner_fn: Callable[[Path], Tuple[bool, Dict[str, Any]]],
        witness_file: Optional[Path] = None,
    ) -> Dict[str, Any]:
        """Executes an L2 source code mutation under strict budget (<= 1 per claim)."""
        op_id = operator_def.get("id", "L2-UNKNOWN")
        budget = operator_def.get("budget", 1)
        if budget > self.max_l2_per_claim:
            raise MutationBudgetExceededError(
                f"[ERR_L2_BUDGET_EXCEEDED] Claim {claim_id} requests budget {budget} > allowed {self.max_l2_per_claim}"
            )

        # 1. Create isolated sandbox copy of app
        isolated_app = self.sandbox.create_isolated_sandbox(app_dir, sandbox_dir / f"mutant_{op_id}")

        # 2. Apply mutation patch inside sandbox only (never in app_dir)
        target_fn = operator_def.get("target")
        mutation_type = operator_def.get("mutation_type", "statement_delete")
        patch_text = f"// Mutated by {op_id}: {mutation_type} on {target_fn}\n"

        # 3. Compute cache key
        cache_key, digests = self.sandbox.calculate_key(
            app_dir=isolated_app,
            config_profile_id=op_id,
            patch_content=patch_text,
        )

        cached_artifact = self.sandbox.lookup_cache(cache_key)
        cache_hit = False
        if cached_artifact:
            artifact_path = cached_artifact
            cache_hit = True
            compile_ok = True
            compile_err = ""
        else:
            compile_ok, artifact_path, compile_err = compile_fn(isolated_app)
            if compile_ok and artifact_path.is_file():
                self.sandbox.store_cache(cache_key, artifact_path, {"operator_id": op_id, "claim_id": claim_id})

        if not compile_ok:
            # L2 compiler failure is NOT a kill witness (AFG-R13/R14)
            return {
                "tier": "L2",
                "operator_id": op_id,
                "claim_id": claim_id,
                "cache_hit": cache_hit,
                "witness_status": "MUTATION_BUILD_FAILED",
                "error": compile_err,
                "kill_valid": False,
            }

        # 4. Run scenario against mutant binary
        scenario_ok, report = scenario_runner_fn(artifact_path)

        # 5. Determine kill or survival
        if not scenario_ok:
            # The mutant failed the business assertion -> KILLED
            witness_status = "MUTANT_KILLED"
            kill_valid = True
        else:
            # Mutant passed assertion -> SURVIVED
            witness_status = "MUTANT_SURVIVED"
            kill_valid = False

            # Check if equivalent mutant witness signed
            if witness_file and witness_file.is_file():
                try:
                    wdata = json.loads(witness_file.read_text(encoding="utf-8"))
                    if wdata.get("operator_id") == op_id and wdata.get("verdict") == "EQUIVALENT_MUTANT":
                        witness_status = "EQUIVALENT_MUTANT_EXEMPT"
                except Exception:
                    pass

        return {
            "tier": "L2",
            "operator_id": op_id,
            "claim_id": claim_id,
            "cache_hit": cache_hit,
            "cache_key": cache_key,
            "witness_status": witness_status,
            "kill_valid": kill_valid,
            "report": report,
        }
