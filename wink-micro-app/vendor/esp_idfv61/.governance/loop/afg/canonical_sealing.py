# SPDX-License-Identifier: Apache-2.0
"""
canonical_sealing.py - Payload & Envelope Separation & Canonical Sealing (T1.6)
================================================================================
Implements RFC 8785 (JCS) JSON canonicalization, cross-platform LF line
normalization, immutable payload vs mutable envelope separation, tamper detection,
and strict preflight audit candidate validation (addressing I-08, I-09, AFG-NEW-02).
"""
from __future__ import annotations

import datetime
import hashlib
import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple


# Set of filenames reserved for envelope metadata (excluded from payload digests)
ENVELOPE_FILENAMES: Set[str] = {
    "package_summary.json",
    "audit-decision.json",
    "promotion_receipt.json",
    "promotion-receipt.json",
}

# Text extensions subject to LF newline normalization
TEXT_EXTENSIONS: Set[str] = {
    ".json", ".jsonl", ".txt", ".diff", ".patch",
    ".yaml", ".yml", ".c", ".h", ".md", ".log"
}


def canonical_json_dumps(obj: Any) -> str:
    """
    Serialize Python object into canonical JSON per RFC 8785 (JCS).
    Ensures deterministic key sorting, compact delimiters without whitespace,
    and unescaped UTF-8 characters.
    """
    return json.dumps(
        obj,
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False
    )


def canonical_json_bytes(obj: Any) -> bytes:
    """Return UTF-8 encoded canonical JSON bytes."""
    return canonical_json_dumps(obj).encode("utf-8")


def compute_normalized_file_sha256(path: Path) -> str:
    """
    Compute SHA-256 digest of file with LF line normalization for text files.
    This guarantees bit-identical hashes across Windows (CRLF) and Linux (LF).
    Binary files are read raw.
    """
    raw_data = path.read_bytes()
    if path.suffix.lower() in TEXT_EXTENSIONS:
        try:
            text = raw_data.decode("utf-8")
            # Normalize CRLF and standalone CR to LF
            normalized_bytes = text.replace("\r\n", "\n").replace("\r", "\n").encode("utf-8")
            return hashlib.sha256(normalized_bytes).hexdigest()
        except UnicodeDecodeError:
            pass
    return hashlib.sha256(raw_data).hexdigest()


def is_envelope_file(rel_path: str) -> bool:
    """
    Check if a relative path corresponds to an envelope file or temporary file.
    Envelope files are metadata wrappers and must NOT be included in payload hashes.
    """
    name = Path(rel_path).name
    if name in ENVELOPE_FILENAMES:
        return True
    if name.endswith(".tmp") or ".tmp." in name:
        return True
    return False


