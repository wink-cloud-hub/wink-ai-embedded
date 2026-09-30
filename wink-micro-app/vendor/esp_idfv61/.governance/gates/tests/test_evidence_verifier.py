# SPDX-License-Identifier: Apache-2.0
"""
test_evidence_verifier.py
=========================
Unit tests for evidence_verifier.py and composite SHA-256 validation.
"""

import json
import hashlib
from pathlib import Path
import pytest

from evidence_verifier import (
    compute_file_sha256,
    compute_assets_composite_sha256,
    compute_scenario_sha256,
    verify_execution_report,
    verify_evidence,
)


@pytest.fixture
def dummy_assets_dir(tmp_path):
    assets = tmp_path / "unisim-assets"
    assets.mkdir()
    (assets / "wink_simulator.wasm").write_bytes(b"\x00asm\x01\x00\x00\x00")
    (assets / "wink_simulator.js").write_text("console.log('sim');", encoding="utf-8")
    (assets / "device-tree.json").write_text('{"name": "test"}', encoding="utf-8")
    return assets


@pytest.fixture
def dummy_scenario_file(tmp_path):
    scen = tmp_path / "test.scenario.json"
    scen.write_text('{"steps": []}', encoding="utf-8")
    return scen


@pytest.fixture
def dummy_passing_report(tmp_path):
    rep = tmp_path / "run-report.json"
    rep_content = {
        "timestamp": "2026-09-30T12:00:00Z",
        "total": 1,
        "passed": 1,
        "failed": 0,
        "results": [
            {
                "ok": True,
                "status": "passed",
                "summary": {
                    "totalSteps": 5,
                    "passedSteps": 5,
                    "failedSteps": 0,
                    "errorSteps": 0,
                },
            }
        ],
    }
    rep.write_text(json.dumps(rep_content), encoding="utf-8")
    return rep


def test_composite_assets_sha256(dummy_assets_dir):
    h_wasm = hashlib.sha256(b"\x00asm\x01\x00\x00\x00").hexdigest()
    h_js = hashlib.sha256("console.log('sim');".encode("utf-8")).hexdigest()
    h_tree = hashlib.sha256('{"name": "test"}'.encode("utf-8")).hexdigest()
    expected = hashlib.sha256(f"{h_wasm}\n{h_js}\n{h_tree}".encode("utf-8")).hexdigest()

    actual = compute_assets_composite_sha256(dummy_assets_dir)
    assert actual == expected


def test_composite_assets_missing_file_raises(tmp_path):
    incomplete = tmp_path / "incomplete-assets"
    incomplete.mkdir()
    (incomplete / "wink_simulator.wasm").write_bytes(b"wasm")
    # Missing js and tree
    with pytest.raises(FileNotFoundError):
        compute_assets_composite_sha256(incomplete)


def test_verify_execution_report_success(dummy_passing_report):
    ok, msg = verify_execution_report(dummy_passing_report)
    assert ok is True
    assert "passed" in msg.lower()


def test_verify_execution_report_failed_step(tmp_path):
    bad_rep = tmp_path / "bad-report.json"
    bad_content = {
        "results": [
            {
                "ok": False,
                "status": "failed",
                "summary": {"totalSteps": 5, "passedSteps": 4, "failedSteps": 1, "errorSteps": 0},
            }
        ]
    }
    bad_rep.write_text(json.dumps(bad_content), encoding="utf-8")
    ok, msg = verify_execution_report(bad_rep)
    assert ok is False
    assert "ok is False" in msg or "status is 'failed'" in msg


def test_verify_evidence_tampered_asset_hash(dummy_assets_dir, dummy_scenario_file, dummy_passing_report, tmp_path):
    assets_sha = compute_assets_composite_sha256(dummy_assets_dir)
    scen_sha = compute_scenario_sha256(dummy_scenario_file)

    entry = {
        "id": "esp.test",
        "target_app_dir": "test_app",
    }
    # Create fake directory structure in tmp_path
    app_dir = tmp_path / "test_app"
    app_dir.mkdir(parents=True)
    import shutil
    shutil.copytree(dummy_assets_dir, app_dir / "unisim-assets")

    # Tampered assets hash
    tampered_hash = "f" * 64
    execution_config = {
        "config_id": "cfg",
        "delivery_state": "verified",
        "acceptance": {"scenario_path": str(dummy_scenario_file.relative_to(tmp_path))},
        "evidence": {
            "run_id": "run-test",
            "assets_sha256": tampered_hash,
            "scenario_sha256": scen_sha,
            "execution_report_ref": str(dummy_passing_report),
        },
    }

    ok, errors = verify_evidence(entry, execution_config, ws_root=tmp_path, strict_disk=True)
    assert ok is False
    assert any("assets_sha256 mismatch" in e for e in errors)


def test_verify_evidence_tampered_scenario_hash(dummy_assets_dir, dummy_scenario_file, dummy_passing_report, tmp_path):
    assets_sha = compute_assets_composite_sha256(dummy_assets_dir)

    entry = {
        "id": "esp.test",
        "target_app_dir": "test_app",
    }
    app_dir = tmp_path / "test_app"
    app_dir.mkdir(parents=True, exist_ok=True)
    import shutil
    if not (app_dir / "unisim-assets").exists():
        shutil.copytree(dummy_assets_dir, app_dir / "unisim-assets")

    rel_scen = dummy_scenario_file.relative_to(tmp_path)
    execution_config = {
        "config_id": "cfg",
        "delivery_state": "verified",
        "acceptance": {"scenario_path": str(rel_scen)},
        "evidence": {
            "run_id": "run-test",
            "assets_sha256": assets_sha,
            "scenario_sha256": "e" * 64,  # Tampered
            "execution_report_ref": str(dummy_passing_report),
        },
    }

    ok, errors = verify_evidence(entry, execution_config, ws_root=tmp_path, strict_disk=True)
    assert ok is False
    assert any("scenario_sha256 mismatch" in e for e in errors)
