# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g1_carrier_landing_integrity.py.
"""

from pathlib import Path
from gate_context import build_context
from rules import g1_carrier_landing_integrity


def test_carrier_landing_integrity_on_real_repo():
    """Verify that current workspace passes landing integrity with 0 errors."""
    ctx = build_context(mode="pr", allow_empty_diff=True)
    findings = g1_carrier_landing_integrity.run(ctx)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) == 0, f"Unexpected errors on real repo: {errors}"


def test_negative_missing_scenarios_when_assets_exist(tmp_path):
    ws = tmp_path
    vendor = ws / "wink-micro-app" / "vendor" / "esp_idfv61"
    app = vendor / "peripherals" / "test_app"
    app.mkdir(parents=True)
    (app / "wink-app.json").write_text("{}", encoding="utf-8")
    assets = app / "unisim-assets"
    assets.mkdir()
    (assets / "device-tree.json").write_text("{}", encoding="utf-8")
    (assets / "wink_simulator.js").write_text("console.log();", encoding="utf-8")
    (assets / "wink_simulator.wasm").write_bytes(b"\x00asm\x01\x00\x00\x00")

    context = {
        "workspace_root": str(ws),
        "manifest": {
            "entries": [
                {
                    "id": "esp.test",
                    "display_id": 999,
                    "target_app_dir": "peripherals/test_app",
                    "executions": [{"config_id": "wasm_sim_standard", "delivery_state": "planned"}],
                }
            ]
        },
    }

    findings = g1_carrier_landing_integrity.run(context)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) == 1
    assert "missing 'unisim-scenarios/'" in errors[0]["message"]


def test_negative_missing_assets_when_scenarios_exist(tmp_path):
    ws = tmp_path
    vendor = ws / "wink-micro-app" / "vendor" / "esp_idfv61"
    app = vendor / "peripherals" / "test_app"
    app.mkdir(parents=True)
    (app / "wink-app.json").write_text("{}", encoding="utf-8")
    scen = app / "unisim-scenarios"
    scen.mkdir()
    (scen / "test.scenario.json").write_text("{}", encoding="utf-8")

    context = {
        "workspace_root": str(ws),
        "manifest": {
            "entries": [
                {
                    "id": "esp.test",
                    "display_id": 999,
                    "target_app_dir": "peripherals/test_app",
                    "executions": [{"config_id": "wasm_sim_standard", "delivery_state": "planned"}],
                }
            ]
        },
    }

    findings = g1_carrier_landing_integrity.run(context)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) == 1
    assert "missing 'unisim-assets/'" in errors[0]["message"]


def test_negative_phase1_cannot_be_verified(tmp_path):
    ws = tmp_path
    vendor = ws / "wink-micro-app" / "vendor" / "esp_idfv61"
    app = vendor / "peripherals" / "test_app"
    app.mkdir(parents=True)
    (app / "wink-app.json").write_text("{}", encoding="utf-8")

    context = {
        "workspace_root": str(ws),
        "manifest": {
            "entries": [
                {
                    "id": "esp.test",
                    "display_id": 999,
                    "target_app_dir": "peripherals/test_app",
                    "executions": [
                        {
                            "config_id": "wasm_sim_standard",
                            "delivery_state": "verified",
                            "evidence": None,
                        }
                    ],
                }
            ]
        },
    }

    findings = g1_carrier_landing_integrity.run(context)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) == 1
    assert "Phase 1 apps cannot be verified" in errors[0]["message"]


def test_negative_ghost_scenario_path(tmp_path):
    ws = tmp_path
    vendor = ws / "wink-micro-app" / "vendor" / "esp_idfv61"
    app = vendor / "peripherals" / "test_app"
    app.mkdir(parents=True)
    (app / "wink-app.json").write_text("{}", encoding="utf-8")

    context = {
        "workspace_root": str(ws),
        "manifest": {
            "entries": [
                {
                    "id": "esp.test",
                    "display_id": 999,
                    "target_app_dir": "peripherals/test_app",
                    "executions": [
                        {
                            "config_id": "wasm_sim_standard",
                            "delivery_state": "planned",
                            "acceptance": {
                                "scenario_path": "peripherals/test_app/unisim-scenarios/ghost.scenario.json"
                            },
                        }
                    ],
                }
            ]
        },
    }

    findings = g1_carrier_landing_integrity.run(context)
    errors = [f for f in findings if f["severity"] == "error"]
    assert len(errors) == 1
    assert "file does not exist on disk" in errors[0]["message"]
