# SPDX-License-Identifier: Apache-2.0
"""
g1_carrier_landing_integrity.py
================================
Gate 1 Rule: Verifies the structural integrity, phase state, and artifact pairing
of landed carrier micro-apps under wink-micro-app/vendor/esp_idfv61.

Rules enforced:
1. Pairing Invariant:
   - If an app has unisim-scenarios/, it MUST have unisim-assets/.
   - If an app has unisim-assets/, it MUST have unisim-scenarios/.
   (Simulation assets and test scenarios are strictly paired).
2. Phase 2 (Simulation Ready / In-Progress / Verified) Artifact Completeness:
   - unisim-assets/ MUST contain device-tree.json, wink_simulator.js, and wink_simulator.wasm (all non-empty).
   - unisim-scenarios/ MUST contain at least one non-empty *.scenario.json file.
3. Phase 1 (Mirror-Only) Honesty:
   - If an app has neither unisim-assets/ nor unisim-scenarios/, all execution configs
     in checklist.data.json MUST remain delivery_state='planned' (or 'building') and evidence=null.
   - It cannot be verified without simulation assets and scenarios.
4. Bound Scenario Existence:
   - For any landed app, if an execution config declares a scenario_path, that file MUST exist on disk.
"""

from pathlib import Path
from typing import List, Dict, Any, Optional
from gate_context import is_candidate_artifact

