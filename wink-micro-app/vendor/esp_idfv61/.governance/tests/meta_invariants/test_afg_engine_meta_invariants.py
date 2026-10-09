# SPDX-License-Identifier: Apache-2.0
"""
test_afg_engine_meta_invariants.py - Dual Closed-Loop Meta Invariant Verification Suite
========================================================================================
Implements Task 4.4:
- Negative Invariant Test Suite: META-01 ~ META-26 (100% Rejection / Incomplete Verification)
- Golden Positive Test Suite: META-POS-01 ~ META-POS-05 (100% Eligible Verification)
"""
from __future__ import annotations

import copy
import hashlib
import json
import sys
from pathlib import Path
import pytest

LOOP_DIR = Path(__file__).resolve().parents[2] / "tools" / "loop"
if str(LOOP_DIR) not in sys.path:
    sys.path.insert(0, str(LOOP_DIR))

from afg_engine import AFGEngine, AFGReceipt, ExecutionIdentity, EXPECTED_PROBE_ABI_VERSION, EXPECTED_PROBE_SIZE_BYTES
from archetype_resolver import ArchetypeResolver, ArchetypeResolutionError
from build_sandbox import BuildSandbox, compute_build_cache_key
from mutation_runner import MutationRunner, MutationBudgetExceededError


def _base_valid_identity():
    return {
        "app_id": "test_app",
        "config_id": "default",
        "target_soc": "esp32",
        "backend": "wasm_simulation",
        "sdkconfig_digest": "sha256-sdkconfig-1234",
        "toolchain_version": "emscripten-3.1.56",
        "probe_abi_version": EXPECTED_PROBE_ABI_VERSION,
        "probe_size_bytes": EXPECTED_PROBE_SIZE_BYTES,
    }


def _base_valid_package():
    return {
        "execution_identity": _base_valid_identity(),
        "claims": [
            {"id": "claim.core.boot", "evidence_class": "baseline"}
        ],
        "evidence_records": {
            "claim.core.boot": [
                {
                    "evidence_class": "baseline",
                    "status": "PASS",
                    "has_business_assertion": True,
                    "assertions": [{"type": "status_code", "expected": 0, "actual": 0}],
                },
                {
                    "evidence_class": "implementation_mutation",
                    "status": "MUTANT_KILLED",
                    "has_business_assertion": True,
                    "assertions": [{"type": "status_code", "expected": 0, "actual": -1}],
                }
            ]
        }
    }


# ============================================================================
# NEGATIVE TEST SUITE: META-01 ~ META-26 (100% REJECTION / INCOMPLETE)
# ============================================================================

def test_meta_01_empty_assertion_reports():
    """META-01: Empty assertion reports or no claims -> INCOMPLETE."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["claims"] = []
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "INCOMPLETE"


def test_meta_02_log_only_assertion():
    """META-02: Only log/console regex assertion without domain state -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"][0]["assertions"] = [
        {"type": "log_only", "stdout_match": "Hello world"}
    ]
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("LOG_ONLY_ASSERTION" in r for r in receipt.rejection_reasons)


def test_meta_03_mutant_not_activated():
    """META-03: Mutant injected but branch not reached -> REJECTED (MUTATION_NOT_ACTIVATED)."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"][1]["status"] = "MUTATION_NOT_ACTIVATED"
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("MUTATION_NOT_ACTIVATED" in r for r in receipt.rejection_reasons)


def test_meta_04_mutant_survived():
    """META-04: Mutant survived without equivalent witness -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"][1]["status"] = "MUTANT_SURVIVED"
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("MUTANT_SURVIVED" in r for r in receipt.rejection_reasons)


def test_meta_05_unhandled_fault():
    """META-05: Fault injection resulted in unhandled failure -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"][1] = {
        "evidence_class": "fault_injection",
        "status": "FAULT_UNHANDLED_FAIL",
        "has_business_assertion": True,
    }
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("FAULT_UNHANDLED_FAIL" in r for r in receipt.rejection_reasons)


def test_meta_06_zero_virtual_clock_progression():
    """META-06: Claims timing progression but advanced 0ms virtual time -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"][0]["claims_timing_progression"] = True
    pkg["evidence_records"]["claim.core.boot"][0]["virtual_time_advanced_ms"] = 0
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("ZERO_TIME_PROGRESSION" in r for r in receipt.rejection_reasons)


