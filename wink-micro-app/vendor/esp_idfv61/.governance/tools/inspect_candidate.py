# SPDX-License-Identifier: Apache-2.0
"""
inspect_candidate.py - Candidate Package Inspector & Audit Helper (L5-T2)
=========================================================================
Provides read-only candidate inspection (text / JSON) and explicit audit
decision signing (ACCEPT / REJECT / NEEDS_EVIDENCE) per TECH-DESIGN §6.
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import sys
from pathlib import Path
from typing import Any, Dict, Optional


def compute_dir_digest(target_dir: Path) -> str:
    """Compute deterministic SHA-256 tree digest of candidate package directory."""
    hasher = hashlib.sha256()
    for p in sorted(target_dir.rglob("*")):
        if p.is_file() and not p.name.endswith(".tmp"):
            rel = p.relative_to(target_dir).as_posix()
            fhash = hashlib.sha256(p.read_bytes()).hexdigest()
            hasher.update(f"{rel}:{fhash}\n".encode("utf-8"))
    return hasher.hexdigest()


def inspect_candidate(candidate_path: Path, output_json: bool = False) -> int:
    """Read-only presentation of candidate evidence and gaps within 15 seconds."""
    if candidate_path.is_dir():
        cand_json = candidate_path / "candidate_evidence.json"
        pkg_dir = candidate_path
    else:
        cand_json = candidate_path
        pkg_dir = candidate_path.parent

    if not cand_json.is_file():
        sys.stderr.write(f"Error: candidate_evidence.json not found at {cand_json}\n")
        return 1

    try:
        data = json.loads(cand_json.read_text(encoding="utf-8"))
    except Exception as exc:
        sys.stderr.write(f"Error: Invalid JSON in candidate file: {exc}\n")
        return 1

    pkg_digest = compute_dir_digest(pkg_dir)

    report_obj = {
        "inspector_version": "1.0",
        "inspected_at_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "package_directory": str(pkg_dir.resolve()),
        "package_sha256": pkg_digest,
        "app_id": data.get("app_id"),
        "config_id": data.get("config_id"),
        "target_app_dir": data.get("target_app_dir"),
        "status": data.get("status"),
        "stage": data.get("stage"),
        "runner_mode": data.get("runner_mode"),
        "assets_sha256": data.get("assets_sha256"),
        "checks_count": len(data.get("checks", [])),
        "checks": [
            {
                "kind": c.get("kind"),
                "exit_code": c.get("exit_code"),
                "accepted": c.get("accepted"),
                "verdict": c.get("verdict"),
            }
            for c in data.get("checks", [])
        ],
        "mutation": data.get("mutation"),
        "limitations": data.get("limitations", []),
        "ready_for_audit": data.get("status") == "candidate_ready" and all(c.get("accepted") for c in data.get("checks", []))
    }

    if output_json:
        print(json.dumps(report_obj, indent=2, ensure_ascii=False))
        return 0

    print("=" * 76)
    print("  CANDIDATE PACKAGE AUDIT INSPECTOR (L5-T2)")
    print("=" * 76)
    print(f"  App ID:           {report_obj['app_id']}")
    print(f"  Config ID:        {report_obj['config_id']}")
    print(f"  Target Dir:       {report_obj['target_app_dir']}")
    print(f"  Status:           {report_obj['status']} (Stage: {report_obj['stage']})")
    print(f"  Package SHA-256:  {report_obj['package_sha256']}")
    print(f"  Assets SHA-256:   {report_obj['assets_sha256']}")
    print("-" * 76)
    print(f"  Evidence Checks ({report_obj['checks_count']} total):")
    for chk in report_obj["checks"]:
        tag = "[✓ ACCEPTED]" if chk["accepted"] else "[✗ REJECTED]"
        print(f"    {tag:<14} Kind: {chk['kind']:<22} Exit: {chk['exit_code']} -> {chk['verdict']}")
    if report_obj["mutation"]:
        mut = report_obj["mutation"]
        print("-" * 76)
        print("  Mutation Evidence:")
        print(f"    Dimension:     {mut.get('dimension')}")
        print(f"    Target:        {mut.get('target')}")
        print(f"    Description:   {mut.get('mutation_desc')}")
    if report_obj["limitations"]:
        print("-" * 76)
        print("  Known Limitations & Gaps:")
        for lim in report_obj["limitations"]:
            print(f"    ! {lim}")
    print("=" * 76)
    print(f"  Audit Ready: {'YES (Ready for independent audit decision)' if report_obj['ready_for_audit'] else 'NO (Missing evidence)'}")
    print("=" * 76)
    return 0


def sign_audit_decision(
    candidate_path: Path,
    verdict: str,
    auditor: str,
    rationale: str,
    output_path: Optional[Path] = None
) -> int:
    """Submit explicit independent audit decision bound to immutable package digest."""
    if candidate_path.is_dir():
        cand_json = candidate_path / "candidate_evidence.json"
        pkg_dir = candidate_path
    else:
        cand_json = candidate_path
        pkg_dir = candidate_path.parent

    if not cand_json.is_file():
        sys.stderr.write(f"Error: candidate_evidence.json not found at {cand_json}\n")
        return 1

    data = json.loads(cand_json.read_text(encoding="utf-8"))
    pkg_digest = compute_dir_digest(pkg_dir)

    if verdict not in ("ACCEPT", "REJECT", "NEEDS_EVIDENCE"):
        sys.stderr.write(f"Error: Invalid verdict '{verdict}'. Allowed: ACCEPT, REJECT, NEEDS_EVIDENCE\n")
        return 1

    decision = {
        "$schema": "https://json-schema.winkmicroos.org/governance/audit-decision-v1.json",
        "candidate_package_sha256": pkg_digest,
        "app_id": data.get("app_id"),
        "config_id": data.get("config_id"),
        "auditor": {
            "identity": auditor,
            "role": "independent_reviewer",
            "signed_at_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        },
        "verdict": verdict,
        "contract_coverage_confirmed": verdict == "ACCEPT",
        "rationale": rationale
    }

    out_file = output_path or (pkg_dir / "audit-decision.json")
    out_file.write_text(json.dumps(decision, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"[✓ AUDIT SIGNED] Verdict: {verdict} by {auditor}")
    print(f"  Decision saved to: {out_file.resolve()}")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Candidate Inspector & Independent Audit Helper")
    parser.add_argument("--candidate", type=str, required=True, help="Path to candidate directory or candidate_evidence.json")
    parser.add_argument("--json", action="store_true", help="Output machine-readable JSON")
    parser.add_argument("--sign", choices=["ACCEPT", "REJECT", "NEEDS_EVIDENCE"], help="Sign audit decision")
    parser.add_argument("--auditor", type=str, default="arch_team", help="Auditor identity")
    parser.add_argument("--rationale", type=str, default="", help="Auditor rationale text")
    parser.add_argument("--out", type=str, help="Destination for audit-decision.json")

    args = parser.parse_args()
    cand_path = Path(args.candidate).resolve()

    if args.sign:
        rc = sign_audit_decision(
            candidate_path=cand_path,
            verdict=args.sign,
            auditor=args.auditor,
            rationale=args.rationale or f"Explicit audit {args.sign} decision submitted via inspector.",
            output_path=Path(args.out).resolve() if args.out else None
        )
        sys.exit(rc)

    rc = inspect_candidate(candidate_path=cand_path, output_json=args.json)
    sys.exit(rc)


if __name__ == "__main__":
    main()
