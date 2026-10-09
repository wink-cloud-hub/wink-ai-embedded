# SPDX-License-Identifier: Apache-2.0
"""
test_t1_7_cas_promotion.py - Unit Test Suite for T1.7 CAS Promotion, Idempotence & Lock Leasing
================================================================================================
Verifies:
1. Manifest CAS optimistic locking conflict rejection.
2. Idempotent promotion on repeated identical requests.
3. Transactional staging isolation, journal logging, and crash recovery.
4. FileLockLease fencing token, renewal, and living-process TTL protection.
5. Tampered candidate and identity-mismatched package rejection during promotion.
"""
import json
import os
import shutil
import time
from typing import Tuple
import pytest
from pathlib import Path

from loop.harness.lock_lease import FileLockLease, LockLeaseError
from loop.services.promotion_service import PromotionService, compute_file_sha256
from loop.afg.canonical_sealing import seal_candidate_payload


def _setup_test_workspace(tmp_path: Path) -> Tuple[Path, Path, Path]:
    ws = tmp_path
    gov = ws / "wink-micro-app" / "vendor" / "esp_idfv61" / ".governance"
    data_dir = gov / "data"
    data_dir.mkdir(parents=True, exist_ok=True)
    (gov / "reports").mkdir(parents=True, exist_ok=True)

    manifest_file = data_dir / "checklist.data.json"
    manifest_content = {
        "schema_version": "1.0",
        "entries": [
            {
                "id": "get-started/hello_world",
                "target_app_dir": "get-started/hello_world",
                "executions": [
                    {
                        "config_id": "default",
                        "backend": "wasm_simulation",
                        "delivery_state": "candidate_ready"
                    }
                ]
            }
        ]
    }
    manifest_file.write_text(json.dumps(manifest_content, indent=2), encoding="utf-8")
    return ws, gov, manifest_file


def _create_valid_candidate(cand_dir: Path, run_id: str = "run-20261009-01") -> Path:
    cand_dir.mkdir(parents=True, exist_ok=True)
    (cand_dir / "candidate_evidence.json").write_text(json.dumps({
        "status": "candidate_ready",
        "app_id": "get-started/hello_world",
        "config_id": "default",
        "target_app_dir": "get-started/hello_world",
        "run_id": run_id,
        "checks": [{"kind": "baseline", "accepted": True, "verdict": "pass"}]
    }, indent=2), encoding="utf-8")

    (cand_dir / "proofplan.json").write_text(json.dumps({
        "claims": [{"id": "CLAIM-01"}]
    }, indent=2), encoding="utf-8")

    (cand_dir / "afg_evidence_receipt_v1_1.json").write_text(json.dumps({
        "overall_verdict": "ELIGIBLE",
        "schema_version": "1.1"
    }, indent=2), encoding="utf-8")

    summary = seal_candidate_payload(cand_dir, run_id=run_id, app_id="get-started/hello_world", config_id="default")
    pkg_hash = summary["payload_sha256"]

    (cand_dir / "audit-decision.json").write_text(json.dumps({
        "verdict": "ACCEPT",
        "candidate_package_sha256": pkg_hash,
        "app_id": "get-started/hello_world",
        "config_id": "default",
        "auditor": {"identity": "independent_auditor"}
    }, indent=2), encoding="utf-8")

    return cand_dir


def test_cas_conflict_rejection(tmp_path: Path):
    """Verify that expected_manifest_sha256 mismatch halts promotion immediately."""
    ws, _, manifest_file = _setup_test_workspace(tmp_path)
    cand_dir = _create_valid_candidate(tmp_path / "cand")

    service = PromotionService(ws)
    ok, msg, receipt = service.promote_candidate(
        cand_dir,
        expected_manifest_sha256="0000000000000000000000000000000000000000000000000000000000000000"
    )

    assert not ok
    assert "[CAS_CONFLICT]" in msg
    assert receipt is None


def test_promotion_transaction_and_idempotence(tmp_path: Path):
    """Verify atomic promotion updates checklist, creates archive, logs journal, and handles duplicate promotion."""
    ws, gov, manifest_file = _setup_test_workspace(tmp_path)
    cand_dir = _create_valid_candidate(tmp_path / "cand", run_id="run-idemp-01")

    service = PromotionService(ws)
    current_manifest_sha = compute_file_sha256(manifest_file)

    # 1. First promotion succeeds
    ok, msg, receipt = service.promote_candidate(cand_dir, expected_manifest_sha256=current_manifest_sha)
    assert ok, f"Initial promotion failed: {msg}"
    assert receipt is not None
    assert receipt["idempotent"] is False
    assert receipt["app_id"] == "get-started/hello_world"

    # Verify committed manifest
    committed = json.loads(manifest_file.read_text(encoding="utf-8"))
    exec_item = committed["entries"][0]["executions"][0]
    assert exec_item["delivery_state"] == "verified_v1_1"
    assert exec_item["evidence"]["package_sha256"] == receipt["package_sha256"]

    # Verify journal log
    journal_path = gov / "data" / "promotion.journal.jsonl"
    assert journal_path.is_file()
    journal_lines = [json.loads(line) for line in journal_path.read_text(encoding="utf-8").strip().split("\n")]
    assert len(journal_lines) == 2
    assert journal_lines[0]["stage"] == "PENDING"
    assert journal_lines[1]["stage"] == "COMMITTED"

    # 2. Second identical promotion succeeds as idempotent without modifying state
    ok2, msg2, receipt2 = service.promote_candidate(cand_dir)
    assert ok2, f"Idempotent promotion failed: {msg2}"
    assert receipt2 is not None
    assert receipt2["idempotent"] is True
    assert "idempotent" in msg2.lower()


