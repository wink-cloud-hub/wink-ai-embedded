# SPDX-License-Identifier: Apache-2.0
"""
Small-Batch Rollout Engine (V1-T2)
==================================
Implements phased, evidence-based expansion for planned checklist entries:
1. Selects representative pilot configurations (Pilot Slice) per lane/domain.
2. Evaluates pilot results: any mutation survival or infrastructure failure
   immediately freezes domain rollout (Stop-the-Line).
3. Only permits expansion batches when pilots achieve 100% candidate readiness.
"""

from __future__ import annotations

import argparse
import datetime
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional

LOOP_DIR = Path(__file__).resolve().parent
TOOLS_DIR = LOOP_DIR.parent
VENDOR_ROOT = TOOLS_DIR.parent.parent


@dataclass
class RolloutDecision:
    status: str  # PILOT_PASSED | DOMAIN_FROZEN | INSUFFICIENT_EVIDENCE
    domain_or_lane: str
    message: str
    pilot_count: int
    passed_count: int
    failed_items: List[str] = field(default_factory=list)
    expansion_authorized: bool = False
    max_expansion_size: int = 0


class RolloutManager:
    """Manages pilot slicing and safe batch expansion."""

    def __init__(self, vendor_root: Optional[Path] = None):
        self.vendor_root = vendor_root or VENDOR_ROOT
        self.manifest_path = self.vendor_root / ".governance" / "data" / "checklist.data.json"
        self.state_dir = self.vendor_root / ".governance" / "runs" / "rollout_state"
        self.state_dir.mkdir(parents=True, exist_ok=True)
        self.state_file = self.state_dir / "rollout_registry.json"

    def _load_manifest(self) -> Dict[str, Any]:
        with open(self.manifest_path, "r", encoding="utf-8") as f:
            return json.load(f)

    def _load_state(self) -> Dict[str, Any]:
        if self.state_file.is_file():
            try:
                return json.loads(self.state_file.read_text(encoding="utf-8"))
            except Exception:
                pass
        return {"domains": {}, "frozen_reasons": {}, "updated_at": None}

    def _save_state(self, state: Dict[str, Any]) -> None:
        state["updated_at"] = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        self.state_file.write_text(json.dumps(state, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    def create_pilot_batch(
        self,
        lane: Optional[int] = None,
        domain_keyword: Optional[str] = None,
        pilot_size: int = 3,
    ) -> Dict[str, Any]:
        """Select a representative pilot batch for a lane or domain."""
        state = self._load_state()
        domain_key = f"lane_{lane}" if lane else (domain_keyword or "general")

        # Check if domain is already frozen
        if state.get("domains", {}).get(domain_key) == "FROZEN":
            reason = state.get("frozen_reasons", {}).get(domain_key, "Unknown previous failure")
            return {
                "status": "BLOCKED",
                "domain_key": domain_key,
                "message": f"Domain '{domain_key}' is FROZEN due to previous failure: {reason}",
                "entries": [],
            }

        data = self._load_manifest()
        entries = data.get("entries", [])
        matched = []

        lane_mapping = {
            1: ["get-started", "system"],
            2: ["peripherals/uart", "peripherals/i2c", "peripherals/spi"],
            3: ["peripherals/timer", "peripherals/ledc", "peripherals/mcpwm", "peripherals/gptimer"],
            4: ["peripherals/adc", "peripherals/dac"],
            5: ["storage"],
            6: ["wifi", "protocols", "bluetooth"],
        }
        keywords = lane_mapping.get(lane, [domain_keyword]) if lane else [domain_keyword]

        for entry in entries:
            target_dir = entry.get("target_app_dir", "")
            # Only consider planned entries
            execs = entry.get("executions", [])
            is_planned = any(ex.get("delivery_state") == "planned" for ex in execs)
            if not is_planned:
                continue

            if keywords and keywords[0]:
                if any(kw in target_dir for kw in keywords if kw):
                    matched.append(entry)
            else:
                matched.append(entry)

            if len(matched) >= pilot_size:
                break

        now_str = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        batch_id = f"PILOT-{domain_key}-{now_str}"
        pilot_manifest = {
            "batch_id": batch_id,
            "domain_key": domain_key,
            "created_at_utc": now_str,
            "pilot_size": len(matched),
            "status": "SCHEDULED",
            "entries": [{"id": e.get("id"), "target_app_dir": e.get("target_app_dir")} for e in matched],
            "budget": {
                "timeout_per_entry_sec": 120,
                "max_attempts": 2,
            },
        }

        # Save pilot batch manifest
        batch_file = self.state_dir / f"{batch_id}.json"
        batch_file.write_text(json.dumps(pilot_manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        pilot_manifest["manifest_path"] = str(batch_file)
        return pilot_manifest

    def evaluate_pilot(
        self,
        domain_key: str,
        results: List[Dict[str, Any]],
    ) -> RolloutDecision:
        """
        Evaluate execution results of a pilot batch.
        Any failure freezes the domain. 100% success allows expansion.
        """
        state = self._load_state()
        if not results:
            return RolloutDecision(
                status="INSUFFICIENT_EVIDENCE",
                domain_or_lane=domain_key,
                message="No pilot results provided for evaluation",
                pilot_count=0,
                passed_count=0,
            )

        failed_items = []
        for r in results:
            app_id = r.get("app_id", "unknown")
            success = r.get("success", False)
            mutation_survived = r.get("mutation_survived", False)
            if not success or mutation_survived:
                failed_items.append(f"{app_id}: {r.get('message', 'Uncaught failure or mutation survived')}")

        if failed_items:
            # Freeze domain immediately
            state.setdefault("domains", {})[domain_key] = "FROZEN"
            state.setdefault("frozen_reasons", {})[domain_key] = "; ".join(failed_items)
            self._save_state(state)
            return RolloutDecision(
                status="DOMAIN_FROZEN",
                domain_or_lane=domain_key,
                message=f"Pilot failure detected! Freezing domain: {'; '.join(failed_items)}",
                pilot_count=len(results),
                passed_count=len(results) - len(failed_items),
                failed_items=failed_items,
                expansion_authorized=False,
                max_expansion_size=0,
            )

        # 100% Pilot Success: authorize expansion batch
        state.setdefault("domains", {})[domain_key] = "PILOT_PASSED"
        self._save_state(state)
        return RolloutDecision(
            status="PILOT_PASSED",
            domain_or_lane=domain_key,
            message=f"All {len(results)} pilot configurations passed with full candidate readiness.",
            pilot_count=len(results),
            passed_count=len(results),
            failed_items=[],
            expansion_authorized=True,
            max_expansion_size=10,  # Controlled bounded expansion
        )

    def unfreeze_domain(self, domain_key: str, resolution_reason: str) -> None:
        """Explicitly unfreeze a domain after defect remediation and review."""
        state = self._load_state()
        if state.get("domains", {}).get(domain_key) == "FROZEN":
            state["domains"][domain_key] = "UNFROZEN"
            state.setdefault("unfreeze_log", []).append({
                "domain_key": domain_key,
                "reason": resolution_reason,
                "unfrozen_at": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            })
            self._save_state(state)


def main() -> int:
    parser = argparse.ArgumentParser(description="WinkMicroOS ESP-IDF Batch Rollout Engine (V1-T2)")
    parser.add_argument("--create-pilot", action="store_true", help="Create a representative pilot slice")
    parser.add_argument("--lane", type=int, choices=[1, 2, 3, 4, 5, 6], help="Lane ID (1-6)")
    parser.add_argument("--domain", type=str, help="Domain keyword (e.g. storage, uart)")
    parser.add_argument("--pilot-size", type=int, default=3, help="Pilot slice size (default: 3)")
    parser.add_argument("--status", action="store_true", help="Show rollout state and frozen domains")
    parser.add_argument("--unfreeze", type=str, help="Domain key to unfreeze")
    parser.add_argument("--reason", type=str, default="Remediation completed", help="Reason for unfreezing")
    args = parser.parse_args()

    mgr = RolloutManager()

    if args.status:
        state = mgr._load_state()
        print(json.dumps(state, indent=2, ensure_ascii=False))
        return 0

    if args.unfreeze:
        mgr.unfreeze_domain(args.unfreeze, args.reason)
        print(f"[rollout] Domain '{args.unfreeze}' unfrozen with reason: {args.reason}")
        return 0

    if args.create_pilot:
        batch = mgr.create_pilot_batch(lane=args.lane, domain_keyword=args.domain, pilot_size=args.pilot_size)
        print("=" * 60)
        print(f"  PILOT BATCH CREATION: {batch.get('status', 'SCHEDULED')}")
        print("=" * 60)
        print(f"Domain Key: {batch.get('domain_key')}")
        print(f"Entries:    {len(batch.get('entries', []))}")
        for e in batch.get("entries", []):
            print(f"  - {e.get('id')} ({e.get('target_app_dir')})")
        if "manifest_path" in batch:
            print(f"Manifest:   {batch.get('manifest_path')}")
        return 0 if batch.get("status") != "BLOCKED" else 1

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
