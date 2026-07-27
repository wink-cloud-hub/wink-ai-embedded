"""Tests for tools/codegen/list_drivers.py (ADR-0046)."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

_CODEGEN = Path(__file__).resolve().parents[1]
_SDK = _CODEGEN.parent.parent
_SCRIPT = _CODEGEN / "list_drivers.py"


def _run(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(_SCRIPT), *args],
        capture_output=True,
        text=True,
        cwd=str(_SDK),
        check=False,
    )


def test_cmake_source_lists_nine_drivers_and_font_sources():
    proc = _run("--cmake", "--mode=source")
    assert proc.returncode == 0, proc.stderr
    out = proc.stdout
    assert "set(WINK_KNOWN_DRIVERS" in out
    for tok in (
        "LED",
        "BUTTON",
        "SERVO",
        "SSD1306",
        "ULTRASONIC",
        "GPS",
        "EEPROM",
        "MOTOR",
        "ENCODER",
    ):
        assert f"WINK_USE_{tok}" in out
        assert f"WINK_DAL_{tok}_REL_SRC" in out
    assert "dal_ssd1306_font_5x7" in out
    assert "target_sources(${WINK_DAL_TARGET}" in out
    assert "WINK_SSD1306_FONT_MINIMAL" in out
    assert "function(wink_dal_apply_extra_cmake)" in out
    # Must not call entry-specific enable helpers.
    assert "_wink_dal_enable" not in out


def test_cmake_defs_omits_font_target_sources():
    proc = _run("--cmake", "--mode=defs")
    assert proc.returncode == 0, proc.stderr
    out = proc.stdout
    assert "WINK_SSD1306_FONT" in out
    assert "WINK_SSD1306_FONT_MINIMAL" in out or "WINK_SSD1306_FONT_ASCII_UPPER" in out
    assert "dal_ssd1306_font_5x7" not in out
    assert "function(wink_dal_apply_extra_cmake)" in out
    assert "extra_cmake_sources from drivers/ssd1306.py" not in out


def test_json_inventory():
    proc = _run("--json")
    assert proc.returncode == 0, proc.stderr
    data = json.loads(proc.stdout)
    types = {d["type"] for d in data}
    assert types == {
        "led",
        "button",
        "servo",
        "ssd1306",
        "ultrasonic",
        "gps",
        "eeprom",
        "motor",
        "encoder",
    }
    ssd = next(d for d in data if d["type"] == "ssd1306")
    assert ssd["has_extra_cmake_sources"] is True
    assert ssd["has_extra_cmake_defs"] is True
    assert ssd["category"] == "display"


def test_check_ok_on_current_tree():
    proc = _run("--check", f"--sdk-root={_SDK}")
    assert proc.returncode == 0, proc.stderr + proc.stdout
