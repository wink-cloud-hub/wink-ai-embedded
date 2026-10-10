# SPDX-License-Identifier: Apache-2.0
"""
test_t1_6_envelope_sealing.py - Unit Test Suite for T1.6 Envelope Sealing & Audit Binding
==========================================================================================
Verifies:
1. RFC 8785 (JCS) JSON canonicalization and cross-platform LF line normalization.
2. Strict payload vs envelope separation (no self-referential cycles, envelope files excluded).
3. Payload tampering detection (modify, delete, add file after sealing).
4. Candidate audit preflight validation (fixes I-09 all([]) vulnerability, requires AFG receipt & ProofPlan).
5. Audit decision signing safeguards (disallows self-signing, requires explicit auditor, rejects ineligible candidates).
"""
import json
import pytest
from pathlib import Path

from loop.afg.canonical_sealing import (
    canonical_json_dumps,
    canonical_json_bytes,
    compute_normalized_file_sha256,
    is_envelope_file,
    compute_payload_manifest,
    seal_candidate_payload,
    verify_payload_seal,
    validate_candidate_for_audit,
    ENVELOPE_FILENAMES,
)
from tools.inspect_candidate import inspect_candidate, sign_audit_decision, compute_dir_digest


def test_jcs_canonical_json_serialization():
    """Verify RFC 8785 (JCS) deterministic sorting, delimiter compaction, and Unicode preservation."""
    obj1 = {"zebra": 1, "apple": 2, "nested": {"beta": 10, "alpha": 20}}
    obj2 = {"nested": {"alpha": 20, "beta": 10}, "apple": 2, "zebra": 1}

    dump1 = canonical_json_dumps(obj1)
    dump2 = canonical_json_dumps(obj2)

    assert dump1 == dump2
    # Ensure keys are sorted at root and nested level
    assert dump1 == '{"apple":2,"nested":{"alpha":20,"beta":10},"zebra":1}'
    # Ensure no whitespace around delimiters
    assert " " not in dump1
    assert "\n" not in dump1

    # UTF-8 preservation
    unicode_obj = {"message": "测试固件", "status": "ok"}
    dump_u = canonical_json_dumps(unicode_obj)
    assert "测试固件" in dump_u
    assert "\\u" not in dump_u


def test_cross_platform_lf_normalization(tmp_path: Path):
    """Verify that CRLF on Windows and LF on Unix produce the exact same SHA-256 digest."""
    crlf_file = tmp_path / "crlf_sample.json"
    lf_file = tmp_path / "lf_sample.json"

    content_text = '{\n  "name": "uart_echo",\n  "status": "pass"\n}\n'
    lf_file.write_bytes(content_text.encode("utf-8"))
    crlf_file.write_bytes(content_text.replace("\n", "\r\n").encode("utf-8"))

    sha_lf = compute_normalized_file_sha256(lf_file)
    sha_crlf = compute_normalized_file_sha256(crlf_file)

    assert sha_lf == sha_crlf, "CRLF and LF files must yield identical normalized digests"


def test_payload_envelope_separation_and_no_self_reference(tmp_path: Path):
    """Verify envelope files are excluded from payload manifest and do not alter payload hash."""
    cand_dir = tmp_path / "cand_run"
    cand_dir.mkdir()

    # Create immutable payload files
    (cand_dir / "candidate_evidence.json").write_text('{"status":"candidate_ready"}\n', encoding="utf-8")
    (cand_dir / "build-manifest.json").write_text('{"build_id":"bld-1"}\n', encoding="utf-8")
    (cand_dir / "run-manifest.json").write_text('{"run_id":"run-1"}\n', encoding="utf-8")

    # Seal payload
    summary = seal_candidate_payload(
        candidate_dir=cand_dir,
        run_id="run-1",
        app_id="uart_echo",
        config_id="standard"
    )
    initial_payload_sha = summary["payload_sha256"]

    # Ensure package_summary.json itself is excluded from payload manifest
    manifest_paths = [m["path"] for m in summary["payload_manifest"]]
    assert "package_summary.json" not in manifest_paths
    assert "candidate_evidence.json" in manifest_paths
    assert "build-manifest.json" in manifest_paths
    assert "run-manifest.json" in manifest_paths

    # Verify initial seal
    ok, msg, _ = verify_payload_seal(cand_dir)
    assert ok, f"Initial seal verification failed: {msg}"

    # Now add envelope files (audit decision and promotion receipt)
    (cand_dir / "audit-decision.json").write_text('{"verdict":"ACCEPT"}\n', encoding="utf-8")
    (cand_dir / "promotion_receipt.json").write_text('{"promoted":true}\n', encoding="utf-8")
    (cand_dir / "staging.tmp").write_text('temp\n', encoding="utf-8")

    # Recompute payload manifest
    recomputed_sha, recomputed_manifest = compute_payload_manifest(cand_dir)
    assert recomputed_sha == initial_payload_sha, "Adding envelope files must NOT alter payload hash"
    recomputed_paths = [m["path"] for m in recomputed_manifest]
    assert "audit-decision.json" not in recomputed_paths
    assert "promotion_receipt.json" not in recomputed_paths
    assert "staging.tmp" not in recomputed_paths

    # Seal must remain valid after envelope files are written
    ok_after, msg_after, _ = verify_payload_seal(cand_dir)
    assert ok_after, f"Seal verification must remain valid after envelope files added: {msg_after}"


