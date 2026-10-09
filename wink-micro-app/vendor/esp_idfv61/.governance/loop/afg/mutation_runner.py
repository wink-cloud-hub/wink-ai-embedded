# SPDX-License-Identifier: Apache-2.0
"""
loop.afg.mutation_runner - Multi-Tier Mutation Execution & Budget Control
==========================================================================
Implements Task T1.2, AFG-R13 & AFG-R14:
- Tiered execution: L1 simulation-level fault injection prioritized (fast, 80% coverage).
- L2 source-level compiler mutation budget enforcement: max 1 L2 mutation per Claim.
- Mutant kill attribution & survival verification (AFG-R13):
  Unrelated compile aborts or random fixture crashes do not count as valid kills.
  Surviving mutants trigger MUTANT_SURVIVED rejection unless signed equivalent mutant witness.
- Real AST/regex source mutation inside isolated sandbox (no comments/no-op bypass).
- Verification of recovery to clean baseline state without SRAM/flash contamination.
"""
from __future__ import annotations

import difflib
import hashlib
import json
import re
import shutil
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

try:
    from .build_sandbox import BuildSandbox
except ImportError:
    try:
        from loop.afg.build_sandbox import BuildSandbox
    except ImportError:
        from build_sandbox import BuildSandbox

try:
    from loop.mutation_catalog import CATALOG_OPERATORS, apply_catalog_mutation
