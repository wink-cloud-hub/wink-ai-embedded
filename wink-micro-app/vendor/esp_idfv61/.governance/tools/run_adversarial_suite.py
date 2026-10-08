#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
run_adversarial_suite.py
========================
Executes the full adversarial negative regression suite (AT-01 ~ AT-35)
and generates auditable receipts under:
.governance/runs/<run_id>/adversarial/<test_id>/receipt.json
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, List

TOOLS_DIR = Path(__file__).resolve().parent
GOV_DIR = TOOLS_DIR.parent
VENDOR_DIR = GOV_DIR.parent
WORKSPACE_ROOT = VENDOR_DIR.parents[2]


def main():
    parser = argparse.ArgumentParser(description="Run Adversarial Test Suite & Generate Receipts")
    parser.add_argument("--run-id", type=str, default="", help="Run identifier")
    parser.add_argument("--out-dir", type=str, default="", help="Custom output directory")
    args = parser.parse_args()

    run_id = args.run_id or datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ-adversarial-suite")
    out_dir = Path(args.out_dir).resolve() if args.out_dir else (GOV_DIR / "runs" / run_id / "adversarial")
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"============================================================")
    print(f"Executing Adversarial Test Suite AT-01 ~ AT-35")
    print(f"Run ID: {run_id}")
    print(f"Receipts Directory: {out_dir}")
    print(f"============================================================")

    test_file = GOV_DIR / "gates" / "tests" / "test_adversarial_suite.py"

    # Discover tests
    tests = [f"test_at_{i:02d}" for i in range(1, 36)]
    # Map test ID to test name
    at_names = {
        "AT-01": "Borrowed Report Identity Mismatch Rejected",
        "AT-02": "SoC / Profile Label Swap Rejected",
        "AT-03": "Scenario or Report Hash Mismatch Rejected",
        "AT-04": "Invalid Observation (null/missing) Rejected",
        "AT-05": "Mixed Fixture vs Business Assertion Disambiguation",
        "AT-06": "Fail-Fast Incomplete Evidence Pending",
        "AT-07": "External Injection Taint Detection",
        "AT-08": "Ineffective or Empty Mutation Not Counted as Kill",
        "AT-09": "Canary Killed But Business Defect Survives Rejected",
        "AT-10": "Infrastructure Crash or Timeout Not Counted as Kill",
        "AT-11": "Controlled Fault Handling & Recovery Accepted",
        "AT-12": "Recovery Failure Halts Candidate Readiness",
        "AT-13": "Header / Runtime / Toolchain Dependency Invalidation",
        "AT-14": "Timer Observation Distinction (Legitimate 0 vs Uptime)",
        "AT-15": "Tampered Package Sealing CAS Blocked",
        "AT-16": "Agent Boundary Prevents Formal Directory Write",
        "AT-17": "Quota Failover Terminates Old Attempt Processes",
        "AT-18": "Timeout Bounded Execution and Cleanup",
        "AT-19": "Promotion Crash Recovery Idempotence",
        "AT-20": "Concurrent Promotion Conflict (Optimistic CAS)",
        "AT-21": "Lane Partition Disjoint and Complete",
        "AT-22": "Corrupted or Tampered Checkpoint Rejected",
        "AT-23": "Same Process State and Generation Reset",
        "AT-24": "Capability Mismatch (Deep Sleep vs Light Sleep) Caught",
        "AT-25": "Legitimate Steady-State and Zero Count Accepted",
        "AT-26": "Missing Recovery Evidence Blocks Promotion",
        "AT-27": "Incomplete Upstream Source Manifest Rejected",
        "AT-28": "Virtual Clock 20-Iteration Replay Determinism",
        "AT-29": "Process Tree Bounded Cleanup (Parent Exit & Pipe Inherit)",
        "AT-30": "Mutation Catalog 15 Operators Classification",
        "AT-31": "Tool Capabilities Baseline Verified",
        "AT-32": "Environment Lock Drift Detection",
        "AT-33": "Requirements Trace Coverage (RC-01 ~ RC-14)",
        "AT-34": "Audit Rejection / Pending Blocks Formal Promotion",
        "AT-35": "SoC Mismatch ESP32 vs ESP32-S3 Target Rejected"
    }

    results_summary = []
    start_all = time.time()

    for idx in range(1, 36):
        at_id = f"AT-{idx:02d}"
        at_name = at_names.get(at_id, f"Adversarial Test {at_id}")
        fn_pattern = f"test_at_{idx:02d}"

        test_dir = out_dir / at_id
        test_dir.mkdir(parents=True, exist_ok=True)
        receipt_file = test_dir / "receipt.json"

        t0 = time.time()
        cmd = [sys.executable, "-X", "utf8", "-B", "-m", "pytest", str(test_file), "-k", fn_pattern, "-q"]
        res = subprocess.run(cmd, cwd=str(WORKSPACE_ROOT), capture_output=True, text=True, encoding="utf-8")
        duration_ms = int((time.time() - t0) * 1000)

        passed = (res.returncode == 0)
        status = "PASSED" if passed else "FAILED"

        receipt_data = {
            "schema_version": "1.0",
            "test_id": at_id,
            "name": at_name,
            "status": status,
            "executed_at_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "duration_ms": duration_ms,
            "returncode": res.returncode,
            "stdout_summary": res.stdout.strip().split("\n")[-1] if res.stdout.strip() else "",
            "expected_verdict": "REJECT / CONTRACT_ENFORCED",
            "actual_verdict": "CONFIRMED" if passed else "FAILURE"
        }
        receipt_json = json.dumps(receipt_data, indent=2, ensure_ascii=False) + "\n"
        receipt_file.write_text(receipt_json, encoding="utf-8")

        results_summary.append({
            "test_id": at_id,
            "name": at_name,
            "status": status,
            "duration_ms": duration_ms,
            "receipt_path": str(receipt_file.relative_to(GOV_DIR))
        })

        tag = "PASSED" if passed else "FAILED"
        print(f"[{tag:<6}] {at_id}: {at_name} ({duration_ms}ms)")

    total_duration = time.time() - start_all
    passed_count = sum(1 for r in results_summary if r["status"] == "PASSED")
    failed_count = sum(1 for r in results_summary if r["status"] == "FAILED")

    summary_file = out_dir / "adversarial_suite_summary.json"
    summary_data = {
        "schema_version": "1.0",
        "run_id": run_id,
        "completed_at_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "total_tests": len(results_summary),
        "passed_tests": passed_count,
        "failed_tests": failed_count,
        "all_passed": (failed_count == 0),
        "total_duration_sec": round(total_duration, 2),
        "tests": results_summary
    }
    summary_file.write_text(json.dumps(summary_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    print(f"\n============================================================")
    print(f"Summary: {passed_count}/{len(results_summary)} PASSED in {round(total_duration, 2)}s")
    print(f"Summary Receipt: {summary_file}")
    print(f"============================================================")

    return 0 if failed_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
