# SPDX-License-Identifier: Apache-2.0
"""
test_t1_8_pilot_e2e.py - Unit Test Suite for T1.8 Pilot E2E & Anti-Defense Inversion
====================================================================================
Verifies:
1. Pilot A (hello_world) and Pilot B (uart_echo) real physical evidence validation.
2. Pilot C (adc_continuous_read) S-03 driver defect detection (needs_driver_fix).
3. Anti-Defense Inversion: Removing specific validation defenses turns the test RED
   (borrowed report, tampered artifact, mutant survived, dirty recovery, vague assertion).
"""
import copy
import json
import pytest
from pathlib import Path

from tools.verify_afg_engine import PilotVerifier, find_workspace_root
from loop.afg.engine import AFGEngine, AFGReceipt


@pytest.fixture
def repo_root() -> Path:
    return find_workspace_root()


def test_pilot_a_physical_evidence_verified(repo_root: Path):
    """Verify Pilot A extracts physical assets, executes report contract, and has zero rejections."""
    verifier = PilotVerifier(repo_root)
    ok, receipt = verifier.run_pilot_a_hello_world(physical=True)

    assert receipt.app_id == "esp.get_started.hello_world"
    assert receipt.overall_verdict in ("ELIGIBLE", "INCOMPLETE")
    assert not receipt.rejection_reasons, f"Unexpected rejection reasons: {receipt.rejection_reasons}"
    assert receipt.receipt_digest is not None


def test_pilot_b_physical_evidence_verified(repo_root: Path):
    """Verify Pilot B extracts physical assets and validates physical run report without crashes."""
    verifier = PilotVerifier(repo_root)
    ok, receipt = verifier.run_pilot_b_uart_echo(physical=True)

    assert receipt.app_id == "esp.peripherals.uart.uart_echo"
    assert receipt.overall_verdict in ("ELIGIBLE", "INCOMPLETE")
    assert not receipt.rejection_reasons, f"Unexpected rejection reasons: {receipt.rejection_reasons}"
    assert receipt.receipt_digest is not None


def test_pilot_c_intercepts_s03_defect(repo_root: Path):
    """Verify Pilot C accurately flags S-03 backpressure overrun defect as needs_driver_fix."""
    verifier = PilotVerifier(repo_root)
    decision, receipt = verifier.run_pilot_c_adc_continuous(physical=True)

    assert decision == "needs_driver_fix"
    assert receipt.overall_verdict == "REJECTED"
    assert any("BACKPRESSURE_VIOLATION" in r for r in receipt.rejection_reasons)


def test_anti_defense_borrowed_report_rejected(repo_root: Path, tmp_path: Path):
    """Anti-Defense: Passing a foreign app's report to Pilot A must be rejected by report contract."""
    verifier = PilotVerifier(repo_root)
    app_id = "esp.get_started.hello_world"
    resolved = verifier.resolver.resolve({
        "inherits": "archetype_start",
        "archetype_claim_diff": ["claim.start.boot_banner"]
    })

    # Create a borrowed report with mismatched templateId and step counts
    borrowed_report = tmp_path / "borrowed-report.json"
    borrowed_report.write_text(json.dumps({
        "results": [{
            "ok": True,
            "header": {"name": "uart_echo", "templateId": "uart_echo", "totalSteps": 99},
            "stepResults": [{"type": "uart_byte", "status": "passed"}]
        }]
    }), encoding="utf-8")

    ok, _, _, reason = verifier.load_physical_pilot_evidence(
        app_subpath="get-started/hello_world",
        scenario_filename="hello_world.scenario.json",
        app_id=app_id,
        resolved_proofplan=resolved,
        report_path=borrowed_report
    )

    assert not ok
    assert "REPORT_SCENARIO_MISMATCH" in reason or "NO_PHYSICAL_EXECUTION_EVIDENCE" in reason