def test_tampered_candidate_blocked_at_promotion(tmp_path: Path):
    """Verify that tampering with a sealed candidate causes promotion failure."""
    ws, _, _ = _setup_test_workspace(tmp_path)
    cand_dir = _create_valid_candidate(tmp_path / "cand_tamper")

    # Modify candidate_evidence.json after sealing
    cand_evidence_path = cand_dir / "candidate_evidence.json"
    evidence_data = json.loads(cand_evidence_path.read_text(encoding="utf-8"))
    evidence_data["tampered_field"] = "malicious_change"
    cand_evidence_path.write_text(json.dumps(evidence_data), encoding="utf-8")

    service = PromotionService(ws)
    ok, msg, _ = service.promote_candidate(cand_dir)
    assert not ok
    assert "[PAYLOAD_TAMPERED]" in msg


def test_identity_mismatch_blocked_at_promotion(tmp_path: Path):
    """Verify that mismatched app_id or config_id between audit and candidate blocks promotion."""
    ws, _, _ = _setup_test_workspace(tmp_path)
    cand_dir = _create_valid_candidate(tmp_path / "cand_mismatch")

    # Alter app_id in audit-decision.json
    audit_file = cand_dir / "audit-decision.json"
    audit_data = json.loads(audit_file.read_text(encoding="utf-8"))
    audit_data["app_id"] = "peripherals/uart/uart_echo"  # Borrowed from different app
    audit_file.write_text(json.dumps(audit_data), encoding="utf-8")

    service = PromotionService(ws)
    ok, msg, _ = service.promote_candidate(cand_dir)
    assert not ok
    assert "[AUDIT_IDENTITY_MISMATCH]" in msg


def test_recover_pending_journals_cleans_orphaned_staging(tmp_path: Path):
    """Verify recover_pending_journals removes leftover .staging. directories."""
    ws, gov, _ = _setup_test_workspace(tmp_path)
    reports_dir = gov / "reports" / "get-started" / "hello_world"
    reports_dir.mkdir(parents=True, exist_ok=True)

    orphan1 = reports_dir / "package-orphan-01.staging.9999"
    orphan2 = reports_dir / "package-orphan-02.staging.8888"
    orphan1.mkdir()
    orphan2.mkdir()
    (orphan1 / "dummy.txt").write_text("dummy")

    # Also touch journal file
    (gov / "data" / "promotion.journal.jsonl").write_text('{"stage":"PENDING"}\n', encoding="utf-8")

    service = PromotionService(ws)
    cleaned = service.recover_pending_journals()
    assert cleaned == 2
    assert not orphan1.exists()
    assert not orphan2.exists()


def test_file_lock_lease_fencing_token_and_renewal(tmp_path: Path):
    """Verify FileLockLease token tracking, is_owner check, and renewal."""
    lock_file = tmp_path / "manifest.lock"

    lease = FileLockLease(lock_file, lease_ttl_seconds=5.0)
    assert lease.token is None
    assert not lease.is_owner()

    with lease:
        assert lease.token is not None
        assert lease.is_owner()
        # Verify token written to disk
        data = json.loads(lock_file.read_text(encoding="utf-8"))
        assert data["token"] == lease.token

        # Test renewal
        ok = lease.renew(extension_seconds=10.0)
        assert ok
        renewed_data = json.loads(lock_file.read_text(encoding="utf-8"))
        assert "renewed_at" in renewed_data

    # After exit, lock released and token cleared
    assert not lock_file.exists()
    assert not lease.is_owner()


def test_file_lock_lease_protects_living_process_on_same_host(tmp_path: Path):
    """Verify that an active process is NOT evicted merely due to TTL on the same host (Constraint 3.5)."""
    lock_file = tmp_path / "manifest.lock"

    # Simulate an existing lock held by our own PID (which is definitely ALIVE) but with expired timestamp
    stale_expired_data = {
        "token": "foreign-token-123",
        "pid": os.getpid(),  # Alive!
        "hostname": FileLockLease(lock_file).hostname,
        "created_at": time.time() - 100,
        "expires_at": time.time() - 50,  # Expired TTL!
    }
    lock_file.write_text(json.dumps(stale_expired_data), encoding="utf-8")

    # Another lease attempt must NOT break the lock of this living process
    challenger = FileLockLease(lock_file, lease_ttl_seconds=5.0)
    with pytest.raises(LockLeaseError):
        challenger.acquire(timeout_seconds=0.3, poll_interval_ms=50)

    # Lock file must still be intact
    assert lock_file.exists()
    content = json.loads(lock_file.read_text(encoding="utf-8"))
    assert content["token"] == "foreign-token-123"