def compute_payload_manifest(candidate_dir: Path) -> Tuple[str, List[Dict[str, str]]]:
    """
    Compute deterministic SHA-256 payload digest and sorted file manifest.
    Strictly separates immutable payload files from mutable envelope files.
    """
    manifest_entries: List[Dict[str, str]] = []
    hasher = hashlib.sha256()

    for p in sorted(candidate_dir.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(candidate_dir).as_posix()
        if is_envelope_file(rel):
            continue

        fhash = compute_normalized_file_sha256(p)
        manifest_entries.append({"path": rel, "sha256": fhash})
        hasher.update(f"{rel}:{fhash}\n".encode("utf-8"))

    payload_digest = hasher.hexdigest()
    return payload_digest, manifest_entries


def seal_candidate_payload(
    candidate_dir: Path,
    run_id: str,
    app_id: str,
    config_id: str,
    sealed_at_utc: Optional[str] = None
) -> Dict[str, Any]:
    """
    Seal the immutable payload files of a candidate package into package_summary.json.
    All payload files must be completely written and closed BEFORE calling this function.
    No payload files may be modified after sealing.
    """
    if not candidate_dir.is_dir():
        raise FileNotFoundError(f"Candidate directory does not exist: {candidate_dir}")

    payload_digest, manifest_entries = compute_payload_manifest(candidate_dir)
    if not manifest_entries:
        raise ValueError(f"Cannot seal empty candidate directory: {candidate_dir}")

    now_iso = sealed_at_utc or datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    summary_data = {
        "schema_version": "1.0",
        "run_id": run_id,
        "app_id": app_id,
        "config_id": config_id,
        "payload_sha256": payload_digest,
        "package_sha256": payload_digest,  # Backward compatibility
        "payload_files_count": len(manifest_entries),
        "payload_manifest": manifest_entries,
        "status": "candidate_ready",
        "sealed_at_utc": now_iso
    }

    summary_path = candidate_dir / "package_summary.json"
    summary_path.write_text(json.dumps(summary_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return summary_data


def verify_payload_seal(candidate_dir: Path) -> Tuple[bool, str, Optional[Dict[str, Any]]]:
    """
    Verify that the payload files in candidate_dir have not been altered, deleted,
    or augmented after sealing. Envelope files are permitted to exist or be added.
    """
    summary_path = candidate_dir / "package_summary.json"
    if not summary_path.is_file():
        return False, "Missing package_summary.json in candidate directory", None

    try:
        summary_data = json.loads(summary_path.read_text(encoding="utf-8"))
    except Exception as exc:
        return False, f"Invalid JSON in package_summary.json: {exc}", None

    expected_payload_sha = summary_data.get("payload_sha256") or summary_data.get("package_sha256")
    if not expected_payload_sha:
        return False, "package_summary.json does not contain payload_sha256", summary_data

    actual_payload_sha, actual_manifest = compute_payload_manifest(candidate_dir)
    if actual_payload_sha != expected_payload_sha:
        # Determine specific discrepancies if manifest is available
        expected_manifest = {m["path"]: m["sha256"] for m in summary_data.get("payload_manifest", [])}
        actual_manifest_map = {m["path"]: m["sha256"] for m in actual_manifest}

        added = set(actual_manifest_map.keys()) - set(expected_manifest.keys())
        removed = set(expected_manifest.keys()) - set(actual_manifest_map.keys())
        modified = [
            p for p in set(expected_manifest.keys()) & set(actual_manifest_map.keys())
            if expected_manifest[p] != actual_manifest_map[p]
        ]

        details = []
        if added:
            details.append(f"added files: {sorted(list(added))}")
        if removed:
            details.append(f"removed files: {sorted(list(removed))}")
        if modified:
            details.append(f"modified files: {sorted(modified)}")

        detail_str = f" ({'; '.join(details)})" if details else ""
        return False, f"[PAYLOAD_TAMPERED] Payload digest mismatch{detail_str}", summary_data

    return True, "Payload seal verified and intact", summary_data


def validate_candidate_for_audit(candidate_dir: Path) -> Tuple[bool, str, Dict[str, Any]]:
    """
    Perform strict preflight validation of candidate package before independent audit.
    Fixes I-09 (all([]) vulnerability, missing checks, unsealed payload, missing AFG receipt).
    """
    cand_json = candidate_dir / "candidate_evidence.json"
    if not cand_json.is_file():
        return False, f"candidate_evidence.json not found at {cand_json}", {}

    try:
        data = json.loads(cand_json.read_text(encoding="utf-8"))
    except Exception as exc:
        return False, f"Invalid JSON in candidate_evidence.json: {exc}", {}

    # 1. Status check
    if data.get("status") != "candidate_ready":
        return False, f"Candidate status is '{data.get('status')}', expected 'candidate_ready'", data

    # 2. Checks check (Fix I-09: all([]) is True on empty list!)
    checks = data.get("checks")
    if not isinstance(checks, list) or len(checks) == 0:
        return False, "[NO_CHECKS] Candidate has no evidence checks (empty checks array)", data

    failed_checks = [c for c in checks if not c.get("accepted")]
    if failed_checks:
        return False, f"[CHECKS_FAILED] {len(failed_checks)} evidence check(s) rejected", data

    # 3. ProofPlan check
    proofplan_file = candidate_dir / "proofplan.json"
    has_claims = bool(data.get("claims"))
    if not proofplan_file.is_file() and not has_claims:
        return False, "[PROOFPLAN_MISSING] Missing proofplan.json or claims in candidate package", data

    # 4. AFG verification receipt check
    afg_receipt_file = candidate_dir / "afg_evidence_receipt_v1_1.json"
    if not afg_receipt_file.is_file():
        afg_receipt_file = candidate_dir / "canary_mutation_kill_receipt.json"

    afg_verdict = None
    if afg_receipt_file.is_file():
        try:
            receipt_data = json.loads(afg_receipt_file.read_text(encoding="utf-8"))
            afg_verdict = receipt_data.get("overall_verdict")
        except Exception:
            pass

    if not afg_verdict:
        afg_verdict = data.get("afg_verdict")

    if afg_verdict != "ELIGIBLE":
        return False, f"[AFG_NOT_ELIGIBLE] AFG verification verdict is '{afg_verdict}', expected 'ELIGIBLE'", data

    # 5. Seal check
    seal_intact, seal_msg, _ = verify_payload_seal(candidate_dir)
    if not seal_intact:
        return False, f"[SEAL_BROKEN] {seal_msg}", data

    return True, "Candidate package meets all audit prerequisites", data