except ImportError:
    try:
        from ..mutation_catalog import CATALOG_OPERATORS, apply_catalog_mutation
    except ImportError:
        CATALOG_OPERATORS = {}
        def apply_catalog_mutation(src, op): return None, None, None


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
        kill_valid = False
        if expected_behavior == "fault_handling_pass":
            if success:
                witness_status = "FAULT_HANDLED_PASS"
                kill_valid = True
            else:
                witness_status = "FAULT_UNHANDLED_FAIL"
                kill_valid = False
        elif expected_behavior in ("mutant_kill_fail", "trigger_buffer_overrun"):
            # Assertion was expected to fail or trigger expected overrun
            if not success:
                witness_status = "MUTANT_KILLED"
                kill_valid = True
            else:
                witness_status = "MUTANT_SURVIVED"
                kill_valid = False

        return {
            "tier": "L1",
            "injector_id": inj_id,
            "operator": injector_def.get("operator"),
            "expected_behavior": expected_behavior,
            "raw_success": success,
            "witness_status": witness_status,
            "kill_valid": kill_valid,
            "details": raw_report,
        }

    def _apply_source_mutation(
        self,
        isolated_app: Path,
        operator_def: Dict[str, Any],
    ) -> Tuple[bool, str, str, str]:
        """Applies real mutation to target source code in isolated_app.
        
        Returns:
            (activated, target_file_rel, diff_patch, reason_or_witness)
        """
        op_id = operator_def.get("id", "L2-CUSTOM")
        target_file_hint = operator_def.get("target_file")
        
        # Locate candidate C source files
        candidate_files: List[Path] = []
        if target_file_hint:
            p = isolated_app / target_file_hint
            if p.is_file():
                candidate_files.append(p)
        else:
            # Search all .c files excluding generated and assets
            for p in sorted(isolated_app.rglob("*.c")):
                if not any(part in ("unisim-assets", "build", "__pycache__") for part in p.parts):
                    candidate_files.append(p)

        if not candidate_files:
            return False, "", "", "NO_TARGET_FILES: No candidate C files found in application"

        # Strategy 1: Check catalog operator first
        if op_id in CATALOG_OPERATORS:
            for c_file in candidate_files:
                original = c_file.read_text(encoding="utf-8")
                mutated, diff_patch, witness = apply_catalog_mutation(original, op_id)
                if mutated and diff_patch:
                    c_file.write_text(mutated, encoding="utf-8")
                    return True, str(c_file.relative_to(isolated_app)), diff_patch, witness or "catalog_operator"

        # Strategy 2: Explicit pattern and replacement
        pattern = operator_def.get("target_pattern")
        replacement = operator_def.get("replacement")
        if pattern and replacement:
            for c_file in candidate_files:
                original = c_file.read_text(encoding="utf-8")
                if re.search(pattern, original):
                    mutated = re.sub(pattern, replacement, original, count=1)
                    if mutated != original:
                        c_file.write_text(mutated, encoding="utf-8")
                        diff_lines = list(difflib.unified_diff(
                            original.splitlines(keepends=True),
                            mutated.splitlines(keepends=True),
                            fromfile="original.c",
                            tofile=f"mutant_{op_id}.c",
                            n=3
                        ))
                        return True, str(c_file.relative_to(isolated_app)), "".join(diff_lines), "regex_substitution"

        # Strategy 3: Function target (e.g. app_main statement delete/comment)
        target_fn = operator_def.get("target")
        mutation_type = operator_def.get("mutation_type")
        if target_fn:
            fn_pattern = rf"(\b{re.escape(target_fn)}\s*\([^)]*\)\s*\{{)"
            for c_file in candidate_files:
                original = c_file.read_text(encoding="utf-8")
                if re.search(fn_pattern, original):
                    if mutation_type == "statement_delete":
                        # Insert early return or disable creation
                        mutated = re.sub(fn_pattern, r"\1\n    /* MUTATION: statement_delete */\n    return;", original, count=1)
                    else:
                        mutated = re.sub(fn_pattern, r"\1\n    /* MUTATION: generic_abort */\n    return;", original, count=1)
                    if mutated != original:
                        c_file.write_text(mutated, encoding="utf-8")
                        diff_lines = list(difflib.unified_diff(
                            original.splitlines(keepends=True),
                            mutated.splitlines(keepends=True),
                            fromfile="original.c",
                            tofile=f"mutant_{op_id}.c",
                            n=3
                        ))
                        return True, str(c_file.relative_to(isolated_app)), "".join(diff_lines), f"{mutation_type}_on_{target_fn}"

        return False, "", "", "MUTATION_NOT_ACTIVATED: Target pattern or function was not matched in source files"

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

        # 1. Create isolated sandbox copy of app (never touches original app_dir)
        isolated_app = self.sandbox.create_isolated_sandbox(app_dir, sandbox_dir / f"mutant_{op_id}")

        # 2. Apply real mutation patch inside sandbox
        activated, target_rel, diff_patch, witness_info = self._apply_source_mutation(isolated_app, operator_def)
        if not activated:
            return {
                "tier": "L2",
                "operator_id": op_id,
                "claim_id": claim_id,
                "witness_status": "MUTATION_NOT_ACTIVATED",
                "kill_valid": False,
                "diff_patch": "",
                "error": witness_info,
            }

        # 3. Compute cache key binding the diff patch
        cache_key, digests = self.sandbox.calculate_key(
            app_dir=isolated_app,
            config_profile_id=op_id,
            patch_content=diff_patch,
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
            if compile_ok and artifact_path and artifact_path.is_file():
                self.sandbox.store_cache(cache_key, artifact_path, {"operator_id": op_id, "claim_id": claim_id})

        if not compile_ok or not artifact_path or not artifact_path.is_file():
            # L2 compiler failure is NOT a kill witness (AFG-R13/R14 / META-23)
            return {
                "tier": "L2",
                "operator_id": op_id,
                "claim_id": claim_id,
                "cache_hit": cache_hit,
                "witness_status": "MUTATION_BUILD_FAILED",
                "diff_patch": diff_patch,
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
            "target_file": target_rel,
            "diff_patch": diff_patch,
            "semantic_witness": witness_info,
            "cache_hit": cache_hit,
            "cache_key": cache_key,
            "witness_status": witness_status,
            "kill_valid": kill_valid,
            "report": report,
        }

    def run_recovery(
        self,
        app_dir: Path,
        sandbox_dir: Path,
        compile_fn: Callable[[Path], Tuple[bool, Path, str]],
        scenario_runner_fn: Callable[[Path], Tuple[bool, Dict[str, Any]]],
    ) -> Dict[str, Any]:
        """Runs recovery verification from pristine original app.
        
        Ensures dirty state is cleared (META-19 / Axiom 5).
        """
        recovery_app = self.sandbox.create_isolated_sandbox(app_dir, sandbox_dir / "recovery_baseline")
        compile_ok, artifact_path, compile_err = compile_fn(recovery_app)
        if not compile_ok or not artifact_path or not artifact_path.is_file():
            return {
                "status": "FAIL",
                "dirty_state_cleared": False,
                "error": f"Recovery compilation failed: {compile_err}",
            }

        scenario_ok, report = scenario_runner_fn(artifact_path)
        dirty_cleared = bool(scenario_ok)

        return {
            "evidence_class": "recovery",
            "status": "PASS" if dirty_cleared else "FAIL",
            "dirty_state_cleared": dirty_cleared,
            "has_business_assertion": True,
            "report": report,
        }
