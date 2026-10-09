# SPDX-License-Identifier: Apache-2.0
"""
afg_engine.py - Anti-False-Green Verification Decision Kernel v1.1
==================================================================
Implements Task 4.4 and the AFG v1.1 Tech Contract:
- Strict 3-state output: ELIGIBLE, REJECTED, INCOMPLETE
- Decoupled from SSOT CAS promotion (PromotionService handles commit)
- Validates ExecutionIdentity, Probe ABI (0x0101 / 64 bytes), Discrete Clock,
  Axiom 1 (Kill polarity / Fault handled pass), Axiom 2 (Zero loopback firmware dependency),
  Axiom 3 (Oracle trust / Reject log-only assertions), Error domain matching (ADR-0001),
  SoC identity verification (Anti-spoofing), and Receipt digest generation.
"""
from __future__ import annotations

import copy
import datetime
import hashlib
import json
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

# Expected Probe ABI values per esp_sim_probe.h
EXPECTED_PROBE_ABI_VERSION = 0x0101
EXPECTED_PROBE_SIZE_BYTES = 64

# Known hardware-exclusive peripherals requiring P4/C6/H2
EXCLUSIVE_SOC_CAPABILITIES = {
    "cap.bus.i3c_master": {"esp32p4"},
    "cap.dma.async_crc": {"esp32p4"},
    "cap.dma.async_color_convert": {"esp32p4"},
    "cap.coproc.lp_core": {"esp32c6", "esp32p4"},
    "cap.net.bridge_vlan": {"esp32p4"},
}


@dataclass
class ExecutionIdentity:
    app_id: str
    config_id: str
    target_soc: str
    backend: str
    sdkconfig_digest: str
    toolchain_version: str
    probe_abi_version: Optional[int] = None
    probe_size_bytes: Optional[int] = None
    simulated_soc_or_model: Optional[str] = None
    soc_support_verified_by_kconfig: Optional[bool] = False

    def validate(self) -> Tuple[bool, str]:
        if not self.app_id or not self.config_id:
            return False, "INVALID_EXECUTION_IDENTITY: app_id and config_id are required"
        if not self.target_soc:
            return False, "INVALID_EXECUTION_IDENTITY: target_soc is required"
        if not self.backend:
            return False, "INVALID_EXECUTION_IDENTITY: backend is required"
        if not self.sdkconfig_digest:
            return False, "INVALID_EXECUTION_IDENTITY: sdkconfig_digest is required"
        if not self.toolchain_version:
            return False, "INVALID_EXECUTION_IDENTITY: toolchain_version is required"
        return True, ""


@dataclass
class AFGReceipt:
    schema_version: str = "1.1"
    app_id: str = ""
    config_id: str = ""
    overall_verdict: str = "INCOMPLETE"  # ELIGIBLE, REJECTED, INCOMPLETE
    execution_identity: Dict[str, Any] = field(default_factory=dict)
    claims_evaluation: List[Dict[str, Any]] = field(default_factory=list)
    axioms_evaluation: Dict[str, Any] = field(default_factory=dict)
    rejection_reasons: List[str] = field(default_factory=list)
    timestamp_utc: str = ""
    receipt_digest: str = ""

    def sign(self) -> None:
        """Computes deterministic SHA-256 digest of receipt contents."""
        self.timestamp_utc = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        data_to_hash = {
            "schema_version": self.schema_version,
            "app_id": self.app_id,
            "config_id": self.config_id,
            "overall_verdict": self.overall_verdict,
            "execution_identity": self.execution_identity,
            "claims_evaluation": self.claims_evaluation,
            "axioms_evaluation": self.axioms_evaluation,
            "rejection_reasons": sorted(self.rejection_reasons),
        }
        raw = json.dumps(data_to_hash, sort_keys=True, ensure_ascii=False).encode("utf-8")
        self.receipt_digest = hashlib.sha256(raw).hexdigest()

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


