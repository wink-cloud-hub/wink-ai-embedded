"""Tests for tools.json config load/save + schema versioning."""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.toolchain.config import (  # noqa: E402
    ToolsConfig,
    UnsupportedToolsJsonVersionError,
    load_tools_config,
    save_user_path,
    save_workspace_path,
)


class TestToolsConfig(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp_root = Path(self._tmp.name)
        self.fake_home = self.tmp_root / "home"
        self.fake_home.mkdir()
        self.workspace = self.tmp_root / "workspace"
        self.workspace.mkdir()

        # Patch Path.home() so save_user_path / load_tools_config
        # never touch the real user profile.
        self._home_patch = mock.patch(
            "tools.toolchain.config.Path.home",
            return_value=self.fake_home,
        )
        self._home_patch.start()

    def tearDown(self):
        self._home_patch.stop()
        self._tmp.cleanup()

    def test_missing_files_returns_empty_config(self):
        cfg = load_tools_config(self.workspace)
        self.assertEqual(cfg.version, 1)
        self.assertIsNone(cfg.tools_home)
        self.assertEqual(cfg.paths, {})

    def test_missing_files_and_no_workspace(self):
        cfg = load_tools_config(None)
        self.assertEqual(cfg.version, 1)
        self.assertIsNone(cfg.tools_home)
        self.assertEqual(cfg.paths, {})

    def test_unsupported_version_raises(self):
        user_file = self.fake_home / ".wink" / "tools.json"
        user_file.parent.mkdir(parents=True, exist_ok=True)
        user_file.write_text(json.dumps({"version": 2, "paths": {}}), encoding="utf-8")
        with self.assertRaises(UnsupportedToolsJsonVersionError):
            load_tools_config(self.workspace)

    def test_unsupported_version_in_workspace_raises(self):
        ws_file = self.workspace / ".wink" / "tools.json"
        ws_file.parent.mkdir(parents=True, exist_ok=True)
        ws_file.write_text(json.dumps({"version": 2}), encoding="utf-8")
        with self.assertRaises(UnsupportedToolsJsonVersionError):
            load_tools_config(self.workspace)

    def test_save_user_path_creates_parent_dirs_and_round_trip(self):
        wink_dir = self.fake_home / ".wink"
        self.assertFalse(wink_dir.exists())

        path = save_user_path("gcc", "C:/tools/gcc/bin/gcc.exe")
        self.assertTrue(path.exists())
        self.assertEqual(path, self.fake_home / ".wink" / "tools.json")

        # File is valid JSON with version=1
        data = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(data["version"], 1)
        self.assertEqual(data["paths"]["gcc"], "C:/tools/gcc/bin/gcc.exe")

        # Round-trip via loader
        cfg = load_tools_config(None)
        self.assertEqual(cfg.paths.get("gcc"), "C:/tools/gcc/bin/gcc.exe")

    def test_save_user_path_merges_with_existing(self):
        save_user_path("gcc", "C:/gcc")
        save_user_path("emsdk", "D:/emsdk")
        cfg = load_tools_config(None)
        self.assertEqual(cfg.paths.get("gcc"), "C:/gcc")
        self.assertEqual(cfg.paths.get("emsdk"), "D:/emsdk")

    def test_workspace_overrides_user_same_key(self):
        save_user_path("gcc", "C:/user/gcc")
        save_workspace_path(self.workspace, "gcc", "C:/ws/gcc")
        save_workspace_path(self.workspace, "cmake", "C:/ws/cmake")

        cfg = load_tools_config(self.workspace)
        # workspace overrides user for gcc
        self.assertEqual(cfg.paths.get("gcc"), "C:/ws/gcc")
        # workspace-only key present
        self.assertEqual(cfg.paths.get("cmake"), "C:/ws/cmake")

    def test_workspace_only_keeps_user_unrelated_keys(self):
        save_user_path("gcc", "C:/user/gcc")
        save_workspace_path(self.workspace, "cmake", "C:/ws/cmake")

        cfg = load_tools_config(self.workspace)
        # user key preserved
        self.assertEqual(cfg.paths.get("gcc"), "C:/user/gcc")
        # workspace key added
        self.assertEqual(cfg.paths.get("cmake"), "C:/ws/cmake")

    def test_save_workspace_path_creates_parent_dirs(self):
        wink_dir = self.workspace / ".wink"
        self.assertFalse(wink_dir.exists())
        path = save_workspace_path(self.workspace, "gcc", "C:/gcc")
        self.assertTrue(path.exists())
        self.assertEqual(path, self.workspace / ".wink" / "tools.json")
        data = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(data["version"], 1)

    def test_tools_home_is_loaded_as_path(self):
        user_file = self.fake_home / ".wink" / "tools.json"
        user_file.parent.mkdir(parents=True, exist_ok=True)
        user_file.write_text(
            json.dumps({"version": 1, "tools_home": "C:/tools", "paths": {}}),
            encoding="utf-8",
        )
        cfg = load_tools_config(None)
        self.assertEqual(cfg.tools_home, Path("C:/tools"))

    def test_workspace_tools_home_overrides_user(self):
        user_file = self.fake_home / ".wink" / "tools.json"
        user_file.parent.mkdir(parents=True, exist_ok=True)
        user_file.write_text(
            json.dumps({"version": 1, "tools_home": "C:/user_tools"}),
            encoding="utf-8",
        )
        ws_file = self.workspace / ".wink" / "tools.json"
        ws_file.parent.mkdir(parents=True, exist_ok=True)
        ws_file.write_text(
            json.dumps({"version": 1, "tools_home": "C:/ws_tools"}),
            encoding="utf-8",
        )
        cfg = load_tools_config(self.workspace)
        self.assertEqual(cfg.tools_home, Path("C:/ws_tools"))

    def test_invalid_json_propagates(self):
        user_file = self.fake_home / ".wink" / "tools.json"
        user_file.parent.mkdir(parents=True, exist_ok=True)
        user_file.write_text("{not valid json", encoding="utf-8")
        with self.assertRaises(json.JSONDecodeError):
            load_tools_config(None)


if __name__ == "__main__":
    unittest.main()
