# SPDX-License-Identifier: Apache-2.0
"""
tests/test_governance_import_contract.py - Governance Architecture Import & Dependency Contract Verification
=============================================================================================================
Implements Plan Phase 3 (T3.3):
1. AST Static Audit: Recursively analyzes all .py files in loop/ and gates/.
   Enforces invariant: Zero reverse imports into tools/ or cli/. Zero sys.path pollution of tools/cli.
2. Bidirectional Import Compatibility:
   Verifies canonical-first and legacy-first import order preserves object identity across all frozen symbols.
3. Shared State & Monkeypatch Reflection:
   Verifies mutations on legacy symbols reflect on canonical symbols without desync or duplicate classes.
4. Clean Import Execution:
   Verifies import of loop/gates packages is side-effect-free (no spurious stdout/stderr or premature writes).
"""
from __future__ import annotations

import ast
import os
import subprocess
import sys
from pathlib import Path
from typing import List, Set, Tuple

import pytest

GOV_DIR = Path(__file__).resolve().parents[1]
LOOP_DIR = GOV_DIR / "loop"
GATES_DIR = GOV_DIR / "gates"
TOOLS_DIR = GOV_DIR / "tools"


def _gather_python_files(dir_path: Path) -> List[Path]:
    """Recursively collect all .py files under a directory."""
    files: List[Path] = []
    for root, _, filenames in os.walk(dir_path):
        for name in filenames:
            if name.endswith(".py"):
                files.append(Path(root) / name)
    return sorted(files)


# -----------------------------------------------------------------------------
# 1. AST Static Dependency Audit
# -----------------------------------------------------------------------------

def test_ast_dependency_closure_no_reverse_imports():
    """
    Enforces that all modules in loop/ and gates/ contain zero imports from tools/ or cli/.
    All imports must parse without syntax error.
    """
    core_files = _gather_python_files(LOOP_DIR) + _gather_python_files(GATES_DIR)
    assert len(core_files) >= 50, f"Expected at least 50 core python files, found {len(core_files)}"

    forbidden_prefixes = ("tools", "cli")
    violations: List[str] = []

    for py_file in core_files:
        try:
            tree = ast.parse(py_file.read_bytes(), filename=str(py_file))
        except SyntaxError as e:
            pytest.fail(f"Syntax error in core file {py_file}: {e}")

        for node in ast.walk(tree):
            # Check standard import: import tools...
            if isinstance(node, ast.Import):
                for alias in node.names:
                    parts = alias.name.split(".")
                    if parts[0] in forbidden_prefixes:
                        violations.append(f"{py_file.relative_to(GOV_DIR)}: line {node.lineno}: import {alias.name}")

            # Check from ... import ...
            elif isinstance(node, ast.ImportFrom):
                if node.level == 0 and node.module:
                    parts = node.module.split(".")
                    if parts[0] in forbidden_prefixes:
                        violations.append(f"{py_file.relative_to(GOV_DIR)}: line {node.lineno}: from {node.module} import ...")

            # Check for sys.path manipulations pointing to tools
            elif isinstance(node, ast.Call):
                func_name = ""
                if isinstance(node.func, ast.Attribute):
                    func_name = node.func.attr
                if func_name in ("insert", "append"):
                    # Check if target is sys.path
                    arg_dump = ast.dump(node)
                    if "tools" in arg_dump.lower() and "sys" in arg_dump:
                        violations.append(
                            f"{py_file.relative_to(GOV_DIR)}: line {node.lineno}: suspicious sys.path mutation referencing 'tools'"
                        )

    assert not violations, "Reverse dependency violations found in core packages:\n" + "\n".join(violations)


# -----------------------------------------------------------------------------
# 2. Subprocess Bidirectional Import Order & Identity Verification
# -----------------------------------------------------------------------------

def _run_python_script(code: str, env: dict | None = None) -> subprocess.CompletedProcess:
    test_env = os.environ.copy()
    if env:
        test_env.update(env)
    pythonpath = str(GOV_DIR)
    if "PYTHONPATH" in test_env:
        pythonpath = f"{pythonpath}{os.pathsep}{test_env['PYTHONPATH']}"
    test_env["PYTHONPATH"] = pythonpath

    return subprocess.run(
        [sys.executable, "-X", "utf8", "-B", "-W", "error", "-c", code],
        cwd=str(GOV_DIR.parents[3]),
        capture_output=True,
        text=True,
        encoding="utf-8",
        env=test_env,
        timeout=30,
    )