RULE_ID = "g1.carrier_landing_integrity"
REQUIRED_ASSET_FILES = ["device-tree.json", "wink_simulator.js", "wink_simulator.wasm"]


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    ws_root = Path(context.get("workspace_root", "."))
    vendor_root = ws_root / "wink-micro-app" / "vendor" / "esp_idfv61"

    if not vendor_root.is_dir():
        return findings

    manifest = context.get("manifest", {})
    entries = manifest.get("entries", [])
    entries_by_target: Dict[str, dict] = {}
    for e in entries:
        t = e.get("target_app_dir")
        if t:
            if (vendor_root / t).resolve().is_relative_to((vendor_root / ".governance" / "runs").resolve()):
                findings.append({
                    "rule_id": RULE_ID, "severity": "error",
                    "entry_id": e.get("id"), "display_id": e.get("display_id"),
                    "config_id": None, "file_path": str(vendor_root / t),
                    "message": f"target_app_dir '{t}' uses the reserved candidate namespace; it cannot be a formal carrier.",
                })
                continue
            entries_by_target[t] = e

    # Discover all landed apps (directories containing wink-app.json)
    for manifest_file in sorted(vendor_root.rglob("wink-app.json")):
        if is_candidate_artifact(manifest_file, vendor_root):
            continue
        app_dir = manifest_file.parent
        target_app_dir = app_dir.relative_to(vendor_root).as_posix()

        entry = entries_by_target.get(target_app_dir)
        entry_id = entry.get("id") if entry else None
        display_id = entry.get("display_id") if entry else None

        if not entry:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": str(manifest_file),
                "message": (
                    f"Landed carrier app '{target_app_dir}' is not registered in checklist.data.json "
                    f"under target_app_dir."
                ),
            })
            continue

        assets_dir = app_dir / "unisim-assets"
        scenarios_dir = app_dir / "unisim-scenarios"

        has_assets = assets_dir.is_dir()
        has_scenarios = scenarios_dir.is_dir()

        # Rule 1: Pairing Invariant
        if has_assets and not has_scenarios:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": entry_id,
                "display_id": display_id,
                "config_id": None,
                "file_path": str(assets_dir),
                "message": (
                    f"Landed carrier app '{target_app_dir}' has 'unisim-assets/' but is missing 'unisim-scenarios/'. "
                    f"Simulation assets must be paired with deterministic scenario tests."
                ),
            })
        elif has_scenarios and not has_assets:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": entry_id,
                "display_id": display_id,
                "config_id": None,
                "file_path": str(scenarios_dir),
                "message": (
                    f"Landed carrier app '{target_app_dir}' has 'unisim-scenarios/' but is missing 'unisim-assets/'. "
                    f"Scenario tests require compiled simulation assets (device-tree.json, wink_simulator.js, wink_simulator.wasm)."
                ),
            })

        # Rule 2: Phase 2 Artifact Completeness
        if has_assets and has_scenarios:
            for req_file in REQUIRED_ASSET_FILES:
                target_f = assets_dir / req_file
                if not target_f.is_file():
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": None,
                        "file_path": str(target_f),
                        "message": (
                            f"Landed carrier app '{target_app_dir}' unisim-assets/ is missing required artifact: '{req_file}'."
                        ),
                    })
                elif target_f.stat().st_size == 0:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": None,
                        "file_path": str(target_f),
                        "message": (
                            f"Landed carrier app '{target_app_dir}' unisim-assets/ artifact '{req_file}' is 0 bytes."
                        ),
                    })

            scenario_files = list(scenarios_dir.glob("*.scenario.json"))
            if not scenario_files:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": entry_id,
                    "display_id": display_id,
                    "config_id": None,
                    "file_path": str(scenarios_dir),
                    "message": (
                        f"Landed carrier app '{target_app_dir}' unisim-scenarios/ does not contain any '*.scenario.json' test files."
                    ),
                })
            else:
                for sf in scenario_files:
                    if sf.stat().st_size == 0:
                        findings.append({
                            "rule_id": RULE_ID,
                            "severity": "error",
                            "entry_id": entry_id,
                            "display_id": display_id,
                            "config_id": None,
                            "file_path": str(sf),
                            "message": (
                                f"Scenario test file '{sf.name}' in '{target_app_dir}' is 0 bytes."
                            ),
                        })

            # Check scenario_path consistency if declared
            for ex in entry.get("executions", []):
                cid = ex.get("config_id")
                sc_path_declared = ex.get("acceptance", {}).get("scenario_path")
                if sc_path_declared:
                    cand1 = vendor_root / sc_path_declared
                    cand2 = ws_root / sc_path_declared
                    if not cand1.is_file() and not cand2.is_file():
                        findings.append({
                            "rule_id": RULE_ID,
                            "severity": "error",
                            "entry_id": entry_id,
                            "display_id": display_id,
                            "config_id": cid,
                            "file_path": sc_path_declared,
                            "message": (
                                f"Landed carrier app '{target_app_dir}' config '{cid}' declared scenario_path "
                                f"'{sc_path_declared}', but file does not exist on disk."
                            ),
                        })

        # Rule 3: Phase 1 (Mirror-Only) Honesty
        if not has_assets and not has_scenarios:
            for ex in entry.get("executions", []):
                cid = ex.get("config_id")
                deliv = ex.get("delivery_state")
                ev = ex.get("evidence")
                sc_path_declared = ex.get("acceptance", {}).get("scenario_path")

                if deliv == "verified":
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": (
                            f"Landed carrier app '{target_app_dir}' is in Phase 1 (Mirror-Only, no unisim assets/scenarios), "
                            f"but execution config '{cid}' has delivery_state='verified'. Phase 1 apps cannot be verified."
                        ),
                    })
                if ev is not None:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": entry_id,
                        "display_id": display_id,
                        "config_id": cid,
                        "file_path": None,
                        "message": (
                            f"Landed carrier app '{target_app_dir}' is in Phase 1 (no unisim assets/scenarios), "
                            f"but execution config '{cid}' has non-null evidence attached."
                        ),
                    })
                if sc_path_declared:
                    cand1 = vendor_root / sc_path_declared
                    cand2 = ws_root / sc_path_declared
                    if not cand1.is_file() and not cand2.is_file():
                        findings.append({
                            "rule_id": RULE_ID,
                            "severity": "error",
                            "entry_id": entry_id,
                            "display_id": display_id,
                            "config_id": cid,
                            "file_path": sc_path_declared,
                            "message": (
                                f"Landed Phase 1 carrier app '{target_app_dir}' config '{cid}' declared scenario_path "
                                f"'{sc_path_declared}', but file does not exist on disk."
                            ),
                        })

    return findings
