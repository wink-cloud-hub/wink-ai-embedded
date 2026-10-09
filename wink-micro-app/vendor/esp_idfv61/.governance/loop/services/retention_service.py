# SPDX-License-Identifier: Apache-2.0
"""
loop.services.retention_service - Tiered Retention & Disk Health Manager (GAP-03)
================================================================================
Enforces two-state tiered retention policy on runs/ directory:
1. ELIGIBLE Instant Slimming: Destroys heavy Wasm/JS/source binaries immediately,
   retaining only JSON receipts and audit logs (< 5% disk consumption).
2. REJECTED Quota Preservation: Retains full debug sandboxes for up to 10 latest
   failures, with max 48-hour time-to-live.
3. Global Hard Cap & LRU Eviction: Hard cap at 500 MB (local) / 2 GB (CI) with
   automatic LRU eviction to guarantee disk usage remains bounded.
"""
from __future__ import annotations

import datetime
import json
import os
import shutil
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


@dataclass
class RetentionConfig:
    max_eligible_hours: float = 24.0
    max_rejected_hours: float = 48.0
    max_rejected_quota: int = 10
    hard_cap_mb: float = 500.0
    target_cap_mb: float = 200.0


def compute_dir_size_bytes(directory: Path) -> int:
    """Recursively calculate total bytes in directory."""
    total = 0
    if not directory.is_dir():
        return 0
    try:
        for entry in os.scandir(directory):
            try:
                if entry.is_file(follow_symlinks=False):
                    total += entry.stat().st_size
                elif entry.is_dir(follow_symlinks=False):
                    total += compute_dir_size_bytes(Path(entry.path))
            except (OSError, PermissionError):
                pass
    except (OSError, PermissionError):
        pass
    return total