def test_payload_tamper_detection_on_mutation(tmp_path: Path):
    """Verify that tampering with any payload file breaks the seal."""
    cand_dir = tmp_path / "cand_run"
    cand_dir.mkdir()

    raw_report = cand_dir / "raw_report.json"
    raw_report.write_text('{"result":"success"}\n', encoding="utf-8")
    (cand_dir / "candidate_evidence.json").write_text('{"status":"candidate_ready"}\n', encoding="utf-8")

    seal_candidate_payload(cand_dir, run_id="r1", app_id="test_app", config_id="cfg1")
    ok, _, _ = verify_payload_seal(cand_dir)
    assert ok

    # Tamper with raw_report.json
    raw_report.write_text('{"result":"tampered"}\n', encoding="utf-8")
    ok_tampered, msg, _ = verify_payload_seal(cand_dir)
    assert not ok_tampered
    assert "[PAYLOAD_TAMPERED]" in msg
    assert "raw_report.json" in msg


def test_payload_tamper_detection_on_addition_and_deletion(tmp_path: Path):
    """Verify that adding or deleting a payload file breaks the seal."""
    cand_dir = tmp_path / "cand_run"
    cand_dir.mkdir()

    f1 = cand_dir / "file1.txt"
    f2 = cand_dir / "file2.txt"
    f1.write_text("1\n", encoding="utf-8")
    f2.write_text("2\n", encoding="utf-8")

    seal_candidate_payload(cand_dir, run_id="r1", app_id="test_app", config_id="cfg1")

    # 1. Delete a payload file
    f2.unlink()
    ok_del, msg_del, _ = verify_payload_seal(cand_dir)
    assert not ok_del
    assert "[PAYLOAD_TAMPERED]" in msg_del
    assert "removed files" in msg_del

    # Restore f2 and verify it passes again
    f2.write_text("2\n", encoding="utf-8")
    assert verify_payload_seal(cand_dir)[0]

    # 2. Add an unauthorized payload file (not in ENVELOPE_FILENAMES)
    f3 = cand_dir / "unauthorized_probe.log"
    f3.write_text("injected log\n", encoding="utf-8")
    ok_add, msg_add, _ = verify_payload_seal(cand_dir)
    assert not ok_add
    assert "[PAYLOAD_TAMPERED]" in msg_add
    assert "added files" in msg_add


def _create_valid_candidate_dir(cand_dir: Path) -> Path:
    """Helper to populate a compliant candidate package ready for audit."""
    cand_dir.mkdir(parents=True, exist_ok=True)
    (cand_dir / "candidate_evidence.json").write_text(json.dumps({
        "status": "candidate_ready",
        "app_id": "uart_echo",
        "config_id": "standard",
        "target_app_dir": "peripherals/uart/uart_echo",
        "runner_mode": "qemu_worker",
        "checks": [
            {"kind": "baseline", "exit_code": 0, "accepted": True, "verdict": "pass"},
            {"kind": "assertion_self_check", "exit_code": 1, "accepted": True, "verdict": "killed"},
            {"kind": "recovery", "exit_code": 0, "accepted": True, "verdict": "restored"},
        ],
        "claims": ["CLAIM-UART-01"],
        "afg_verdict": "ELIGIBLE"
    }, indent=2), encoding="utf-8")

    (cand_dir / "proofplan.json").write_text(json.dumps({
        "schema_version": "1.0",
        "claims": [{"id": "CLAIM-UART-01", "evidence_class": "baseline"}]
    }, indent=2), encoding="utf-8")

    (cand_dir / "afg_evidence_receipt_v1_1.json").write_text(json.dumps({
        "overall_verdict": "ELIGIBLE",
        "schema_version": "1.1",
        "claims_total": 1,
        "claims_eligible": 1
    }, indent=2), encoding="utf-8")

    seal_candidate_payload(cand_dir, run_id="r1", app_id="uart_echo", config_id="standard")
    return cand_dir


def test_validate_candidate_for_audit_compliant(tmp_path: Path):
    """Verify a properly sealed candidate with checks, claims, and AFG receipt passes audit validation."""
    cand_dir = _create_valid_candidate_dir(tmp_path / "valid_pkg")
    ok, msg, data = validate_candidate_for_audit(cand_dir)
    assert ok, f"Expected compliant package to pass audit preflight: {msg}"
    assert data["status"] == "candidate_ready"