def test_meta_07_error_domain_mismatch():
    """META-07: Error code domain mismatch (e.g. positive esp_err matched to negative) -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"][0]["error_domain_valid"] = False
    pkg["evidence_records"]["claim.core.boot"][0]["error_domain_error"] = "ERROR_DOMAIN_MISMATCH: esp_err got negative"
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("ERROR_DOMAIN_MISMATCH" in r for r in receipt.rejection_reasons)


def test_meta_08_vague_error_matcher():
    """META-08: Vague error matcher rejection -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"][0]["error_domain_valid"] = False
    pkg["evidence_records"]["claim.core.boot"][0]["error_domain_error"] = "VAGUE_MATCHER_REJECTED: wildcards disallowed"
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("VAGUE_MATCHER_REJECTED" in r for r in receipt.rejection_reasons)


def test_meta_09_bypass_causality_violation():
    """META-09: Echo loopback bypassed firmware buffers -> REJECTED (CAUSALITY_VIOLATION)."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"].append({
        "evidence_class": "causality_loop",
        "firmware_traversed": False,
        "is_short_circuit_fixture": True,
    })
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("CAUSALITY_VIOLATION" in r for r in receipt.rejection_reasons)


def test_meta_10_probe_abi_version_mismatch():
    """META-10: Probe ABI version mismatch (not 0x0101) -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["execution_identity"]["probe_abi_version"] = 0x0100
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("ERR_PROBE_ABI_MISMATCH" in r for r in receipt.rejection_reasons)


def test_meta_11_probe_size_mismatch():
    """META-11: Probe struct size mismatch (not 64 bytes) -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["execution_identity"]["probe_size_bytes"] = 72
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("ERR_PROBE_ABI_MISMATCH" in r for r in receipt.rejection_reasons)


def test_meta_12_tampered_artifact_hash():
    """META-12: Tampered artifact hash -> REJECTED (ARTIFACT_HASH_MISMATCH)."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["artifact_sha256"] = "sha256-original-hash"
    receipt = engine.evaluate(pkg, artifact_bytes=b"tampered_binary_payload")
    assert receipt.overall_verdict == "REJECTED"
    assert any("ARTIFACT_HASH_MISMATCH" in r for r in receipt.rejection_reasons)


def test_meta_13_missing_baseline_evidence():
    """META-13: Missing baseline evidence for claim -> INCOMPLETE."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    # Remove baseline record
    pkg["evidence_records"]["claim.core.boot"] = [
        r for r in pkg["evidence_records"]["claim.core.boot"] if r.get("evidence_class") != "baseline"
    ]
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "INCOMPLETE"


def test_meta_14_missing_mutation_evidence():
    """META-14: Missing negative/mutation evidence for claim -> INCOMPLETE."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    # Remove mutation record
    pkg["evidence_records"]["claim.core.boot"] = [
        r for r in pkg["evidence_records"]["claim.core.boot"] if r.get("evidence_class") == "baseline"
    ]
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "INCOMPLETE"


def test_meta_15_archetype_missing_claim_diff():
    """META-15: Archetype inherits without archetype_claim_diff -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    proofplan = {"inherits": "archetype_start"}
    receipt = engine.evaluate(pkg, proofplan=proofplan)
    assert receipt.overall_verdict == "REJECTED"
    assert any("ERR_ARCHETYPE_CLAIM_DIFF_MISSING" in r for r in receipt.rejection_reasons)


def test_meta_16_archetype_empty_claim_diff_intersection():
    """META-16: Archetype inherits with empty claim intersection -> ArchetypeResolver raises ERR_ARCHETYPE_CLAIM_EMPTY_DIFF."""
    resolver = ArchetypeResolver()
    proofplan = {
        "inherits": "archetype_start",
        "archetype_claim_diff": ["totally.unrelated.claim.foo"],
        "claims": [{"id": "totally.unrelated.claim.foo"}],
    }
    with pytest.raises(ArchetypeResolutionError) as exc_info:
        resolver.resolve(proofplan)
    assert "ERR_ARCHETYPE_CLAIM_EMPTY_DIFF" in str(exc_info.value)


def test_meta_17_l2_compiler_mutation_budget_exceeded():
    """META-17: L2 compiler mutation budget exceeded (> 1 per claim) -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["l2_counts_per_claim"] = {"claim.core.boot": 2}
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("ERR_L2_BUDGET_EXCEEDED" in r for r in receipt.rejection_reasons)


def test_meta_18_backpressure_overrun_not_observed():
    """META-18: Backpressure test paused 100ms but overrun/drop never reported -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"].append({
        "evidence_class": "backpressure",
        "overrun_detected": False,
        "dropped_samples_count": 0,
    })
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("BACKPRESSURE_VIOLATION" in r for r in receipt.rejection_reasons)


def test_meta_19_cold_restart_dirty_state_leakage():
    """META-19: Cold restart fails to clear SRAM dirty state -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"].append({
        "evidence_class": "recovery",
        "dirty_state_cleared": False,
    })
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("RECOVERY_INVARIANT_VIOLATION" in r for r in receipt.rejection_reasons)