class RetentionService:
    """Manages disk lifecycle and garbage collection for governance runs."""

    def __init__(self, runs_dir: Path, config: Optional[RetentionConfig] = None):
        self.runs_dir = runs_dir.resolve()
        self.cfg = config or RetentionConfig()

    def slim_eligible_run(self, run_dir: Path) -> Tuple[int, int]:
        """Slim an ELIGIBLE run directory by removing heavy build & binary assets."""
        if not run_dir.is_dir():
            return 0, 0

        initial_size = compute_dir_size_bytes(run_dir)
        heavy_extensions = {".wasm", ".js", ".map", ".a", ".o", ".bin", ".elf", ".tar", ".zip"}

        # 1. Remove app source copy directory if exists
        app_copy_dir = run_dir / "app"
        if app_copy_dir.is_dir():
            try:
                shutil.rmtree(app_copy_dir, ignore_errors=True)
            except OSError:
                pass

        # 2. Remove heavy files in run_dir and subdirectories
        for root, dirs, files in os.walk(run_dir, topdown=False):
            for f in files:
                ext = Path(f).suffix.lower()
                if ext in heavy_extensions:
                    p = Path(root) / f
                    try:
                        p.unlink(missing_ok=True)
                    except OSError:
                        pass

        final_size = compute_dir_size_bytes(run_dir)
        bytes_freed = max(0, initial_size - final_size)
        return initial_size, bytes_freed

    def inspect_run_verdict(self, run_dir: Path) -> str:
        """Determines run outcome: ELIGIBLE, REJECTED, or UNKNOWN."""
        afg_receipt = run_dir / "afg_evidence_receipt_v1_1.json"
        if afg_receipt.is_file():
            try:
                data = json.loads(afg_receipt.read_text(encoding="utf-8"))
                return data.get("overall_verdict", "UNKNOWN")
            except Exception:
                pass

        cand_evidence = run_dir / "candidate_evidence.json"
        if cand_evidence.is_file():
            try:
                data = json.loads(cand_evidence.read_text(encoding="utf-8"))
                st = data.get("status")
                if st == "candidate_ready":
                    return "ELIGIBLE"
                elif st in ("failed", "rejected", "canary_kill_failed"):
                    return "REJECTED"
            except Exception:
                pass

        return "UNKNOWN"

    def enforce_retention(
        self,
        is_ci: bool = False,
        exclude_run_ids: Optional[List[str]] = None,
        min_age_seconds: float = 60.0,
    ) -> Dict[str, Any]:
        """Enforces full tiered retention policy across all runs."""
        if not self.runs_dir.is_dir():
            return {"status": "NO_RUNS_DIR", "freed_mb": 0.0}

        hard_cap = (2048.0 if is_ci else self.cfg.hard_cap_mb) * 1024 * 1024
        target_cap = (1024.0 if is_ci else self.cfg.target_cap_mb) * 1024 * 1024
        now = time.time()

        run_entries: List[Dict[str, Any]] = []
        for item in self.runs_dir.iterdir():
            if item.is_dir() and not item.name.startswith("."):
                try:
                    mtime = item.stat().st_mtime
                    size = compute_dir_size_bytes(item)
                    verdict = self.inspect_run_verdict(item)
                    run_entries.append({
                        "path": item,
                        "mtime": mtime,
                        "size": size,
                        "verdict": verdict,
                        "age_hours": (now - mtime) / 3600.0,
                    })
                except (OSError, PermissionError):
                    pass

        total_freed = 0
        deleted_count = 0

        # Pass 1: Slim all eligible runs that haven't been slimmed and are older than min_age_seconds
        for r in run_entries:
            if exclude_run_ids and r["path"].name in exclude_run_ids:
                continue
            if r["verdict"] == "ELIGIBLE" and (now - r["mtime"]) >= min_age_seconds:
                _, freed = self.slim_eligible_run(r["path"])
                total_freed += freed

        # Pass 2: Expire old runs based on TTL
        remaining_runs: List[Dict[str, Any]] = []
        for r in run_entries:
            should_delete = False
            if r["verdict"] == "ELIGIBLE" and r["age_hours"] > self.cfg.max_eligible_hours:
                should_delete = True
            elif r["verdict"] == "REJECTED" and r["age_hours"] > self.cfg.max_rejected_hours:
                should_delete = True

            if should_delete:
                try:
                    shutil.rmtree(r["path"], ignore_errors=True)
                    total_freed += r["size"]
                    deleted_count += 1
                except OSError:
                    remaining_runs.append(r)
            else:
                remaining_runs.append(r)

        # Pass 3: Enforce rejected quota (keep latest N)
        rejected_runs = [r for r in remaining_runs if r["verdict"] == "REJECTED"]
        rejected_runs.sort(key=lambda x: x["mtime"], reverse=True)
        if len(rejected_runs) > self.cfg.max_rejected_quota:
            for r in rejected_runs[self.cfg.max_rejected_quota:]:
                try:
                    shutil.rmtree(r["path"], ignore_errors=True)
                    total_freed += r["size"]
                    deleted_count += 1
                    if r in remaining_runs:
                        remaining_runs.remove(r)
                except OSError:
                    pass

        # Pass 4: LRU eviction if hard cap exceeded
        current_total_size = sum(r["size"] for r in remaining_runs)
        if current_total_size > hard_cap:
            remaining_runs.sort(key=lambda x: x["mtime"])  # Oldest first
            for r in remaining_runs:
                if current_total_size <= target_cap:
                    break
                try:
                    shutil.rmtree(r["path"], ignore_errors=True)
                    total_freed += r["size"]
                    current_total_size -= r["size"]
                    deleted_count += 1
                except OSError:
                    pass

        return {
            "status": "COMPLETED",
            "deleted_runs": deleted_count,
            "freed_mb": round(total_freed / (1024 * 1024), 2),
            "remaining_size_mb": round(compute_dir_size_bytes(self.runs_dir) / (1024 * 1024), 2),
        }

    apply_retention_policy = enforce_retention

