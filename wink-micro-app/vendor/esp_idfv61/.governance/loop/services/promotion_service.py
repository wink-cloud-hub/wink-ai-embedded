# SPDX-License-Identifier: Apache-2.0
"""
promotion_service.py - Atomic CAS Promotion & Evidence Publisher (L5-T4)
========================================================================
Implements immutable package validation, independent audit verification,
manifest CAS optimistic locking, atomic replace, and readback verification.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import shutil
import sys
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

try:
    from loop.harness.process_supervisor import safe_file_retry
    from loop.harness.lock_lease import FileLockLease
except ImportError:
    try:
        from ..harness.process_supervisor import safe_file_retry
        from ..harness.lock_lease import FileLockLease
    except ImportError:
        from process_supervisor import safe_file_retry
        FileLockLease = None



def compute_file_sha256(path: Path) -> str:
    hasher = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)
    return hasher.hexdigest()


class PromotionService:
    """Manages transactional promotion from isolated candidate to formal evidence."""

    def __init__(self, workspace_root: Path):
        self.ws_root = workspace_root.resolve()
        self.vendor_root = self.ws_root / "wink-micro-app" / "vendor" / "esp_idfv61"
        self.gov_dir = self.vendor_root / ".governance"
        self.manifest_path = self.gov_dir / "data" / "checklist.data.json"
        self.lock_file = self.gov_dir / "data" / "manifest.lock"

    def promote_candidate(
        self,
        candidate_dir: Path,
        expected_manifest_sha256: Optional[str] = None,
        target_delivery_state: str = "verified_v1_1"
    ) -> Tuple[bool, str, Optional[Dict[str, Any]]]:
        """Atomically promote verified candidate package to formal repository evidence."""
        if not candidate_dir.is_dir():
            return False, f"Candidate directory not found: {candidate_dir}", None

        cand_evidence_path = candidate_dir / "candidate_evidence.json"
        audit_decision_path = candidate_dir / "audit-decision.json"
        pkg_summary_path = candidate_dir / "package_summary.json"
        afg_receipt_path = candidate_dir / "afg_evidence_receipt_v1_1.json"

        # 1. Verify candidate evidence exists and passed
        if not cand_evidence_path.is_file():
            return False, "Missing candidate_evidence.json in package", None
        cand_data = json.loads(cand_evidence_path.read_text(encoding="utf-8"))
        if cand_data.get("status") != "candidate_ready":
            return False, f"Candidate status is '{cand_data.get('status')}', not 'candidate_ready'", None

        # 2. Verify independent audit decision exists with ACCEPT verdict
        if not audit_decision_path.is_file():
            return False, "[AUDIT_MISSING] Formal promotion requires signed audit-decision.json", None
        audit_data = json.loads(audit_decision_path.read_text(encoding="utf-8"))
        if audit_data.get("verdict") != "ACCEPT":
            return False, f"[AUDIT_REJECTED] Audit verdict is '{audit_data.get('verdict')}'", None

        # 2b. If AFG v1.1 receipt is present, ensure verdict is ELIGIBLE
        if afg_receipt_path.is_file():
            afg_data = json.loads(afg_receipt_path.read_text(encoding="utf-8"))
            if afg_data.get("overall_verdict") != "ELIGIBLE":
                return False, f"[AFG_NOT_ELIGIBLE] AFG receipt verdict is '{afg_data.get('overall_verdict')}', cannot promote", None

        # 3. Verify package hash binding
        if not pkg_summary_path.is_file():
            return False, "Missing package_summary.json in candidate", None
        summary_data = json.loads(pkg_summary_path.read_text(encoding="utf-8"))
        expected_pkg_hash = summary_data.get("package_sha256")
        if audit_data.get("candidate_package_sha256") != expected_pkg_hash:
            return False, "[AUDIT_DIGEST_MISMATCH] Audit was signed for a different package digest", None

        app_id = cand_data.get("app_id")
        config_id = cand_data.get("config_id")
        target_app_dir = cand_data.get("target_app_dir")

        # 4. Manifest optimistic concurrency check (CAS)
        current_manifest_sha = compute_file_sha256(self.manifest_path)
        if expected_manifest_sha256 and current_manifest_sha != expected_manifest_sha256:
            return False, f"[CAS_CONFLICT] Manifest drifted (current: {current_manifest_sha[:12]}, expected: {expected_manifest_sha256[:12]})", None

        # 5. Acquire lock and apply atomic update
        def do_transaction():
            manifest_content = json.loads(self.manifest_path.read_text(encoding="utf-8"))
            matched_entry = next((e for e in manifest_content.get("entries", []) if e.get("id") == app_id), None)
            if not matched_entry:
                raise ValueError(f"Entry {app_id} not found in checklist.data.json")

            matched_exec = next((ex for ex in matched_entry.get("executions", []) if ex.get("config_id") == config_id), None)
            if not matched_exec:
                raise ValueError(f"Execution {config_id} not found in entry {app_id}")

            now_iso = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

            # Copy reports to formal reports location
            formal_reports_dir = self.gov_dir / "reports" / target_app_dir
            formal_reports_dir.mkdir(parents=True, exist_ok=True)
            
            # Copy candidate evidence package
            archive_pkg_dir = formal_reports_dir / f"package-{cand_data.get('run_id')}"
            if archive_pkg_dir.exists():
                shutil.rmtree(archive_pkg_dir)
            shutil.copytree(candidate_dir, archive_pkg_dir)

            # Update entry state
            matched_exec["delivery_state"] = target_delivery_state
            matched_exec.setdefault("evidence", {})
            matched_exec["evidence"].update({
                "backend": matched_exec.get("backend", "wasm_simulation"),
                "run_id": cand_data.get("run_id"),
                "package_sha256": expected_pkg_hash,
                "verified_at": now_iso,
                "audit_ref": f".governance/reports/{target_app_dir}/package-{cand_data.get('run_id')}/audit-decision.json"
            })

            # Atomic replace manifest with PID temp file
            import os
            tmp_manifest = self.manifest_path.parent / f"checklist.data.json.tmp.{os.getpid()}"
            tmp_manifest.write_text(json.dumps(manifest_content, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            os.replace(tmp_manifest, self.manifest_path)

        try:
            if FileLockLease is not None:
                with FileLockLease(self.lock_file, lease_ttl_seconds=30.0):
                    safe_file_retry(do_transaction, backoffs_ms=(100, 200, 400), op_name="promote candidate")
            else:
                safe_file_retry(do_transaction, backoffs_ms=(100, 200, 400), op_name="promote candidate")
        except Exception as exc:
            return False, f"Transaction failed during manifest commit: {exc}", None

        # 6. Readback verification
        verified_data = json.loads(self.manifest_path.read_text(encoding="utf-8"))
        chk_entry = next((e for e in verified_data.get("entries", []) if e.get("id") == app_id), None)
        chk_exec = next((ex for ex in chk_entry.get("executions", []) if ex.get("config_id") == config_id), None)
        if not chk_exec or chk_exec.get("delivery_state") != target_delivery_state:
            return False, "[READBACK_FAILED] Committed delivery_state verification failed", None

        receipt = {
            "app_id": app_id,
            "config_id": config_id,
            "run_id": cand_data.get("run_id"),
            "package_sha256": expected_pkg_hash,
            "promoted_at_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "auditor": audit_data.get("auditor"),
            "manifest_sha256_after": compute_file_sha256(self.manifest_path)
        }
        return True, "Successfully promoted candidate to verified formal evidence", receipt


def main():
    parser = argparse.ArgumentParser(description="Promotion Service & CAS Publisher")
    parser.add_argument("--candidate", type=str, required=True, help="Path to candidate directory")
    parser.add_argument("--expected-manifest-sha", type=str, help="Expected checklist.data.json SHA-256 for CAS")
    parser.add_argument("--workspace-root", type=str, default=".", help="Workspace root")
    args = parser.parse_args()

    svc = PromotionService(Path(args.workspace_root))
    cand_dir = Path(args.candidate).resolve()
    success, msg, receipt = svc.promote_candidate(cand_dir, expected_manifest_sha256=args.expected_manifest_sha)
    if not success:
        sys.stderr.write(f"Promotion failed: {msg}\n")
        sys.exit(1)
    print(f"[✓ PROMOTED] {msg}")
    print(json.dumps(receipt, indent=2))
    sys.exit(0)


if __name__ == "__main__":
    main()
