# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for run_gates.py exit code semantics (0, 1, 2).
"""

import sys
import pytest
import yaml
from pathlib import Path

import run_gates

FIXTURES_DIR = Path(__file__).resolve().parent.parent / "fixtures"


def test_zero_effective_rules_exits_code_2(monkeypatch, tmp_path):
    empty_yaml = tmp_path / "empty_gates.yaml"
    empty_yaml.write_text(yaml.safe_dump({"spec_version": "2.0.0", "rules": []}), encoding="utf-8")

    with pytest.raises(SystemExit) as exc_info:
        run_gates.main([
            "--config", str(empty_yaml),
            "--mode", "pr",
            "--manifest", str(FIXTURES_DIR / "minimal_manifest_v2.json"),
            "--catalog", str(FIXTURES_DIR / "minimal_catalog.yaml"),
            "--quarantine", str(FIXTURES_DIR / "quarantine_sample.yaml"),
            "--allow-empty-diff",
        ])
    assert exc_info.value.code == 2


def test_broken_module_path_exits_code_2(tmp_path):
    broken_yaml = tmp_path / "broken_gates.yaml"
    broken_yaml.write_text(yaml.safe_dump({
        "spec_version": "2.0.0",
        "rules": [
            {
                "id": "g1.broken",
                "gate": 1,
                "modes": ["pr"],
                "module": "rules.non_existent_module_xyz",
            }
        ]
    }), encoding="utf-8")

    with pytest.raises(SystemExit) as exc_info:
        run_gates.main([
            "--config", str(broken_yaml),
            "--mode", "pr",
            "--manifest", str(FIXTURES_DIR / "minimal_manifest_v2.json"),
            "--catalog", str(FIXTURES_DIR / "minimal_catalog.yaml"),
            "--quarantine", str(FIXTURES_DIR / "quarantine_sample.yaml"),
            "--allow-empty-diff",
        ])
    assert exc_info.value.code == 2


def test_all_pass_exits_code_0(tmp_path):
    # Runs g1.path_unique on minimal_manifest_v2 (clean)
    with pytest.raises(SystemExit) as exc_info:
        run_gates.main([
            "--rule", "g1.path_unique",
            "--mode", "pr",
            "--manifest", str(FIXTURES_DIR / "minimal_manifest_v2.json"),
            "--catalog", str(FIXTURES_DIR / "minimal_catalog.yaml"),
            "--quarantine", str(FIXTURES_DIR / "quarantine_sample.yaml"),
            "--allow-empty-diff",
        ])
    assert exc_info.value.code == 0


def test_error_finding_exits_code_1(tmp_path):
    # Manifest with duplicate upstream_path
    bad_manifest = tmp_path / "bad_manifest.json"
    bad_manifest.write_text(__import__("json").dumps({
        "spec_version": "2.0.0",
        "entries": [
            {"id": "esp.a", "display_id": 1, "upstream_path": "examples/dup", "executions": []},
            {"id": "esp.b", "display_id": 2, "upstream_path": "examples/dup", "executions": []},
        ]
    }), encoding="utf-8")

    with pytest.raises(SystemExit) as exc_info:
        run_gates.main([
            "--rule", "g1.path_unique",
            "--mode", "pr",
            "--manifest", str(bad_manifest),
            "--catalog", str(FIXTURES_DIR / "minimal_catalog.yaml"),
            "--quarantine", str(FIXTURES_DIR / "quarantine_sample.yaml"),
            "--allow-empty-diff",
        ])
    assert exc_info.value.code == 1
