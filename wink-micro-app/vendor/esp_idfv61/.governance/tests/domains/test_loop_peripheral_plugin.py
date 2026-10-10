# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for Layer D Peripheral Plugin Governance Workflow
Tests branch D1.A vs D1.B, build pre-flight check, topology wiring,
master plan bidirectional sync, and RemediatorState lifecycle.
100% offline and deterministic.
"""
from __future__ import annotations

import json
import tempfile
from pathlib import Path
import pytest
import sys


from loop.agent import AgentSynthesizer
from loop.pipeline import LoopPipeline
from loop.remediator import Remediator, RemediatorState, InvestigationWorkspace


def test_detect_peripheral_type_from_failure():
    """Verify failure log analyzer correctly maps errors to peripheral types."""
    assert AgentSynthesizer.detect_peripheral_type_from_failure("I2C read WHO_AM_I failed for MPU9250") == "imu"
    assert AgentSynthesizer.detect_peripheral_type_from_failure("Failed reading PIR motion sensor pin 13") == "motion"
    assert AgentSynthesizer.detect_peripheral_type_from_failure("DS1307 RTC time get failed with -1") == "rtc"
    assert AgentSynthesizer.detect_peripheral_type_from_failure("HX711 load cell tare timeout") == "load_cell"
    assert AgentSynthesizer.detect_peripheral_type_from_failure("ILI9341 display init failed") == "tft"
    assert AgentSynthesizer.detect_peripheral_type_from_failure("FreeRTOS task watchdog trigger") is None


def test_existing_vs_new_peripheral_branching():
    """Verify branch D1.A (existing plugin) vs D1.B (new plugin) detection."""
    with tempfile.TemporaryDirectory() as tmpdir:
        ws_root = Path(tmpdir)
        agent = AgentSynthesizer(ws_root)

        # imu plugin exists with dist/manifest.json
        imu_dist = ws_root / "wink-plugin-peripherals" / "builtin" / "imu" / "1.0.0" / "dist"
        imu_dist.mkdir(parents=True, exist_ok=True)
        (imu_dist / "manifest.json").write_text("{}", encoding="utf-8")

        assert agent.is_existing_peripheral_plugin("imu") is True
        assert agent.is_existing_peripheral_plugin("non_existent_sensor") is False


def test_pipeline_compile_peripheral_plugin_preflight():
    """Verify compile_peripheral_plugin produces [BUILD_ENV_ERROR] on missing pkg/env."""
    with tempfile.TemporaryDirectory() as tmpdir:
        ws_root = Path(tmpdir)
        pipeline = LoopPipeline(ws_root)

        # Plugin directory does not exist
        ok, msg = pipeline.compile_peripheral_plugin("ghost_plugin")
        assert ok is False
        assert "not found" in msg

        # Plugin dir exists but missing package.json
        p_dir = ws_root / "wink-plugin-peripherals" / "builtin" / "broken_sensor"
        p_dir.mkdir(parents=True, exist_ok=True)
        ok, msg = pipeline.compile_peripheral_plugin("broken_sensor")
        assert ok is False
        assert "[BUILD_ENV_ERROR]" in msg


def test_pipeline_update_app_peripheral_topology():
    """Verify update_app_peripheral_topology safely injects into wink-app.json."""
    with tempfile.TemporaryDirectory() as tmpdir:
        ws_root = Path(tmpdir)
        pipeline = LoopPipeline(ws_root)

        app_dir = ws_root / "test_app"
        app_dir.mkdir(parents=True, exist_ok=True)
        wink_app_file = app_dir / "wink-app.json"
        wink_app_file.write_text(json.dumps({"name": "test", "peripherals": []}), encoding="utf-8")

        # Inject imu
        ok, msg = pipeline.update_app_peripheral_topology(
            app_dir=app_dir,
            peripheral_type="imu",
            variant="mpu9250_i2c",
            address=104,
        )
        assert ok is True

        data = json.loads(wink_app_file.read_text(encoding="utf-8"))
        assert len(data["peripherals"]) == 1
        assert data["peripherals"][0]["type"] == "imu"
        assert data["peripherals"][0]["variant"] == "mpu9250_i2c"
        assert data["peripherals"][0]["address"] == 104


def test_sync_master_execution_plan():
    """Verify bidirectional update of 00-master-execution-plan.md."""
    with tempfile.TemporaryDirectory() as tmpdir:
        ws_root = Path(tmpdir)
        agent = AgentSynthesizer(ws_root)
        remediator = Remediator(ws_root, agent)

        plan_dir = ws_root / "docs" / "implementation-plans" / "wokwi-dal-type-coverage-type"
        plan_dir.mkdir(parents=True, exist_ok=True)
        master_file = plan_dir / "00-master-execution-plan.md"

        sample_table = """
| # | Category | type | Wokwi | Desc | 进度状态 | 子计划文档路径 |
|---|---|---|---|---|---|---|
| 15 | sensor | `motion` | `pir-motion-sensor` | PIR | 🆕 Planned | `15-p2-sensor-motion-plan.md` |
"""
        master_file.write_text(sample_table, encoding="utf-8")

        # Sync motion status
        ok = remediator.sync_master_execution_plan("motion", "15-p2-sensor-motion-plan.md")
        assert ok is True

        updated_text = master_file.read_text(encoding="utf-8")
        assert "🟢 Implemented (Sim Plugin Delivered)" in updated_text
        assert "`15-p2-sensor-motion-plan.md`" in updated_text


def test_remediator_peripheral_states_sequence():
    """Verify peripheral-specific states can be recorded and persisted cleanly."""
    with tempfile.TemporaryDirectory() as tmpdir:
        ws_root = Path(tmpdir)
        ws = InvestigationWorkspace(ws_root, "esp.peripherals.imu_test")
        ws.initialize("failure log")

        ws.transition_to(RemediatorState.PERIPHERAL_D1_SSOT_MAPPED, peripheral_type="imu")
        ws.transition_to(RemediatorState.PERIPHERAL_D2_ADVERSARIAL_REVIEW)
        ws.transition_to(RemediatorState.PERIPHERAL_D3_PLUGIN_BUILDING)
        ws.transition_to(RemediatorState.PERIPHERAL_D4_TOPOLOGY_WIRED)
        ws.transition_to(RemediatorState.PERIPHERAL_SYNCED_MASTER_PLAN)

        state = ws.load_state()
        assert state["current_state"] == RemediatorState.PERIPHERAL_SYNCED_MASTER_PLAN.value
        states_recorded = [h["state"] for h in state["history"]]
        assert RemediatorState.PERIPHERAL_D1_SSOT_MAPPED.value in states_recorded
        assert RemediatorState.PERIPHERAL_D4_TOPOLOGY_WIRED.value in states_recorded