class AFGEngine:
    """Anti-False-Green Verification Decision Kernel."""

    def __init__(self, capability_catalog: Optional[Dict[str, Any]] = None):
        self.catalog = capability_catalog or {}

    def evaluate(
        self,
        evidence_package: Dict[str, Any],
        proofplan: Optional[Dict[str, Any]] = None,
        artifact_bytes: Optional[bytes] = None,
    ) -> AFGReceipt:
        """Evaluates an evidence package against the AFG v1.1 contract.
        
        Returns an AFGReceipt with overall_verdict in {'ELIGIBLE', 'REJECTED', 'INCOMPLETE'}.
        """
        rejection_reasons: List[str] = []
        axioms_eval: Dict[str, Any] = {}
        claims_eval: List[Dict[str, Any]] = []

        # 1. Parse & validate ExecutionIdentity
        identity_data = evidence_package.get("execution_identity", {})
        if not isinstance(identity_data, dict) or not identity_data:
            return AFGReceipt(
                app_id=evidence_package.get("app_id", "unknown"),
                config_id=evidence_package.get("config_id", "unknown"),
                overall_verdict="REJECTED",
                rejection_reasons=["INVALID_EXECUTION_IDENTITY: Missing or non-dict execution_identity"],
            )

        identity = ExecutionIdentity(
            app_id=identity_data.get("app_id", ""),
            config_id=identity_data.get("config_id", ""),
            target_soc=identity_data.get("target_soc", ""),
            backend=identity_data.get("backend", ""),
            sdkconfig_digest=identity_data.get("sdkconfig_digest", ""),
            toolchain_version=identity_data.get("toolchain_version", ""),
            probe_abi_version=identity_data.get("probe_abi_version"),
            probe_size_bytes=identity_data.get("probe_size_bytes"),
            simulated_soc_or_model=identity_data.get("simulated_soc_or_model"),
            soc_support_verified_by_kconfig=identity_data.get("soc_support_verified_by_kconfig", False),
        )

        valid_id, id_msg = identity.validate()
        if not valid_id:
            return AFGReceipt(
                app_id=identity.app_id,
                config_id=identity.config_id,
                overall_verdict="REJECTED",
                rejection_reasons=[id_msg],
            )

        # 2. Check SoC Identity & Spoofing Defense (META-25 / AFG-R02)
        claimed_capabilities: Set[str] = set(evidence_package.get("capabilities", []))
        if proofplan:
            claimed_capabilities.update(proofplan.get("applicable_capabilities", []))

        for cap in claimed_capabilities:
            if cap in EXCLUSIVE_SOC_CAPABILITIES:
                allowed_socs = EXCLUSIVE_SOC_CAPABILITIES[cap]
                if identity.target_soc not in allowed_socs:
                    # Unless legitimately verified by Kconfig or explicitly declared generic_behavioral
                    if not identity.soc_support_verified_by_kconfig and identity.simulated_soc_or_model != "generic_behavioral":
                        rejection_reasons.append(
                            f"IDENTITY_MISMATCH: Capability '{cap}' requires SoC in {allowed_socs}, but target_soc is '{identity.target_soc}' (ERR_SOC_SPOOFING)"
                        )

        # Contradiction check: target_soc esp32 but sdkconfig contains exclusive P4 symbols without bridge
        if identity.target_soc == "esp32" and "CONFIG_IDF_TARGET_ESP32P4=y" in identity_data.get("sdkconfig_content", ""):
            rejection_reasons.append("IDENTITY_MISMATCH: Conflicting sdkconfig target in execution identity")

        # 3. Check Probe ABI Version & Struct Size (Task 2.1 / AFG-R11 / META-10, META-11)
        probes = evidence_package.get("probes")
        if probes is not None or identity.probe_abi_version is not None:
            abi_ver = identity.probe_abi_version or evidence_package.get("probe_abi_version")
            size_b = identity.probe_size_bytes or evidence_package.get("probe_size_bytes")
            if abi_ver != EXPECTED_PROBE_ABI_VERSION:
                rejection_reasons.append(
                    f"ERR_PROBE_ABI_MISMATCH: Probe ABI version 0x{abi_ver or 0:04x} != expected 0x{EXPECTED_PROBE_ABI_VERSION:04x}"
                )
            if size_b != EXPECTED_PROBE_SIZE_BYTES:
                rejection_reasons.append(
                    f"ERR_PROBE_ABI_MISMATCH: Probe struct size {size_b} bytes != expected {EXPECTED_PROBE_SIZE_BYTES} bytes"
                )

        # 4. Check Artifact Hash Integrity (META-12)
        recorded_artifact_sha = evidence_package.get("artifact_sha256")
        if recorded_artifact_sha and artifact_bytes is not None:
            actual_sha = hashlib.sha256(artifact_bytes).hexdigest()
            if actual_sha != recorded_artifact_sha:
                rejection_reasons.append(
                    f"ARTIFACT_HASH_MISMATCH: Recorded SHA {recorded_artifact_sha[:12]} != actual {actual_sha[:12]}"
                )

        # 5. Check ProofPlan inheritance compliance if proofplan provided (META-15, META-16)
        if proofplan:
            inherits = proofplan.get("inherits")
            if inherits:
                if "archetype_claim_diff" not in proofplan:
                    rejection_reasons.append("ERR_ARCHETYPE_CLAIM_DIFF_MISSING: Inheriting proofplan lacks archetype_claim_diff")

        # 6. Evaluate Claims & Evidence Classes
        claims = evidence_package.get("claims", [])
        if not claims and proofplan:
            claims = proofplan.get("claims", [])

        if not claims:
            # Empty assertion reports or no claims -> INCOMPLETE (META-01)
            return AFGReceipt(
                app_id=identity.app_id,
                config_id=identity.config_id,
                overall_verdict="INCOMPLETE",
                execution_identity=asdict(identity),
                rejection_reasons=["MISSING_CLAIMS: No claims provided in evidence package"],
            )

        has_incomplete = False
        evidence_records = evidence_package.get("evidence_records", {})

        for claim in claims:
            claim_id = claim.get("id", "unknown")
            claim_eval = {"claim_id": claim_id, "status": "UNKNOWN", "reasons": []}
            records = evidence_records.get(claim_id, [])

            if not records:
                # Check if entire claim has no evidence records
                claim_eval["status"] = "INCOMPLETE"
                claim_eval["reasons"].append("MISSING_EVIDENCE_RECORDS")
                claims_eval.append(claim_eval)
                has_incomplete = True
                continue

            # Check claim-level evidence matching
            claim_ev_class = claim.get("evidence_class")
            if claim_ev_class == "baseline":
                baseline_records = [r for r in records if r.get("evidence_class") == "baseline"]
                if not baseline_records and not claim.get("is_pure_fault_scenario"):
                    claim_eval["status"] = "INCOMPLETE"
                    claim_eval["reasons"].append("MISSING_BASELINE_EVIDENCE")
                    has_incomplete = True
            elif claim_ev_class in ("implementation_mutation", "driver_mutation", "fault_injection", "backpressure", "causality_loop", "recovery"):
                matching_records = [r for r in records if r.get("evidence_class") == claim_ev_class]
                if not matching_records:
                    claim_eval["status"] = "INCOMPLETE"
                    claim_eval["reasons"].append(f"MISSING_{claim_ev_class.upper()}_EVIDENCE")
                    has_incomplete = True
            else:
                # Unspecified claim: requires both baseline and mutation records
                baseline_records = [r for r in records if r.get("evidence_class") == "baseline"]
                if not baseline_records and not claim.get("is_pure_fault_scenario"):
                    claim_eval["status"] = "INCOMPLETE"
                    claim_eval["reasons"].append("MISSING_BASELINE_EVIDENCE")
                    has_incomplete = True

                mutation_records = [
                    r for r in records if r.get("evidence_class") in (
                        "implementation_mutation", "driver_mutation", "fault_injection", "backpressure", "causality_loop", "recovery"
                    )
                ]
                if not mutation_records and not evidence_package.get("applicability_protocol", {}).get("allow_na_physical_fault"):
                    claim_eval["status"] = "INCOMPLETE"
                    claim_eval["reasons"].append("MISSING_MUTATION_EVIDENCE")
                    has_incomplete = True

            # Evaluate each record within claim
            for r in records:
                # Check Axiom 3: Reject log-only assertions (META-02)
                assertions = r.get("assertions", [])
                if not assertions and not r.get("has_business_assertion", True):
                    rejection_reasons.append(f"LOG_ONLY_ASSERTION: Claim '{claim_id}' record has no business assertions")
                for ast in assertions:
                    if isinstance(ast, dict):
                        # If assertion only asserts on console text/log regex and nothing else
                        if ast.get("type") == "log_only" or (set(ast.keys()) <= {"type", "log_regex", "stdout_match"}):
                            rejection_reasons.append(f"LOG_ONLY_ASSERTION: Claim '{claim_id}' uses log-only assertion")

                # Check Axiom 1: Kill polarity & fault handling
                ev_class = r.get("evidence_class")
                status = r.get("status")

                if ev_class in ("implementation_mutation", "driver_mutation"):
                    if status == "MUTATION_NOT_ACTIVATED":
                        rejection_reasons.append(f"MUTATION_NOT_ACTIVATED: Mutant for '{claim_id}' was not reached (META-03)")
                    elif status == "MUTANT_SURVIVED":
                        rejection_reasons.append(f"MUTANT_SURVIVED: Mutant for '{claim_id}' survived without equivalent witness (META-04)")
                    elif status == "MUTATION_BUILD_FAILED":
                        rejection_reasons.append(f"MUTATION_BUILD_FAILED: Mutant for '{claim_id}' failed build rather than assertion kill (META-23)")
                    elif status != "MUTANT_KILLED":
                        rejection_reasons.append(f"MUTANT_KILL_FAILED: Status '{status}' for '{claim_id}' is not MUTANT_KILLED")

                elif ev_class == "fault_injection":
                    if status == "FAULT_UNHANDLED_FAIL":
                        rejection_reasons.append(f"FAULT_UNHANDLED_FAIL: Fault handling failed for '{claim_id}' (META-05)")
                    elif status not in ("FAULT_HANDLED_PASS", "PASS"):
                        rejection_reasons.append(f"FAULT_HANDLING_INVALID: Status '{status}' for '{claim_id}' is not PASS")

                elif ev_class == "causality_loop":
                    if r.get("firmware_traversed") is False or r.get("is_short_circuit_fixture") is True:
                        rejection_reasons.append(f"CAUSALITY_VIOLATION: Echo loopback bypassed firmware buffers for '{claim_id}' (META-09)")

                elif ev_class == "backpressure":
                    if r.get("overrun_detected") is False and r.get("dropped_samples_count", 0) == 0:
                        rejection_reasons.append(f"BACKPRESSURE_VIOLATION: Overrun or drop not observed under backpressure for '{claim_id}' (META-18)")

                elif ev_class == "recovery":
                    if r.get("dirty_state_cleared") is False:
                        rejection_reasons.append(f"RECOVERY_INVARIANT_VIOLATION: State not cleanly recovered for '{claim_id}' (META-19)")

                # Check Axiom 4: Virtual discrete clock progression (META-06)
                if r.get("claims_timing_progression", False):
                    virtual_time_advanced_ms = r.get("virtual_time_advanced_ms", 0)
                    if virtual_time_advanced_ms <= 0:
                        rejection_reasons.append(f"ZERO_TIME_PROGRESSION: Claim '{claim_id}' claims timing progression but advanced 0ms (META-06)")

                # Check Axiom 6: Error code domain (META-07, META-08)
                err_domain_check = r.get("error_domain_valid", True)
                if not err_domain_check or r.get("error_domain_error"):
                    err_msg = r.get("error_domain_error", "ERROR_DOMAIN_MISMATCH")
                    rejection_reasons.append(f"{err_msg} on '{claim_id}'")

            if not claim_eval["reasons"]:
                claim_eval["status"] = "PASSED"
            claims_eval.append(claim_eval)

        # 7. Check L2 Mutation Budget (META-17)
        l2_counts_per_claim: Dict[str, int] = evidence_package.get("l2_counts_per_claim", {})
        for cid, cnt in l2_counts_per_claim.items():
            if cnt > 1:
                rejection_reasons.append(f"ERR_L2_BUDGET_EXCEEDED: Claim '{cid}' used {cnt} L2 mutations > budget 1 (META-17)")

        # 8. Check Applicability Protocol (META-22)
        app_proto = evidence_package.get("applicability_protocol", {})
        if app_proto.get("allow_na_physical_fault"):
            if not app_proto.get("na_rule_id"):
                rejection_reasons.append("INVALID_NA_PROTOCOL: allow_na_physical_fault requires valid na_rule_id (META-22)")

        # Check Axiom 1 at package level: must have at least one baseline record and one negative/mutation/recovery record
        all_records = [r for recs in evidence_records.values() for r in recs]
        has_pkg_baseline = any(r.get("evidence_class") == "baseline" for r in all_records)
        has_pkg_mutation = any(
            r.get("evidence_class") in ("implementation_mutation", "driver_mutation", "fault_injection", "backpressure", "causality_loop", "recovery")
            for r in all_records
        )
        if not has_pkg_baseline:
            has_incomplete = True
        if not has_pkg_mutation and not app_proto.get("allow_na_physical_fault"):
            has_incomplete = True

        # 9. Determine Overall Verdict
        if rejection_reasons:
            overall_verdict = "REJECTED"
        elif has_incomplete:
            overall_verdict = "INCOMPLETE"
        else:
            overall_verdict = "ELIGIBLE"

        receipt = AFGReceipt(
            schema_version="1.1",
            app_id=identity.app_id,
            config_id=identity.config_id,
            overall_verdict=overall_verdict,
            execution_identity=asdict(identity),
            claims_evaluation=claims_eval,
            axioms_evaluation=axioms_eval,
            rejection_reasons=rejection_reasons,
        )
        receipt.sign()
        return receipt
