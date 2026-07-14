"""Tests for resolve priority (env > workspace > user)."""
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

from tools.toolchain.resolve import (  # noqa: E402
    CAP_ENV_VARS,
    ResolveContext,
    candidate_paths,
    resolve_tools_home,
)


def _make_ctx(
    environ: dict[str, str] | None = None,
    user_paths: dict[str, str] | None = None,
    workspace_paths: dict[str, str] | None = None,
    tools_home: Path | None = None,
    workspace_root: Path | None = None,
    os_name: str = "nt",
) -> ResolveContext:
    return ResolveContext(
        environ=dict(environ or {}),
        user_paths=dict(user_paths or {}),
        workspace_paths=dict(workspace_paths or {}),
        tools_home=tools_home,
        workspace_root=workspace_root,
        os_name=os_name,
    )


class TestCandidatePaths(unittest.TestCase):
    def test_env_beats_workspace_beats_user(self):
        ctx = _make_ctx(
            environ={"EMSDK": "C:/env/emsdk"},
            workspace_paths={"emsdk": "C:/ws/emsdk"},
            user_paths={"emsdk": "C:/user/emsdk"},
        )
        result = candidate_paths("emsdk", ctx)
        self.assertEqual(len(result), 3)
        self.assertEqual(result[0], ("env:EMSDK", Path("C:/env/emsdk")))
        self.assertEqual(result[1], ("config:workspace", Path("C:/ws/emsdk")))
        self.assertEqual(result[2], ("config:user", Path("C:/user/emsdk")))

    def test_workspace_beats_user_when_no_env(self):
        ctx = _make_ctx(
            workspace_paths={"gcc": "C:/ws/gcc"},
            user_paths={"gcc": "C:/user/gcc"},
        )
        result = candidate_paths("gcc", ctx)
        self.assertEqual(len(result), 2)
        self.assertEqual(result[0], ("config:workspace", Path("C:/ws/gcc")))
        self.assertEqual(result[1], ("config:user", Path("C:/user/gcc")))

    def test_env_var_ignored_when_empty(self):
        ctx = _make_ctx(
            environ={"EMSDK": ""},
            user_paths={"emsdk": "C:/user/emsdk"},
        )
        result = candidate_paths("emsdk", ctx)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0], ("config:user", Path("C:/user/emsdk")))

    def test_cap_without_env_var_no_env_candidate(self):
        # cmake has no direct env override
        ctx = _make_ctx(
            environ={"WINK_GCC_PREFIX": "C:/env/gcc"},  # unrelated
            workspace_paths={"cmake": "C:/ws/cmake"},
        )
        self.assertNotIn("cmake", CAP_ENV_VARS)
        result = candidate_paths("cmake", ctx)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0], ("config:workspace", Path("C:/ws/cmake")))

    def test_no_config_no_env_returns_empty(self):
        ctx = _make_ctx()
        # cmake w/ nothing configured
        self.assertEqual(candidate_paths("cmake", ctx), [])
        # gcc (has env var) but env not set and no config
        self.assertEqual(candidate_paths("gcc", ctx), [])

    def test_cap_env_vars_mapping(self):
        self.assertEqual(CAP_ENV_VARS["gcc"], "WINK_GCC_PREFIX")
        self.assertEqual(CAP_ENV_VARS["python"], "WINK_PYTHON")
        self.assertEqual(CAP_ENV_VARS["emsdk"], "EMSDK")
        self.assertEqual(CAP_ENV_VARS["idf"], "IDF_PATH")
        # caps without direct env override
        self.assertNotIn("cmake", CAP_ENV_VARS)
        self.assertNotIn("make", CAP_ENV_VARS)
        self.assertNotIn("node", CAP_ENV_VARS)
        self.assertNotIn("powershell", CAP_ENV_VARS)
        self.assertNotIn("jinja2", CAP_ENV_VARS)


