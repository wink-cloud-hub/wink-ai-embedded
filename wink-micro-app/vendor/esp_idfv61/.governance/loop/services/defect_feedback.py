# SPDX-License-Identifier: Apache-2.0
"""
Defect Feedback & Rule Evolution Manager (V1-T3)
=================================================
Tracks regression defects and calculates reverse transitive impact on verified
configurations whenever underlying C drivers, gate rules, or recipes change.
Invalidates stale evidence packages and prevents automatic reuse.
"""

from __future__ import annotations

import argparse
import datetime
import json
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional

SERVICES_DIR = Path(__file__).resolve().parent
GOV_DIR = SERVICES_DIR.parent.parent
VENDOR_ROOT = GOV_DIR.parent
GATES_DIR = GOV_DIR / "gates"
if str(GATES_DIR) not in sys.path:
    sys.path.insert(0, str(GATES_DIR))

try:
    from impact_scope import compute_impact_closure
except ImportError:
    compute_impact_closure = None


DEFAULT_DEFECT_REGISTRY = {
    "schema_version": "1.0",
    "updated_at": "2026-10-08T12:00:00Z",
    "defects": [
        {
            "defect_id": "S-01",
            "category": "DRIVER_RUNTIME",
            "title": "GPTimer sim_timer_get_counter clock fallback removal",
            "affected_components": ["wink-micro-os/frameworks/esp_idf/src/drivers/esp_gptimer.c"],
            "regression_test_id": "AT-14",
            "status": "REVERIFIED",
            "resolution": "Counter returns exact running ticks; invalid handle returns 0."
        },
        {
            "defect_id": "S-02",
            "category": "DRIVER_RUNTIME",
            "title": "HWTimer oneshot dispatch race and re-arm closing bug",
            "affected_components": ["wink-micro-os/targets/wasm/pal_wasm_hwtimer.c"],
            "regression_test_id": "AT-07",
            "status": "REVERIFIED",
            "resolution": "Slot generation tracking and pre-dispatch oneshot state clearing."
        },
        {
            "defect_id": "S-03",
            "category": "DRIVER_RUNTIME",
            "title": "ADC dummy formulaic sample removal and negative error code propagation",
            "affected_components": ["wink-micro-os/targets/wasm/pal_wasm_ch3_adc.c"],
            "regression_test_id": "AT-04",
            "status": "REVERIFIED",
            "resolution": "Negative wink_status_t returned on invalid state; formula samples removed."
        },
        {
            "defect_id": "S-04",
            "category": "DRIVER_RUNTIME",
            "title": "SPI bus static EEPROM memory cross-instance pollution",
            "affected_components": ["wink-micro-os/frameworks/esp_idf/src/drivers/esp_spi.c"],
            "regression_test_id": "AT-07",
            "status": "REVERIFIED",
            "resolution": "Lowered EEPROM state into struct spi_device_t instance isolation."
        },
        {
            "defect_id": "S-05",
            "category": "DRIVER_STORAGE",
            "title": "SPIFFS dual-backend discrepancy and RAM VFS inode tracking",
            "affected_components": [
                "wink-micro-os/frameworks/esp_idf/src/core/esp_vfs_ram.c",
                "wink-micro-os/frameworks/esp_idf/src/core/esp_spiffs.c"
            ],
            "regression_test_id": "AT-11",
            "status": "REVERIFIED",
            "resolution": "esp_vfs_ram_get_used_bytes() returns real inode bytes to esp_spiffs_info()."
        },
        {
            "defect_id": "Q-01",
            "category": "SCENARIO_CAUSALITY",
            "title": "DAC fixture loopback self-proof elimination",
            "affected_components": ["wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_cosine_wave"],
            "regression_test_id": "AT-05",
            "status": "REVERIFIED",
            "resolution": "True DAC pin norm output assertion with verifiable negative kill."
        },
        {
            "defect_id": "Q-06",
            "category": "METADATA_INTEGRITY",
            "title": "Upstream metadata SHA-256 and source path normalization",
            "affected_components": [
                "wink-micro-app/vendor/esp_idfv61/peripherals/dac_dac_cosine_wave/wink-app.json",
                "wink-micro-app/vendor/esp_idfv61/storage/spiffs/wink-app.json"
            ],
            "regression_test_id": "AT-01",
            "status": "REVERIFIED",
            "resolution": "Completed 64-char upstream hash and mapped to existing main.c."
        }
    ]
}


