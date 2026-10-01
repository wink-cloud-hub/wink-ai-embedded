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


def test_verify_evidence_fail_closed_nonexistent_files(tmp_path):
    entry = {
        "id": "esp.test_nonexistent",
        "target_app_dir": "nonexistent_app",
    }
    execution_config = {
        "config_id": "cfg",
        "delivery_state": "verified",
        "acceptance": {"scenario_path": "nonexistent_app/nonexistent.scenario.json"},
        "evidence": {
            "run_id": "run-test",
            "assets_sha256": "a" * 64,
            "scenario_sha256": "b" * 64,
            "execution_report_ref": ".governance/reports/nonexistent_app/run-report.json",
        },
    }

    # Strict disk check MUST fail closed when declared files do not exist on disk
    ok, errors = verify_evidence(entry, execution_config, ws_root=tmp_path, strict_disk=True)
    assert ok is False
    assert any("not found on disk" in e for e in errors)


def test_verify_evidence_esp32_hardware(tmp_path):
    entry = {"id": "esp.test_hw", "target_app_dir": "test_hw"}
    log_file = tmp_path / "serial_log.txt"
    log_file.write_text("All 10 tests passed on ESP32-S3\n", encoding="utf-8")

    execution_config = {
        "config_id": "hw-esp32s3",
        "delivery_state": "verified",
        "evidence": {
            "backend": "esp32_hardware",
            "run_id": "run-hw-1",
            "firmware_elf_sha256": "1" * 64,
            "serial_log_report_ref": str(log_file),
            "board_type": "esp32s3_devkit_c",
            "verified_commit": "abcdef1",
            "verified_at": "2026-09-30T12:00:00Z",
        },
    }

    ok, errors = verify_evidence(entry, execution_config, ws_root=tmp_path, strict_disk=True)
    assert ok is True
    assert errors == []


def test_verify_evidence_build_system(tmp_path):
    entry = {"id": "esp.test_build", "target_app_dir": "test_build"}
    build_log = tmp_path / "build.log"
    build_log.write_text("Build succeeded with 0 errors\n", encoding="utf-8")

    execution_config = {
        "config_id": "build-toolchain",
        "delivery_state": "verified",
        "evidence": {
            "backend": "build_system",
            "run_id": "run-build-1",
            "build_log_ref": str(build_log),
            "compiler_version": "xtensa-esp32-elf-gcc 13.2.0",
            "verified_commit": "abcdef1",
            "verified_at": "2026-09-30T12:00:00Z",
        },
    }

    ok, errors = verify_evidence(entry, execution_config, ws_root=tmp_path, strict_disk=True)
    assert ok is True
    assert errors == []


def test_verify_execution_report_zero_steps_rejected(tmp_path):
    """Execution report with passedSteps=0 is strictly rejected."""
    empty_rep = tmp_path / "zero-steps-report.json"
    content = {
        "results": [
            {
                "ok": True,
                "status": "passed",
                "summary": {"totalSteps": 0, "passedSteps": 0, "failedSteps": 0, "errorSteps": 0},
            }
        ]
    }
    empty_rep.write_text(json.dumps(content), encoding="utf-8")
    ok, msg = verify_execution_report(empty_rep)
    assert ok is False
    assert "0 passed steps" in msg


def test_verify_execution_report_single_run_zero_steps_rejected(tmp_path):
    """Single run report format with passedSteps=0 is strictly rejected."""
    single_rep = tmp_path / "single-zero-steps.json"
    content = {
        "status": "passed",
        "summary": {"totalSteps": 0, "passedSteps": 0, "failedSteps": 0},
    }
    single_rep.write_text(json.dumps(content), encoding="utf-8")
    ok, msg = verify_execution_report(single_rep)
    assert ok is False
    assert "0 passed steps" in msg


def test_write_evidence_targeting_config_id(tmp_path, dummy_assets_dir, dummy_passing_report):
    from evidence_verifier import write_evidence_for_app

    # Create dummy app structure in tmp_path
    vendor_root = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61"
    gov_data = vendor_root / ".governance" / "data"
    gov_data.mkdir(parents=True)
    app_dir = vendor_root / "get-started" / "test_app"
    app_dir.mkdir(parents=True)
    (app_dir / "unisim-assets").mkdir(parents=True)
    for f in dummy_assets_dir.iterdir():
        (app_dir / "unisim-assets" / f.name).write_bytes(f.read_bytes())
    (app_dir / "unisim-scenarios").mkdir(parents=True)
    scen_file = app_dir / "unisim-scenarios" / "test.scenario.json"
    scen_file.write_text('{"steps": []}', encoding="utf-8")

    manifest = {
        "entries": [
            {
                "id": "esp.test_app",
                "target_app_dir": "get-started/test_app",
                "executions": [
                    {"config_id": "cfg_a", "delivery_state": "planned"},
                    {"config_id": "cfg_b", "delivery_state": "planned"},
                ],
            }
        ]
    }
    (gov_data / "checklist.data.json").write_text(json.dumps(manifest), encoding="utf-8")

    # Record evidence targeting cfg_b specifically
    ok = write_evidence_for_app(
        "esp.test_app",
        ws_root=tmp_path,
        report_src=dummy_passing_report,
        config_id="cfg_b",
        scenario_path=scen_file,
    )
    assert ok is True

    # Check manifest updated cfg_b, but NOT cfg_a
    updated = json.loads((gov_data / "checklist.data.json").read_text(encoding="utf-8"))
    execs = updated["entries"][0]["executions"]
    cfg_a = next(e for e in execs if e["config_id"] == "cfg_a")
    cfg_b = next(e for e in execs if e["config_id"] == "cfg_b")
    assert cfg_a["delivery_state"] == "planned"
    assert cfg_b["delivery_state"] == "verified"
    assert cfg_b["evidence"]["scenario_sha256"] != ""