def test_meta_20_invalid_execution_identity():
    """META-20: Missing ExecutionIdentity required fields -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["execution_identity"]["target_soc"] = ""
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("INVALID_EXECUTION_IDENTITY" in r for r in receipt.rejection_reasons)


def test_meta_21_contradictory_sdkconfig_target():
    """META-21: Contradictory target_soc and sdkconfig content -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["execution_identity"]["target_soc"] = "esp32"
    pkg["execution_identity"]["sdkconfig_content"] = "CONFIG_IDF_TARGET_ESP32P4=y\n"
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("IDENTITY_MISMATCH" in r for r in receipt.rejection_reasons)


def test_meta_22_invalid_na_protocol():
    """META-22: allow_na_physical_fault without valid na_rule_id -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["applicability_protocol"] = {"allow_na_physical_fault": True, "na_rule_id": ""}
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("INVALID_NA_PROTOCOL" in r for r in receipt.rejection_reasons)


def test_meta_23_mutant_build_failure_not_kill():
    """META-23: Mutant killed by compiler failure instead of runtime assertion -> REJECTED."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["evidence_records"]["claim.core.boot"][1]["status"] = "MUTATION_BUILD_FAILED"
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("MUTATION_BUILD_FAILED" in r for r in receipt.rejection_reasons)


def test_meta_24_receipt_digest_forgery():
    """META-24: Receipt digest tampered with -> verification detects alteration."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "ELIGIBLE"
    original_digest = receipt.receipt_digest

    # Tamper with verdict
    receipt.overall_verdict = "REJECTED"
    data_to_hash = {
        "schema_version": receipt.schema_version,
        "app_id": receipt.app_id,
        "config_id": receipt.config_id,
        "overall_verdict": receipt.overall_verdict,
        "execution_identity": receipt.execution_identity,
        "claims_evaluation": receipt.claims_evaluation,
        "axioms_evaluation": receipt.axioms_evaluation,
        "rejection_reasons": sorted(receipt.rejection_reasons),
    }
    recomputed = hashlib.sha256(json.dumps(data_to_hash, sort_keys=True, ensure_ascii=False).encode("utf-8")).hexdigest()
    assert original_digest != recomputed


def test_meta_25_soc_identity_spoofing():
    """META-25: ESP32-P4 exclusive capability marked as esp32 -> IDENTITY_MISMATCH."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["capabilities"] = ["cap.bus.i3c_master"]
    pkg["execution_identity"]["target_soc"] = "esp32"
    pkg["execution_identity"]["soc_support_verified_by_kconfig"] = False
    pkg["execution_identity"]["simulated_soc_or_model"] = None
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "REJECTED"
    assert any("ERR_SOC_SPOOFING" in r or "IDENTITY_MISMATCH" in r for r in receipt.rejection_reasons)


def test_meta_26_toolchain_hash_cache_invalidation():
    """META-26: Altering toolchain_version invalidates build cache key."""
    key1 = compute_build_cache_key(
        app_source_digest="sha256-src-1",
        header_closure_digest="sha256-hdr-1",
        effective_sdkconfig_digest="sha256-cfg-1",
        toolchain_version="emscripten-3.1.56",
        facade_git_sha="git-sha-1",
        patch_content_digest="sha256-patch-0",
        config_profile_id="default",
    )
    key2 = compute_build_cache_key(
        app_source_digest="sha256-src-1",
        header_closure_digest="sha256-hdr-1",
        effective_sdkconfig_digest="sha256-cfg-1",
        toolchain_version="emscripten-3.1.57",  # Changed
        facade_git_sha="git-sha-1",
        patch_content_digest="sha256-patch-0",
        config_profile_id="default",
    )
    assert key1 != key2, "Cache key must change when toolchain changes"


# ============================================================================
# GOLDEN POSITIVE TEST SUITE: META-POS-01 ~ META-POS-05 (100% ELIGIBLE)
# ============================================================================

def test_meta_pos_01_baseline_and_mutation():
    """META-POS-01: Baseline PASS + mutant killed -> ELIGIBLE."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "ELIGIBLE"
    assert len(receipt.rejection_reasons) == 0
    assert len(receipt.receipt_digest) == 64


def test_meta_pos_02_bidirectional_causality():
    """META-POS-02: Compliant UART Echo PASS + disabled interrupt killed -> ELIGIBLE."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["claims"] = [{"id": "claim.uart.echo", "evidence_class": "baseline"}]
    pkg["evidence_records"] = {
        "claim.uart.echo": [
            {
                "evidence_class": "baseline",
                "status": "PASS",
                "has_business_assertion": True,
                "assertions": [{"type": "uart_echo", "expected": "hello", "actual": "hello"}],
            },
            {
                "evidence_class": "causality_loop",
                "firmware_traversed": True,
                "is_short_circuit_fixture": False,
                "status": "PASS",
                "has_business_assertion": True,
            },
            {
                "evidence_class": "implementation_mutation",
                "status": "MUTANT_KILLED",
                "has_business_assertion": True,
            }
        ]
    }
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "ELIGIBLE"