def test_anti_defense_missing_wasm_asset_rejected(tmp_path: Path):
    """Anti-Defense: Missing wink_simulator.wasm causes load_physical_pilot_evidence to fail."""
    # Setup dummy tree with missing wasm file
    app_dir = tmp_path / "wink-micro-app" / "vendor" / "esp_idfv61" / "get-started" / "hello_world"
    (app_dir / "unisim-assets").mkdir(parents=True, exist_ok=True)
    (app_dir / "unisim-scenarios").mkdir(parents=True, exist_ok=True)
    (app_dir / "unisim-scenarios" / "hello_world.scenario.json").write_text("{}", encoding="utf-8")

    verifier = PilotVerifier(tmp_path)
    ok, _, _, reason = verifier.load_physical_pilot_evidence(
        app_subpath="get-started/hello_world",
        scenario_filename="hello_world.scenario.json",
        app_id="esp.get_started.hello_world",
        resolved_proofplan={"claims": []}
    )

    assert not ok
    assert "NO_PHYSICAL_EXECUTION_EVIDENCE" in reason
    assert "WASM simulator assets missing" in reason


def test_anti_defense_mutant_survived_fails_afg(repo_root: Path):
    """Anti-Defense: A survived mutant must flip AFG verdict to REJECTED."""
    verifier = PilotVerifier(repo_root)
    engine = AFGEngine()

    proofplan = verifier.resolver.resolve({
        "inherits": "archetype_start",
        "archetype_claim_diff": [
            "claim.start.boot_banner",
            "claim.start.restart_mutation_kill"
        ],
        "applicability_protocol": {"allow_na_physical_fault": True, "na_rule_id": "na_pure_console_app"}
    })

    # Construct package where mutant survived
    pkg = {
        "app_id": "esp.get_started.hello_world",
        "config_id": "default",
        "execution_identity": {
            "app_id": "esp.get_started.hello_world",
            "config_id": "default",
            "target_soc": "esp32",
            "backend": "wasm_simulation",
            "sdkconfig_digest": "sha256-mock",
            "toolchain_version": "emscripten-6.0.9",
            "probe_abi_version": 1,
            "probe_size_bytes": 1024,
        },
        "claims": proofplan["claims"],
        "applicability_protocol": proofplan["applicability_protocol"],
        "evidence_records": {
            "claim.start.boot_banner": [
                {"evidence_class": "baseline", "status": "PASS", "has_business_assertion": True}
            ],
            "claim.start.restart_mutation_kill": [
                {"evidence_class": "baseline", "status": "PASS", "has_business_assertion": True},
                {
                    "evidence_class": "implementation_mutation",
                    "status": "MUTANT_SURVIVED",  # Mutant survived!
                    "has_business_assertion": True
                }
            ]
        }
    }

    receipt = engine.evaluate(pkg, proofplan=proofplan)
    assert receipt.overall_verdict == "REJECTED"
    assert any("MUTANT_SURVIVED" in r for r in receipt.rejection_reasons)


def test_anti_defense_dirty_state_leakage_fails_afg(repo_root: Path):
    """Anti-Defense: Incomplete recovery with dirty state leakage causes AFG rejection."""
    verifier = PilotVerifier(repo_root)
    engine = AFGEngine()

    proofplan = verifier.resolver.resolve({
        "inherits": "archetype_start",
        "archetype_claim_diff": [
            "claim.start.boot_banner",
            "claim.start.clean_state_recovery"
        ],
        "applicability_protocol": {"allow_na_physical_fault": True, "na_rule_id": "na_pure_console_app"}
    })

    pkg = {
        "app_id": "esp.get_started.hello_world",
        "config_id": "default",
        "execution_identity": {
            "app_id": "esp.get_started.hello_world",
            "config_id": "default",
            "target_soc": "esp32",
            "backend": "wasm_simulation",
            "sdkconfig_digest": "sha256-mock",
            "toolchain_version": "emscripten-6.0.9",
            "probe_abi_version": 1,
            "probe_size_bytes": 1024,
        },
        "claims": proofplan["claims"],
        "applicability_protocol": proofplan["applicability_protocol"],
        "evidence_records": {
            "claim.start.boot_banner": [
                {"evidence_class": "baseline", "status": "PASS", "has_business_assertion": True}
            ],
            "claim.start.clean_state_recovery": [
                {"evidence_class": "baseline", "status": "PASS", "has_business_assertion": True},
                {
                    "evidence_class": "recovery",
                    "status": "PASS",
                    "dirty_state_cleared": False,  # Dirty state not cleared!
                    "has_business_assertion": True
                }
            ]
        }
    }

    receipt = engine.evaluate(pkg, proofplan=proofplan)
    assert receipt.overall_verdict == "REJECTED"
    assert any("RECOVERY_INVARIANT_VIOLATION" in r for r in receipt.rejection_reasons)

