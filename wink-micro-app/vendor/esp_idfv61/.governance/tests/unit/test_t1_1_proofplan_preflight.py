# SPDX-License-Identifier: GPL-3.0-only
"""
Unit tests for Task T1.1: 真实采集入口改造与 ProofPlan 预检
Validates AC-1.1 acceptance criteria:
- NE-07: LoopPipeline rejects missing or empty ProofPlan when required.
- Real configuration digest computation (not synthetic string hash).
- No fake implementation_mutation injection during canary checks.
- Removal of hardcoded bypasses for hello_world and uart_echo in legacy triage.
- Unified Pilot B identity.
"""
from __future__ import annotations

import json
import hashlib
from pathlib import Path
import pytest

from loop.pipeline.pipeline import LoopPipeline
from tools.verify_afg_engine import PilotVerifier


def write_json(path: Path, data: dict) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")
    return path


def test_pipeline_computes_real_config_digest(tmp_path: Path):
    """Verifies that configuration digest reflects real file bytes, not string hash."""
    app_dir = tmp_path / "test_app"
    app_dir.mkdir(parents=True)
    
    wink_app_file = app_dir / "wink-app.json"
    wink_app_file.write_text(json.dumps({"app_name": "test_app_real_digest", "version": "1.0.0"}), encoding="utf-8")

    digest = LoopPipeline.input_hash(app_dir)
    assert isinstance(digest, str)
    assert len(digest) == 64
    
    # Modifying file must change the digest
    wink_app_file.write_text(json.dumps({"app_name": "test_app_real_digest", "version": "2.0.0"}), encoding="utf-8")
    new_digest = LoopPipeline.input_hash(app_dir)
    assert new_digest != digest


def test_pipeline_rejects_missing_proofplan_when_required(tmp_path: Path):
    """NE-07: require_proofplan=True rejects configuration with no proofplan."""
    vendor = tmp_path / "wink-micro-app/vendor/esp_idfv61"
    app = vendor / "peripherals/test_app"
    write_json(app / "wink-app.json", {"app_name": "test_app"})
    source = write_json(app / "unisim-scenarios/base.scenario.json", {
        "name": "base", "steps": [{"type": "EXECUTE"}]
    })
    execution = {
        "config_id": "wasm_sim_standard",
        "backend": "wasm_browser",
        "target_soc": "esp32",
        "profile": "standard",
        "acceptance": {"type": "wasm_simulation", "scenario_path": source.relative_to(vendor).as_posix()},
    }
    # No proofplan declared in entry
    entry = {"id": "esp.test", "target_app_dir": "peripherals/test_app", "executions": [execution]}
    write_json(vendor / ".governance/data/checklist.data.json", {"entries": [entry]})

    pipeline = LoopPipeline(workspace_root=tmp_path, require_proofplan=True)
    result = pipeline.execute_app(entry)

    assert not result.success
    assert result.stage == "PROOFPLAN"
    assert "MISSING_PROOFPLAN_OR_CLAIMS" in result.message


def test_pipeline_rejects_empty_claims_proofplan(tmp_path: Path):
    """NE-07: require_proofplan=True rejects proofplan with empty claims."""
    vendor = tmp_path / "wink-micro-app/vendor/esp_idfv61"
    app = vendor / "peripherals/test_app"
    write_json(app / "wink-app.json", {"app_name": "test_app"})
    source = write_json(app / "unisim-scenarios/base.scenario.json", {
        "name": "base", "steps": [{"type": "EXECUTE"}]
    })
    execution = {
        "config_id": "wasm_sim_standard",
        "backend": "wasm_browser",
        "target_soc": "esp32",
        "profile": "standard",
        "acceptance": {"type": "wasm_simulation", "scenario_path": source.relative_to(vendor).as_posix()},
    }
    # Proofplan with empty claims
    entry = {
        "id": "esp.test",
        "target_app_dir": "peripherals/test_app",
        "executions": [execution],
        "proofplan": {
            "app_id": "esp.test",
            "claims": [],
        }
    }
    write_json(vendor / ".governance/data/checklist.data.json", {"entries": [entry]})

    pipeline = LoopPipeline(workspace_root=tmp_path, require_proofplan=True)
    result = pipeline.execute_app(entry)

    assert not result.success
    assert result.stage == "PROOFPLAN"
    assert "MISSING_PROOFPLAN_OR_CLAIMS" in result.message


def test_pipeline_no_fake_implementation_mutation_source_check():
    """Verifies that pipeline.py contains no synthetic 'implementation_mutation' injection."""
    import inspect
    from loop.pipeline import pipeline

    src = inspect.getsource(pipeline)
    # The string 'evidence_class": "implementation_mutation"' must not appear as hardcoded injection
    assert 'evidence_records["claim.canary"] = [{"evidence_class": "implementation_mutation"' not in src
    assert '"evidence_class": "implementation_mutation"' not in src


def test_pilot_verifier_no_hardcoded_legacy_triage_bypass():
    """Confirms legacy 46 triage no longer has hardcoded bypass for hello_world or uart_echo."""
    import inspect
    from tools.verify_afg_engine import PilotVerifier

    source = inspect.getsource(PilotVerifier.triage_legacy_items)
    
    # Must NOT contain hardcoded eid bypass
    assert 'eid in ("esp.get_started.hello_world"' not in source
    assert 'eid in ("esp.peripherals.uart.uart_echo"' not in source
    assert 'if tok:' in source


def test_pilot_b_canonical_identity():
    """Confirms Pilot B canonical app identity is esp.peripherals.uart.uart_echo."""
    verifier = PilotVerifier(Path("."))
    # In algo-exercise, Pilot B app_id must be canonical
    ok_b, receipt_b = verifier.run_pilot_b_uart_echo(physical=False)
    assert receipt_b.app_id == "esp.peripherals.uart.uart_echo"