class DefectFeedbackManager:
    """Manages defect tracking and impact propagation."""

    def __init__(self, vendor_root: Optional[Path] = None):
        self.vendor_root = vendor_root or VENDOR_ROOT
        self.data_dir = self.vendor_root / ".governance" / "data"
        self.data_dir.mkdir(parents=True, exist_ok=True)
        self.registry_file = self.data_dir / "defect_registry.json"
        self.catalog_file = self.vendor_root / ".governance" / "catalog" / "capability-catalog.yaml"
        self.manifest_file = self.data_dir / "checklist.data.json"
        self._ensure_registry()

    def _ensure_registry(self) -> None:
        if not self.registry_file.is_file():
            self.registry_file.write_text(
                json.dumps(DEFAULT_DEFECT_REGISTRY, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8"
            )

    def load_registry(self) -> Dict[str, Any]:
        try:
            return json.loads(self.registry_file.read_text(encoding="utf-8"))
        except Exception:
            return DEFAULT_DEFECT_REGISTRY

    def record_defect(
        self,
        defect_id: str,
        category: str,
        title: str,
        affected_components: List[str],
        regression_test_id: str,
        status: str = "TRACKING",
        resolution: str = "",
    ) -> Dict[str, Any]:
        reg = self.load_registry()
        defects = reg.get("defects", [])
        # Check if already exists
        for d in defects:
            if d.get("defect_id") == defect_id:
                d.update(
                    category=category,
                    title=title,
                    affected_components=affected_components,
                    regression_test_id=regression_test_id,
                    status=status,
                    resolution=resolution,
                )
                break
        else:
            defects.append({
                "defect_id": defect_id,
                "category": category,
                "title": title,
                "affected_components": affected_components,
                "regression_test_id": regression_test_id,
                "status": status,
                "resolution": resolution,
            })

        reg["defects"] = defects
        reg["updated_at"] = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        self.registry_file.write_text(json.dumps(reg, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        return {"status": "SUCCESS", "defect_id": defect_id}

    def evaluate_impact(self, changed_files: List[str]) -> Dict[str, Any]:
        """
        Calculates affected entries from changed files using reverse transitive closure.
        Returns impact report with stale evidence invalidation list.
        """
        import yaml
        if not compute_impact_closure:
            return {"error": "impact_scope module not available"}

        if not self.catalog_file.is_file():
            return {"error": f"Capability catalog not found at: {self.catalog_file}"}

        catalog = yaml.safe_load(self.catalog_file.read_text(encoding="utf-8"))
        manifest = json.loads(self.manifest_file.read_text(encoding="utf-8"))

        res = compute_impact_closure(changed_files, catalog, manifest)

        impact_entries = res.get("impact_entries", [])
        # Find which of the impacted entries are currently marked 'verified'
        verified_stale_entries = []
        entries_by_num = {}
        for idx, entry in enumerate(manifest.get("entries", []), 1):
            entries_by_num[idx] = entry
            display_id = entry.get("display_id")
            if display_id:
                entries_by_num[display_id] = entry

        for item in impact_entries:
            entry = entries_by_num.get(item)
            if entry:
                execs = entry.get("executions", [])
                if any(ex.get("delivery_state") == "verified" for ex in execs):
                    verified_stale_entries.append({
                        "id": entry.get("id"),
                        "display_id": entry.get("display_id", item),
                        "target_app_dir": entry.get("target_app_dir"),
                        "remediation_action": "NEEDS_REVERIFICATION",
                    })

        report = {
            "evaluated_at_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "changed_files": changed_files,
            "hit_global": res.get("hit_global", False),
            "affected_capabilities": list(res.get("affected_capabilities", set())),
            "total_impacted_entries": len(impact_entries),
            "verified_stale_entries": verified_stale_entries,
            "stale_count": len(verified_stale_entries),
            "directive": "INVALIDATE_STALE_EVIDENCE_PACKAGES" if verified_stale_entries else "NO_VERIFIED_IMPACT",
        }

        # Save impact report
        out_report = self.vendor_root / ".governance" / "runs" / "impact_feedback_report.json"
        out_report.parent.mkdir(parents=True, exist_ok=True)
        out_report.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        report["report_path"] = str(out_report)
        return report


def main() -> int:
    parser = argparse.ArgumentParser(description="WinkMicroOS ESP-IDF Defect Feedback & Rule Evolution (V1-T3)")
    parser.add_argument("--list-defects", action="store_true", help="List registered defects")
    parser.add_argument("--record-defect", action="store_true", help="Record a new defect")
    parser.add_argument("--id", type=str, help="Defect ID (e.g. DEF-2026-01)")
    parser.add_argument("--category", type=str, default="DRIVER_RUNTIME", help="Defect category")
    parser.add_argument("--title", type=str, help="Defect title")
    parser.add_argument("--components", type=str, help="Comma-separated affected components")
    parser.add_argument("--test-id", type=str, help="Associated regression test ID (e.g. AT-14)")
    parser.add_argument("--resolution", type=str, default="", help="Resolution description")
    parser.add_argument("--evaluate-impact", action="store_true", help="Calculate affected checklist items")
    parser.add_argument("--changed-files", type=str, help="Comma-separated list of changed files")
    args = parser.parse_args()

    mgr = DefectFeedbackManager()

    if args.list_defects:
        reg = mgr.load_registry()
        print("=" * 60)
        print("  REGISTERED DEFECTS & REGRESSION TRACE (V1-T3)")
        print("=" * 60)
        for d in reg.get("defects", []):
            print(f"[{d.get('status'):<10}] {d.get('defect_id'):<8} | Test: {d.get('regression_test_id'):<6} | {d.get('title')}")
            for comp in d.get("affected_components", []):
                print(f"               Comp: {comp}")
        return 0

    if args.record_defect:
        if not args.id or not args.title or not args.components:
            parser.error("--record-defect requires --id, --title, and --components")
        comps = [c.strip() for c in args.components.split(",") if c.strip()]
        res = mgr.record_defect(
            defect_id=args.id,
            category=args.category,
            title=args.title,
            affected_components=comps,
            regression_test_id=args.test_id or "UNKNOWN",
            resolution=args.resolution,
        )
        print(f"[defect] Recorded defect {args.id} successfully.")
        return 0

    if args.evaluate_impact:
        if not args.changed_files:
            parser.error("--evaluate-impact requires --changed-files")
        files = [f.strip() for f in args.changed_files.split(",") if f.strip()]
        res = mgr.evaluate_impact(files)
        print("=" * 60)
        print("  DEFECT IMPACT EVALUATION RESULT")
        print("=" * 60)
        print(f"Global Impact:     {res.get('hit_global')}")
        print(f"Impacted Entries:  {res.get('total_impacted_entries')}")
        print(f"Stale Verified:    {res.get('stale_count')}")
        print(f"Directive:         {res.get('directive')}")
        for stale in res.get("verified_stale_entries", []):
            print(f"  [STALE] #{stale.get('display_id')} {stale.get('id')} -> {stale.get('remediation_action')}")
        print(f"Report:            {res.get('report_path')}")
        return 0

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