class TestResolveToolsHome(unittest.TestCase):
    def test_env_wins_over_config(self):
        ctx = _make_ctx(
            environ={"WINK_TOOLS_HOME": "C:/env/tools"},
            tools_home=Path("C:/cfg/tools"),
        )
        self.assertEqual(resolve_tools_home(ctx), Path("C:/env/tools"))

    def test_config_used_when_no_env(self):
        ctx = _make_ctx(tools_home=Path("C:/cfg/tools"))
        self.assertEqual(resolve_tools_home(ctx), Path("C:/cfg/tools"))

    def test_none_when_nothing_set(self):
        ctx = _make_ctx()
        self.assertIsNone(resolve_tools_home(ctx))

    def test_empty_env_falls_through_to_config(self):
        ctx = _make_ctx(
            environ={"WINK_TOOLS_HOME": ""},
            tools_home=Path("C:/cfg/tools"),
        )
        self.assertEqual(resolve_tools_home(ctx), Path("C:/cfg/tools"))


class TestSnapshot(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp_root = Path(self._tmp.name)
        self.fake_home = self.tmp_root / "home"
        self.fake_home.mkdir()
        self.workspace = self.tmp_root / "workspace"
        self.workspace.mkdir()

        self._home_patch = mock.patch(
            "tools.toolchain.config.Path.home",
            return_value=self.fake_home,
        )
        self._home_patch.start()

    def tearDown(self):
        self._home_patch.stop()
        self._tmp.cleanup()

    def test_snapshot_reads_env_and_configs(self):
        # Write user + workspace configs
        user_file = self.fake_home / ".wink" / "tools.json"
        user_file.parent.mkdir(parents=True, exist_ok=True)
        user_file.write_text(
            json.dumps(
                {"version": 1, "paths": {"gcc": "C:/user/gcc"}, "tools_home": "C:/user_tools"}
            ),
            encoding="utf-8",
        )
        ws_file = self.workspace / ".wink" / "tools.json"
        ws_file.parent.mkdir(parents=True, exist_ok=True)
        ws_file.write_text(
            json.dumps(
                {"version": 1, "paths": {"emsdk": "C:/ws/emsdk"}, "tools_home": "C:/ws_tools"}
            ),
            encoding="utf-8",
        )

        with mock.patch.dict(
            "os.environ",
            {"EMSDK": "C:/env/emsdk", "WINK_GCC_PREFIX": "C:/env/gcc"},
            clear=False,
        ):
            ctx = ResolveContext.snapshot(self.workspace)

        # env captured
        self.assertEqual(ctx.environ.get("EMSDK"), "C:/env/emsdk")
        # user + workspace paths captured SEPARATELY
        self.assertEqual(ctx.user_paths.get("gcc"), "C:/user/gcc")
        self.assertEqual(ctx.workspace_paths.get("emsdk"), "C:/ws/emsdk")
        # workspace tools_home wins
        self.assertEqual(ctx.tools_home, Path("C:/ws_tools"))
        # workspace_root recorded
        self.assertEqual(ctx.workspace_root, self.workspace)
        # os_name from os.name
        import os

        self.assertEqual(ctx.os_name, os.name)

    def test_snapshot_priority_end_to_end(self):
        # user says gcc -> C:/user/gcc
        user_file = self.fake_home / ".wink" / "tools.json"
        user_file.parent.mkdir(parents=True, exist_ok=True)
        user_file.write_text(
            json.dumps({"version": 1, "paths": {"gcc": "C:/user/gcc"}}), encoding="utf-8"
        )
        # workspace also says gcc -> C:/ws/gcc
        ws_file = self.workspace / ".wink" / "tools.json"
        ws_file.parent.mkdir(parents=True, exist_ok=True)
        ws_file.write_text(
            json.dumps({"version": 1, "paths": {"gcc": "C:/ws/gcc"}}), encoding="utf-8"
        )
        with mock.patch.dict(
            "os.environ", {"WINK_GCC_PREFIX": "C:/env/gcc"}, clear=False
        ):
            ctx = ResolveContext.snapshot(self.workspace)

        result = candidate_paths("gcc", ctx)
        # env, workspace, user in that order
        self.assertEqual(
            [tag for tag, _ in result],
            ["env:WINK_GCC_PREFIX", "config:workspace", "config:user"],
        )
        self.assertEqual(result[0][1], Path("C:/env/gcc"))
        self.assertEqual(result[1][1], Path("C:/ws/gcc"))
        self.assertEqual(result[2][1], Path("C:/user/gcc"))


if __name__ == "__main__":
    unittest.main()
