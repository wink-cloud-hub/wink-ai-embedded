# SPDX-License-Identifier: Apache-2.0
"""
Loop Runner & Task Scheduler
============================
Command-line driver for the autonomous ESP-IDF example governance loop.
Iterates planned entries, applies filters (lane/priority/app), and drives
the end-to-end pipeline with zero human intervention.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

from .pipeline import LoopPipeline, PipelineResult


class LoopRunner:
    """Discovers, filters, and batches checklist tasks through the autonomous pipeline."""

    def __init__(
        self,
        workspace_root: Path,
        dry_run: bool = False,
        custom_agent_cmd: Optional[str] = None,
    ):
        self.ws_root = workspace_root
        self.dry_run = dry_run
        self.vendor_root = workspace_root / "wink-micro-app" / "vendor" / "esp_idfv61"
        self.manifest_path = self.vendor_root / ".governance" / "data" / "checklist.data.json"
        self.pipeline = LoopPipeline(
            workspace_root=workspace_root,
            custom_agent_cmd=custom_agent_cmd,
            dry_run=dry_run,
        )

    def load_manifest(self) -> Dict[str, Any]:
        with open(self.manifest_path, "r", encoding="utf-8") as f:
            return json.load(f)

    def select_candidates(
        self,
        app_name: Optional[str] = None,
        lane: Optional[int] = None,
        priority: Optional[str] = None,
        limit: Optional[int] = None,
    ) -> List[Dict[str, Any]]:
        """Select eligible planned applications from checklist.data.json."""
        data = self.load_manifest()
        entries = data.get("entries", [])
        candidates = []

        for entry in entries:
            target_dir = entry.get("target_app_dir")
            if not target_dir:
                continue

            app_dir = self.vendor_root / target_dir
            # Only consider apps that are actually landed on disk
            if not app_dir.is_dir():
                continue

            # Check if specific app requested
            entry_app_name = app_dir.name
            if app_name and app_name != entry_app_name and app_name != entry.get("id"):
                continue

            # Check execution state: must be planned or building (or explicitly requested)
            execs = entry.get("executions", [])
            is_verified = any(ex.get("delivery_state") == "verified" for ex in execs)
            if is_verified and not app_name:
                continue

            # Lane filtering (matches target_dir conventions)
            if lane:
                lane_mapping = {
                    1: ["get-started", "system"],
                    2: ["peripherals/uart", "peripherals/i2c", "peripherals/spi"],
                    3: ["peripherals/timer", "peripherals/ledc", "peripherals/mcpwm", "peripherals/gptimer"],
                    4: ["peripherals/adc", "peripherals/dac"],
                    5: ["storage"],
                    6: ["wifi", "protocols", "bluetooth"],
                }
                keywords = lane_mapping.get(lane, [])
                if not any(kw in target_dir for kw in keywords):
                    continue

            candidates.append(entry)
            if limit and len(candidates) >= limit:
                break

        return candidates

    def run(
        self,
        app_name: Optional[str] = None,
        lane: Optional[int] = None,
        priority: Optional[str] = None,
        limit: Optional[int] = None,
        list_only: bool = False,
    ) -> int:
        """Run the autonomous loop over matched candidate entries."""
        print("=" * 76, flush=True)
        print("  WinkMicroOS Autonomous Governance Loop Runner (Option B Engine)", flush=True)
        print("  - Zero human intervention | Automated Canary Kill | Machine Audit", flush=True)
        print("=" * 76, flush=True)

        candidates = self.select_candidates(
            app_name=app_name,
            lane=lane,
            priority=priority,
            limit=limit,
        )

        if not candidates:
            print("[info] No eligible planned applications matched your criteria.", flush=True)
            return 0

        print(f"\n[loop] Found {len(candidates)} candidate application(s) to process.\n", flush=True)

        if list_only:
            for i, c in enumerate(candidates, 1):
                print(f"  {i}. {c.get('id'):<45} -> {c.get('target_app_dir')}", flush=True)
            return 0

        results: List[PipelineResult] = []
        for i, entry in enumerate(candidates, 1):
            app_id = entry.get("id")
            target_dir = entry.get("target_app_dir")
            print(f"\n--- [{i}/{len(candidates)}] Processing: {app_id} ({target_dir}) ---", flush=True)

            res = self.pipeline.execute_app(entry)
            results.append(res)

            status_tag = "PASS" if res.success else "FAIL"
            print(f"[{status_tag}] {app_id} -> {res.message}", flush=True)

        # Summary Table
        print("\n" + "=" * 76, flush=True)
        print("  AUTONOMOUS LOOP EXECUTION SUMMARY", flush=True)
        print("=" * 76, flush=True)
        passed_count = sum(1 for r in results if r.success)
        failed_count = sum(1 for r in results if not r.success)

        for r in results:
            tag = "[✓ PASS]" if r.success else "[✗ FAIL]"
            print(f"  {tag} {r.app_id:<45} (Stage: {r.stage})", flush=True)
            if not r.success:
                print(f"         Reason: {r.message[:80]}", flush=True)

        print("-" * 76, flush=True)
        print(f"  Total Processed: {len(results)} | Passed: {passed_count} | Failed/Blocked: {failed_count}", flush=True)
        print("=" * 76 + "\n", flush=True)

        return 0 if failed_count == 0 else 1


def main():
    parser = argparse.ArgumentParser(description="WinkMicroOS ESP-IDF Autonomous Governance Loop")
    parser.add_argument("--app", type=str, help="Target a specific application by name (e.g. i2c_basic)")
    parser.add_argument("--lane", type=int, choices=[1, 2, 3, 4, 5, 6], help="Target concurrency lane (1-6)")
    parser.add_argument("--priority", type=str, choices=["P0", "P1", "P2", "P3"], help="Filter by priority")
    parser.add_argument("--limit", type=int, help="Maximum number of applications to process in this run")
    parser.add_argument("--list", action="store_true", help="List matched candidates and exit without executing")
    parser.add_argument("--dry-run", action="store_true", help="Simulate execution without git commits or checklist writes")
    parser.add_argument("--agent-cmd", type=str, help="Custom headless agent CLI command (e.g. 'claude -p')")
    parser.add_argument("--workspace-root", type=str, default=".", help="Workspace root directory")

    args = parser.parse_args()
    ws_root = Path(args.workspace_root).resolve()

    runner = LoopRunner(
        workspace_root=ws_root,
        dry_run=args.dry_run,
        custom_agent_cmd=args.agent_cmd,
    )

    exit_code = runner.run(
        app_name=args.app,
        lane=args.lane,
        priority=args.priority,
        limit=args.limit,
        list_only=args.list,
    )
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
