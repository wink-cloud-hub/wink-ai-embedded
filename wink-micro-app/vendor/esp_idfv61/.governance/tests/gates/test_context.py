# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for gate_context.py.
"""

import os
import pytest
from datetime import timezone
from pathlib import Path

from gates.gate_context import (
    find_workspace_root,
    normalize_posix_path,
    load_changed_files,
    build_context,
)

FIXTURES_DIR = Path(__file__).resolve().parent / "fixtures"


def test_posix_path_normalization():
    assert normalize_posix_path("foo\\bar\\baz.c") == "foo/bar/baz.c"
    assert normalize_posix_path("./foo/bar.h") == "foo/bar.h"
    assert normalize_posix_path("foo/../bar/baz.c") == "bar/baz.c"


def test_workspace_root_discovery():
    root = find_workspace_root()
    assert (root / "wink-micro-os").exists() or (root / ".git").exists()


def test_load_changed_files_from_file(tmp_path):
    diff_file = tmp_path / "changed.txt"
    diff_file.write_text(
        "wink-micro-os/pal/pal_gpio.c\n"
        "# comment line\n"
        "\n"
        "wink-micro-app\\vendor\\esp_idfv61\\blink_gpio\\main.c\n",
        encoding="utf-8",
    )
    loaded = load_changed_files(diff_file, tmp_path, mode="pr")
    assert len(loaded) == 2
    assert loaded[0] == "wink-micro-os/pal/pal_gpio.c"
    assert loaded[1] == "wink-micro-app/vendor/esp_idfv61/blink_gpio/main.c"


def test_empty_diff_in_ci_raises_error(monkeypatch, tmp_path):
    monkeypatch.setenv("CI", "true")
    diff_file = tmp_path / "empty.txt"
    diff_file.write_text("", encoding="utf-8")

    with pytest.raises(ValueError, match="0 files in PR mode"):
        load_changed_files(diff_file, tmp_path, mode="pr", allow_empty_diff=False)

    # With allow_empty_diff, should succeed
    res = load_changed_files(diff_file, tmp_path, mode="pr", allow_empty_diff=True)
    assert res == []


def test_build_context_with_fixtures():
    ctx = build_context(
        manifest_path=FIXTURES_DIR / "minimal_manifest_v2.json",
        catalog_path=FIXTURES_DIR / "minimal_catalog.yaml",
        quarantine_path=FIXTURES_DIR / "quarantine_sample.yaml",
        mode="pr",
        allow_empty_diff=True,
    )
    assert ctx["spec_version"] == "2.0.0"
    assert ctx["mode"] == "pr"
    assert ctx["now_utc"].tzinfo == timezone.utc
    assert len(ctx["manifest"]["entries"]) == 3
    assert "cap.proto.ws2812" in ctx["catalog"]["capabilities"]
    assert len(ctx["quarantine"]["quarantined_entries"]) == 2