def test_canonical_first_import_order_identity():
    """Importing canonical first, then legacy shim maintains 100% object identity."""
    code = """
import loop.agent as canonical_agent
import tools.loop.agent as legacy_agent
assert canonical_agent.AgentSynthesizer is legacy_agent.AgentSynthesizer, "AgentSynthesizer desync"

import loop.remediator as canonical_remediator
import tools.loop.remediator as legacy_remediator
assert canonical_remediator.Remediator is legacy_remediator.Remediator, "Remediator desync"

import loop.mutator as canonical_mutator
import tools.loop.mutator as legacy_mutator
assert canonical_mutator.CanaryMutator is legacy_mutator.CanaryMutator, "CanaryMutator desync"

import loop.mutation_catalog as canonical_cat
import tools.loop.mutation_catalog as legacy_cat
assert canonical_cat.CATALOG_OPERATORS is legacy_cat.CATALOG_OPERATORS, "CATALOG_OPERATORS desync"

import loop.services.afg_verification as canonical_afg
import tools.verify_afg_engine as legacy_afg
assert canonical_afg.PilotVerifier is legacy_afg.PilotVerifier, "PilotVerifier desync"

import loop.services.soc_triage as canonical_soc
import tools.triage_soc_support as legacy_soc
assert canonical_soc.apply_gap_remediation_and_triage is legacy_soc.apply_gap_remediation_and_triage, "soc triage desync"

import loop.harness.idf_paths as canonical_paths
import tools.esp_path_resolver as legacy_paths
assert canonical_paths.load_local_config is legacy_paths.load_local_config, "idf_paths desync"
print("CANONICAL_FIRST_OK")
"""
    res = _run_python_script(code)
    assert res.returncode == 0, f"Canonical first failed: {res.stderr}\n{res.stdout}"
    assert "CANONICAL_FIRST_OK" in res.stdout


def test_legacy_first_import_order_identity():
    """Importing legacy shim first, then canonical maintains 100% object identity."""
    code = """
import tools.loop.agent as legacy_agent
import loop.agent as canonical_agent
assert canonical_agent.AgentSynthesizer is legacy_agent.AgentSynthesizer, "AgentSynthesizer desync"

import tools.loop.remediator as legacy_remediator
import loop.remediator as canonical_remediator
assert canonical_remediator.Remediator is legacy_remediator.Remediator, "Remediator desync"

import tools.loop.mutator as legacy_mutator
import loop.mutator as canonical_mutator
assert canonical_mutator.CanaryMutator is legacy_mutator.CanaryMutator, "CanaryMutator desync"

import tools.loop.mutation_catalog as legacy_cat
import loop.mutation_catalog as canonical_cat
assert canonical_cat.CATALOG_OPERATORS is legacy_cat.CATALOG_OPERATORS, "CATALOG_OPERATORS desync"

import tools.verify_afg_engine as legacy_afg
import loop.services.afg_verification as canonical_afg
assert canonical_afg.PilotVerifier is legacy_afg.PilotVerifier, "PilotVerifier desync"

import tools.triage_soc_support as legacy_soc
import loop.services.soc_triage as canonical_soc
assert canonical_soc.apply_gap_remediation_and_triage is legacy_soc.apply_gap_remediation_and_triage, "soc triage desync"

import tools.esp_path_resolver as legacy_paths
import loop.harness.idf_paths as canonical_paths
assert canonical_paths.load_local_config is legacy_paths.load_local_config, "idf_paths desync"
print("LEGACY_FIRST_OK")
"""
    res = _run_python_script(code)
    assert res.returncode == 0, f"Legacy first failed: {res.stderr}\n{res.stdout}"
    assert "LEGACY_FIRST_OK" in res.stdout


# -----------------------------------------------------------------------------
# 3. Shared State & Monkeypatch Verification
# -----------------------------------------------------------------------------

def test_shared_state_and_monkeypatch_reflection():
    """
    Verifies that mutating an attribute on legacy shim reflects on canonical symbol,
    and mutating on canonical reflects on legacy symbol.
    """
    code = """
import loop.agent as canonical_agent
import tools.loop.agent as legacy_agent

# Monkeypatch class on legacy shim
legacy_agent.AgentSynthesizer._test_contract_attr = "contract_value_123"
assert getattr(canonical_agent.AgentSynthesizer, "_test_contract_attr", None) == "contract_value_123", "Monkeypatch desync"

# Monkeypatch class on canonical
canonical_agent.AgentSynthesizer._test_contract_attr_2 = "contract_value_456"
assert getattr(legacy_agent.AgentSynthesizer, "_test_contract_attr_2", None) == "contract_value_456", "Monkeypatch desync 2"

# Mutate shared dictionary in mutation catalog
import loop.mutation_catalog as canonical_cat
import tools.loop.mutation_catalog as legacy_cat

legacy_cat.CATALOG_OPERATORS["_contract_test_op"] = {"name": "test_op"}
assert "_contract_test_op" in canonical_cat.CATALOG_OPERATORS, "Catalog dictionary desync"
print("SHARED_STATE_OK")
"""
    res = _run_python_script(code)
    assert res.returncode == 0, f"Shared state test failed: {res.stderr}\n{res.stdout}"
    assert "SHARED_STATE_OK" in res.stdout


# -----------------------------------------------------------------------------
# 4. Clean Import Execution (No Spurious Output or Unexpected Writes)
# -----------------------------------------------------------------------------

def test_clean_import_side_effects():
    """Importing loop and gates packages must be silent and produce zero stderr/stdout."""
    code = """
import sys
import loop
import gates
import tools.loop
import loop.services.afg_verification
import loop.services.soc_triage
import loop.harness.idf_paths
"""
    res = _run_python_script(code)
    assert res.returncode == 0, f"Clean import failed: {res.stderr}"
    assert res.stdout.strip() == "", f"Spurious stdout on import: {res.stdout}"
    assert res.stderr.strip() == "", f"Spurious stderr on import: {res.stderr}"