def test_validate_candidate_rejects_empty_checks_all_empty_bug(tmp_path: Path):
    """Verify I-09: checks: [] must be rejected, eliminating the python all([]) vulnerability."""
    cand_dir = _create_valid_candidate_dir(tmp_path / "empty_checks_pkg")
    # Overwrite candidate_evidence.json with empty checks array
    evidence_path = cand_dir / "candidate_evidence.json"
    evidence_data = json.loads(evidence_path.read_text(encoding="utf-8"))
    evidence_data["checks"] = []
    evidence_path.write_text(json.dumps(evidence_data), encoding="utf-8")
    seal_candidate_payload(cand_dir, run_id="r1", app_id="uart_echo", config_id="standard")

    ok, msg, _ = validate_candidate_for_audit(cand_dir)
    assert not ok
    assert "[NO_CHECKS]" in msg


def test_validate_candidate_rejects_missing_or_ineligible_afg_receipt(tmp_path: Path):
    """Verify candidate is rejected if AFG receipt is missing or has non-ELIGIBLE verdict."""
    cand_dir = _create_valid_candidate_dir(tmp_path / "ineligible_afg_pkg")
    receipt_path = cand_dir / "afg_evidence_receipt_v1_1.json"

    # 1. Modify AFG receipt to INELIGIBLE
    receipt_path.write_text(json.dumps({"overall_verdict": "MUTANT_SURVIVED"}), encoding="utf-8")
    seal_candidate_payload(cand_dir, run_id="r1", app_id="uart_echo", config_id="standard")

    ok, msg, _ = validate_candidate_for_audit(cand_dir)
    assert not ok
    assert "[AFG_NOT_ELIGIBLE]" in msg

    # 2. Remove AFG receipt completely
    receipt_path.unlink()
    cand_json = cand_dir / "candidate_evidence.json"
    data = json.loads(cand_json.read_text(encoding="utf-8"))
    data.pop("afg_verdict", None)
    cand_json.write_text(json.dumps(data), encoding="utf-8")
    seal_candidate_payload(cand_dir, run_id="r1", app_id="uart_echo", config_id="standard")

    ok_missing, msg_missing, _ = validate_candidate_for_audit(cand_dir)
    assert not ok_missing
    assert "[AFG_NOT_ELIGIBLE]" in msg_missing or "[AFG_MISSING]" in msg_missing


def test_sign_audit_decision_guards(tmp_path: Path):
    """Verify sign_audit_decision requires explicit auditor, blocks self-signing, and rejects ineligible candidate."""
    cand_dir = _create_valid_candidate_dir(tmp_path / "sign_guards_pkg")

    # 1. Empty auditor rejected
    rc_empty = sign_audit_decision(cand_dir, verdict="ACCEPT", auditor="   ", rationale="LGTM")
    assert rc_empty != 0

    # 2. Self-signing rejected (auditor matches runner_mode: 'qemu_worker')
    rc_self = sign_audit_decision(cand_dir, verdict="ACCEPT", auditor="qemu_worker", rationale="Self approved")
    assert rc_self != 0

    # 3. Ineligible candidate rejected when signing ACCEPT
    bad_cand_dir = tmp_path / "bad_pkg"
    bad_cand_dir.mkdir()
    (bad_cand_dir / "candidate_evidence.json").write_text('{"status":"failed"}\n', encoding="utf-8")
    rc_bad = sign_audit_decision(bad_cand_dir, verdict="ACCEPT", auditor="auditor_bob", rationale="Try accept")
    assert rc_bad != 0

    # 4. Valid audit signing succeeds
    rc_ok = sign_audit_decision(cand_dir, verdict="ACCEPT", auditor="auditor_alice", rationale="Verified 3 checks")
    assert rc_ok == 0

    audit_decision_file = cand_dir / "audit-decision.json"
    assert audit_decision_file.is_file()
    audit_data = json.loads(audit_decision_file.read_text(encoding="utf-8"))
    assert audit_data["verdict"] == "ACCEPT"
    assert audit_data["auditor"]["identity"] == "auditor_alice"
    assert audit_data["candidate_package_sha256"] == compute_dir_digest(cand_dir)


def test_inspect_candidate_cli_helper(tmp_path: Path, capsys):
    """Verify inspect_candidate returns 0 and displays audit eligibility accurately."""
    cand_dir = _create_valid_candidate_dir(tmp_path / "inspect_pkg")
    rc = inspect_candidate(cand_dir, output_json=True)
    assert rc == 0
    captured = capsys.readouterr()
    report = json.loads(captured.out)
    assert report["ready_for_audit"] is True
    assert report["payload_seal_intact"] is True
    assert report["checks_count"] == 3
