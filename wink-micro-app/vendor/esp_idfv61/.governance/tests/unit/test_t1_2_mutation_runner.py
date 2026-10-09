# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for Task T1.2: 生效的 L1/L2 变异、归因与恢复
Validates AC-1.2 & AC-1.3 acceptance criteria:
- L1 simulation-level fault injection attribution.
- L2 compiler mutation budget enforcement (<= 1 per claim).
- Real AST/regex source mutation inside isolated sandbox (original preserved).
- Attribution four-state: MUTANT_KILLED, MUTANT_SURVIVED, MUTATION_BUILD_FAILED, MUTATION_NOT_ACTIVATED.
- Recovery verification with dirty_state_cleared=True.
"""
from __future__ import annotations

import json
from pathlib import Path
import pytest

from loop.afg.mutation_runner import MutationRunner, MutationBudgetExceededError
from loop.afg.build_sandbox import BuildSandbox


def test_l1_injection_attributions():
    """Validates L1 fault injection attribution for all polarity branches."""
    runner = MutationRunner(workspace_root=Path("."))

    # 1. Expected to handle fault, successfully handled -> FAULT_HANDLED_PASS
    res1 = runner.run_l1_injection(
        {"id": "L1-TEST-1", "expected_behavior": "fault_handling_pass"},
        lambda inj: (True, {"handled": True})
    )
    assert res1["witness_status"] == "FAULT_HANDLED_PASS"
    assert res1["kill_valid"] is True

    # 2. Expected to handle fault, but unhandled crash -> FAULT_UNHANDLED_FAIL
    res2 = runner.run_l1_injection(
        {"id": "L1-TEST-2", "expected_behavior": "fault_handling_pass"},
        lambda inj: (False, {"error": "unhandled"})
    )
    assert res2["witness_status"] == "FAULT_UNHANDLED_FAIL"
    assert res2["kill_valid"] is False

    # 3. Expected to kill assertion, assertion caught failure -> MUTANT_KILLED
    res3 = runner.run_l1_injection(
        {"id": "L1-TEST-3", "expected_behavior": "mutant_kill_fail"},
        lambda inj: (False, {"assertion_failed": True})
    )
    assert res3["witness_status"] == "MUTANT_KILLED"
    assert res3["kill_valid"] is True

    # 4. Expected to kill assertion, but survived -> MUTANT_SURVIVED
    res4 = runner.run_l1_injection(
        {"id": "L1-TEST-4", "expected_behavior": "mutant_kill_fail"},
        lambda inj: (True, {"unexpected_pass": True})
    )
    assert res4["witness_status"] == "MUTANT_SURVIVED"
    assert res4["kill_valid"] is False


def test_l2_budget_exceeded(tmp_path: Path):
    """Enforces L2 compiler mutation budget <= 1 per claim."""
    runner = MutationRunner(workspace_root=tmp_path, max_l2_per_claim=1)

    with pytest.raises(MutationBudgetExceededError):
        runner.run_l2_mutation(
            claim_id="claim.test.boot",
            operator_def={"id": "L2-OP-TEST", "budget": 2},
            app_dir=tmp_path / "app",
            sandbox_dir=tmp_path / "sandbox",
            compile_fn=lambda p: (True, p / "bin", ""),
            scenario_runner_fn=lambda p: (True, {})
        )


def test_l2_real_source_mutation_and_kill(tmp_path: Path):
    """Verifies that L2 mutates sandbox source without altering original app, and kills mutant."""
    app_dir = tmp_path / "app"
    app_dir.mkdir(parents=True)
    c_source = (
        '#include <stdio.h>\n'
        'void app_main(void) {\n'
        '    printf("Hello from test app\\n");\n'
        '}\n'
    )
    main_c = app_dir / "main.c"
    main_c.write_text(c_source, encoding="utf-8")

    sandbox_dir = tmp_path / "sandbox"
    sandbox_dir.mkdir(parents=True)

    runner = MutationRunner(workspace_root=tmp_path)

    # Operator targeting app_main with statement_delete
    operator_def = {
        "id": "L2-OP-DELETE-APP-MAIN",
        "target": "app_main",
        "mutation_type": "statement_delete",
        "budget": 1,
    }

    # Mock compiler that creates a fake binary
    def mock_compile(isolated_app: Path):
        bin_file = isolated_app / "wink_simulator.wasm"
        bin_file.write_bytes(b"\x00asm\x01\x00\x00\x00")
        return True, bin_file, ""

    # Mock scenario runner: mutant causes assertion failure -> returns False
    def mock_scenario(artifact: Path):
        return False, {"error": "Expected string not printed"}

    res = runner.run_l2_mutation(
        claim_id="claim.test.main",
        operator_def=operator_def,
        app_dir=app_dir,
        sandbox_dir=sandbox_dir,
        compile_fn=mock_compile,
        scenario_runner_fn=mock_scenario,
    )

    # 1. Original source in app_dir MUST be completely untouched!
    assert main_c.read_text(encoding="utf-8") == c_source

    # 2. Mutant result should be MUTANT_KILLED
    assert res["witness_status"] == "MUTANT_KILLED"
    assert res["kill_valid"] is True
    assert "diff_patch" in res
    assert "statement_delete" in res["diff_patch"]


def test_l2_mutation_not_activated_when_unmatched(tmp_path: Path):
    """Verifies that non-matching operators result in MUTATION_NOT_ACTIVATED."""
    app_dir = tmp_path / "app"
    app_dir.mkdir(parents=True)
    main_c = app_dir / "main.c"
    main_c.write_text('void other_func(void) {}\n', encoding="utf-8")

    sandbox_dir = tmp_path / "sandbox"
    sandbox_dir.mkdir(parents=True)

    runner = MutationRunner(workspace_root=tmp_path)

    # Operator targeting nonexistent function
    operator_def = {
        "id": "L2-OP-NONEXISTENT",
        "target": "nonexistent_func",
        "mutation_type": "statement_delete",
        "budget": 1,
    }

    res = runner.run_l2_mutation(
        claim_id="claim.test.none",
        operator_def=operator_def,
        app_dir=app_dir,
        sandbox_dir=sandbox_dir,
        compile_fn=lambda p: (True, p / "bin", ""),
        scenario_runner_fn=lambda p: (False, {})
    )

    assert res["witness_status"] == "MUTATION_NOT_ACTIVATED"
    assert res["kill_valid"] is False


def test_l2_mutation_build_failed_not_kill(tmp_path: Path):
    """Compiler failure must be attributed as MUTATION_BUILD_FAILED, not MUTANT_KILLED (META-23)."""
    app_dir = tmp_path / "app"
    app_dir.mkdir(parents=True)
    (app_dir / "main.c").write_text('void app_main(void) {}\n', encoding="utf-8")

    sandbox_dir = tmp_path / "sandbox"
    sandbox_dir.mkdir(parents=True)

    runner = MutationRunner(workspace_root=tmp_path)

    operator_def = {
        "id": "L2-OP-TEST",
        "target": "app_main",
        "mutation_type": "statement_delete",
        "budget": 1,
    }

    # Mock compiler that fails
    def mock_compile_fail(isolated_app: Path):
        return False, None, "syntax error: unexpected token"

    res = runner.run_l2_mutation(
        claim_id="claim.test.build",
        operator_def=operator_def,
        app_dir=app_dir,
        sandbox_dir=sandbox_dir,
        compile_fn=mock_compile_fail,
        scenario_runner_fn=lambda p: (False, {})
    )

    assert res["witness_status"] == "MUTATION_BUILD_FAILED"
    assert res["kill_valid"] is False
    assert "syntax error" in res["error"]


def test_l2_mutant_survived(tmp_path: Path):
    """Mutant passing assertions is classified as MUTANT_SURVIVED."""
    app_dir = tmp_path / "app"
    app_dir.mkdir(parents=True)
    (app_dir / "main.c").write_text('void app_main(void) {}\n', encoding="utf-8")

    sandbox_dir = tmp_path / "sandbox"
    sandbox_dir.mkdir(parents=True)

    runner = MutationRunner(workspace_root=tmp_path)

    operator_def = {
        "id": "L2-OP-TEST",
        "target": "app_main",
        "mutation_type": "statement_delete",
        "budget": 1,
    }

    def mock_compile(isolated_app: Path):
        b = isolated_app / "bin"
        b.write_bytes(b"dummy")
        return True, b, ""

    # Mock scenario passes -> assertion failed to catch mutant
    def mock_scenario_pass(artifact: Path):
        return True, {"all_assertions": "passed"}

    res = runner.run_l2_mutation(
        claim_id="claim.test.survived",
        operator_def=operator_def,
        app_dir=app_dir,
        sandbox_dir=sandbox_dir,
        compile_fn=mock_compile,
        scenario_runner_fn=mock_scenario_pass,
    )

    assert res["witness_status"] == "MUTANT_SURVIVED"
    assert res["kill_valid"] is False


def test_mutation_runner_recovery(tmp_path: Path):
    """Verifies that clean recovery baseline reports dirty_state_cleared=True."""
    app_dir = tmp_path / "app"
    app_dir.mkdir(parents=True)
    (app_dir / "main.c").write_text('void app_main(void) {}\n', encoding="utf-8")

    sandbox_dir = tmp_path / "sandbox"
    sandbox_dir.mkdir(parents=True)

    runner = MutationRunner(workspace_root=tmp_path)

    def mock_compile(isolated_app: Path):
        b = isolated_app / "bin"
        b.write_bytes(b"clean")
        return True, b, ""

    def mock_scenario(artifact: Path):
        return True, {"status": "passed"}

    rec = runner.run_recovery(
        app_dir=app_dir,
        sandbox_dir=sandbox_dir,
        compile_fn=mock_compile,
        scenario_runner_fn=mock_scenario,
    )

    assert rec["evidence_class"] == "recovery"
    assert rec["status"] == "PASS"
    assert rec["dirty_state_cleared"] is True