def test_meta_pos_03_fault_handled_pass():
    """META-POS-03: Network disconnect injected + app performs backoff timeout -> FAULT_HANDLED_PASS & ELIGIBLE."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["claims"] = [{"id": "claim.net.reconnect", "evidence_class": "baseline"}]
    pkg["evidence_records"] = {
        "claim.net.reconnect": [
            {
                "evidence_class": "baseline",
                "status": "PASS",
                "has_business_assertion": True,
                "assertions": [{"type": "net_status", "expected": "CONNECTED", "actual": "CONNECTED"}],
            },
            {
                "evidence_class": "fault_injection",
                "status": "FAULT_HANDLED_PASS",
                "has_business_assertion": True,
                "assertions": [{"type": "retry_count", "expected": 3, "actual": 3}],
            }
        ]
    }
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "ELIGIBLE"


def test_meta_pos_04_lifecycle_reset():
    """META-POS-04: NVS cold reboot readback + SRAM zeroed -> ELIGIBLE."""
    engine = AFGEngine()
    pkg = _base_valid_package()
    pkg["claims"] = [{"id": "claim.nvs.lifecycle", "evidence_class": "baseline"}]
    pkg["evidence_records"] = {
        "claim.nvs.lifecycle": [
            {
                "evidence_class": "baseline",
                "status": "PASS",
                "has_business_assertion": True,
                "assertions": [{"type": "nvs_val", "expected": 42, "actual": 42}],
            },
            {
                "evidence_class": "recovery",
                "dirty_state_cleared": True,
                "status": "PASS",
                "has_business_assertion": True,
            },
            {
                "evidence_class": "implementation_mutation",
                "status": "MUTANT_KILLED",
                "has_business_assertion": True,
            }
        ]
    }
    receipt = engine.evaluate(pkg)
    assert receipt.overall_verdict == "ELIGIBLE"


def test_meta_pos_05_promotion_service_cas(tmp_path):
    """META-POS-05: Multi-config valid receipt -> PromotionService commits atomic CAS update to verified_v1_1."""
    from promotion_service import PromotionService

    # Set up mock repo layout
    ws = tmp_path
    gov_dir = ws / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance"
    data_dir = gov_dir / "data"
    data_dir.mkdir(parents=True, exist_ok=True)

    manifest_file = data_dir / "checklist.data.json"
    manifest_data = {
        "entries": [
            {
                "id": "get-started/hello_world",
                "executions": [
                    {
                        "config_id": "default",
                        "delivery_state": "planned",
                        "backend": "wasm_simulation",
                    }
                ]
            }
        ]
    }
    manifest_file.write_text(json.dumps(manifest_data, indent=2), encoding="utf-8")

    # Set up candidate package
    cand_dir = tmp_path / "candidate_pkg"
    cand_dir.mkdir()
    (cand_dir / "candidate_evidence.json").write_text(json.dumps({
        "status": "candidate_ready",
        "app_id": "get-started/hello_world",
        "config_id": "default",
        "target_app_dir": "get-started/hello_world",
        "run_id": "20261009T120000Z-test",
    }), encoding="utf-8")

    pkg_hash = "sha256-mock-pkg"
    (cand_dir / "package_summary.json").write_text(json.dumps({
        "package_sha256": pkg_hash,
    }), encoding="utf-8")

    (cand_dir / "audit-decision.json").write_text(json.dumps({
        "verdict": "ACCEPT",
        "candidate_package_sha256": pkg_hash,
        "auditor": "AFG-Inspector-v1.1",
    }), encoding="utf-8")

    (cand_dir / "afg_evidence_receipt_v1_1.json").write_text(json.dumps({
        "overall_verdict": "ELIGIBLE",
        "schema_version": "1.1",
    }), encoding="utf-8")

    service = PromotionService(ws)
    ok, msg, receipt = service.promote_candidate(cand_dir, target_delivery_state="verified_v1_1")
    assert ok, f"Promotion failed: {msg}"
    assert receipt["app_id"] == "get-started/hello_world"

    # Verify committed delivery_state is verified_v1_1
    committed = json.loads(manifest_file.read_text(encoding="utf-8"))
    exec_entry = committed["entries"][0]["executions"][0]
    assert exec_entry["delivery_state"] == "verified_v1_1"
