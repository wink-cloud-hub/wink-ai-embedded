# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for impact_scope.py reverse transitive closure algorithm.
"""

import json
import yaml
from pathlib import Path
from impact_scope import compute_impact_closure

FIXTURES_DIR = Path(__file__).resolve().parent / "fixtures"


def load_fixture_data():
    with open(FIXTURES_DIR / "minimal_manifest_v2.json", "r", encoding="utf-8") as f:
        manifest = json.load(f)
    with open(FIXTURES_DIR / "minimal_catalog.yaml", "r", encoding="utf-8") as f:
        catalog = yaml.safe_load(f)
    return manifest, catalog


def test_impact_direct_capability():
    manifest, catalog = load_fixture_data()
    changed = ["wink-micro-os/targets/wasm/wink_sim_scheduler.c"]
    res = compute_impact_closure(changed, catalog, manifest)

    assert "cap.core.fiber_task" in res["direct_capabilities"]
    assert 1 in res["impact_entries"]   # Entry 1 requires cap.core.fiber_task
    assert 96 in res["impact_entries"]  # Entry 96 requires cap.core.fiber_task
    assert res["pr_inline"] is True


def test_impact_reverse_transitive_closure():
    manifest, catalog = load_fixture_data()
    # Add entry 64 with cap.proto.ws2812 to test transitive traversal
    manifest["entries"].append({
        "id": "esp.peripherals.rmt.led_strip",
        "display_id": 64,
        "required_capabilities": ["cap.proto.ws2812"],
        "executions": []
    })

    # Modify bottom-level pulse header
    # cap.pulse.tx_buffer -> depended by cap.proto.ws2812 -> required by entry 64
    changed = ["wink-micro-os/pal/include/hal/pal_pulse.h"]
    res = compute_impact_closure(changed, catalog, manifest)

    assert "cap.pulse.tx_buffer" in res["direct_capabilities"]
    assert "cap.proto.ws2812" in res["affected_capabilities"]
    assert 64 in res["impact_entries"]


def test_impact_non_code_catalog_diffusion():
    manifest, catalog = load_fixture_data()
    changed = ["wink-micro-app/vendor/esp_idfv61/capability-catalog.yaml"]
    res = compute_impact_closure(changed, catalog, manifest)

    # All capabilities in catalog should be marked affected
    assert len(res["direct_capabilities"]) == len(catalog["capabilities"])
    assert 1 in res["impact_entries"]


def test_impact_pr_inline_threshold():
    manifest, catalog = load_fixture_data()
    # Create 35 dummy entries all requiring cap.core.fiber_task
    manifest["entries"] = [
        {"id": f"esp.sample.{i}", "display_id": i, "required_capabilities": ["cap.core.fiber_task"], "executions": []}
        for i in range(1, 36)
    ]
    changed = ["wink-micro-os/targets/wasm/wink_sim_scheduler.c"]
    res = compute_impact_closure(changed, catalog, manifest, max_inline_entries=30)

    assert res["impact_count"] == 35
    assert res["pr_inline"] is False
