"""Tests for wink.py new-dal scaffolding (ADR-0046)."""
from __future__ import annotations

import argparse
from pathlib import Path

import pytest

from tools.cli.commands.new_dal import NewDalCommand
from tools.cli.context import AppContext


@pytest.fixture()
def tmp_sdk(tmp_path: Path) -> Path:
    # Minimal sdk tree; templates live in the real repo — command reads
    # templates relative to new_dal.py, not sdk_root.
    (tmp_path / "dal" / "include" / "sensor").mkdir(parents=True)
    (tmp_path / "dal" / "src" / "sensor").mkdir(parents=True)
    (tmp_path / "tools" / "codegen" / "drivers").mkdir(parents=True)
    return tmp_path


def test_new_dal_writes_three_files(tmp_sdk: Path):
    ctx = AppContext(
        sdk_root=tmp_sdk,
        workspace_root=tmp_sdk,
        app_dir=None,
        config={},
        env={},
    )
    args = argparse.Namespace(
        type="smoke_sensor",
        category="sensor",
        actuator=True,
        role="distance_sensor",
        pin_fields=["trig_pin", "echo_pin"],
        force=False,
    )
    rc = NewDalCommand().run(ctx, args)
    assert rc == 0
    hdr = tmp_sdk / "dal" / "include" / "sensor" / "dal_smoke_sensor.h"
    src = tmp_sdk / "dal" / "src" / "sensor" / "dal_smoke_sensor.c"
    plugin = tmp_sdk / "tools" / "codegen" / "drivers" / "smoke_sensor.py"
    assert hdr.is_file()
    assert src.is_file()
    assert plugin.is_file()
    hdr_txt = hdr.read_text(encoding="utf-8")
    assert "WINK_USE_SMOKE_SENSOR" in hdr_txt
    assert "dal_smoke_sensor_safe_off" in hdr_txt
    assert "WINK_UNAVAILABLE_MSG" in hdr_txt
    src_txt = src.read_text(encoding="utf-8")
    assert "pal_resource_claim" in src_txt
    plug_txt = plugin.read_text(encoding="utf-8")
    assert 'type = "smoke_sensor"' in plug_txt
    assert "DriverCategory.SENSOR" in plug_txt
    assert "is_actuator = True" in plug_txt


def test_new_dal_refuses_existing_without_force(tmp_sdk: Path):
    ctx = AppContext(
        sdk_root=tmp_sdk,
        workspace_root=tmp_sdk,
        app_dir=None,
        config={},
        env={},
    )
    args = argparse.Namespace(
        type="dup_dev",
        category="output",
        actuator=False,
        role="",
        pin_fields=[],
        force=False,
    )
    # category output dir may not exist yet — command mkdir
    assert NewDalCommand().run(ctx, args) == 0
    assert NewDalCommand().run(ctx, args) == 1
