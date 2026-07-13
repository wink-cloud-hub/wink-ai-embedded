"""Tests for :mod:`tools.esp32.activate`.

All external tool invocations are mocked; these tests must pass on any
machine (Windows or not) with no ESP-IDF installed. They cover:

1. Hot-start: already-activated shell → returns env as-is.
2. Shim banner (``ESP-IDF Tools Installer v1.0.3``) is rejected.
3. Missing ``idf.py`` on PATH → rejected.
4. Wrong major version (v5.x) → rejected.
5. EIM profile is sourced when the shell is not ready.
6. First EIM profile fails → second is tried.
7. Fallback: ``IDF_PATH/export.ps1`` is sourced when no EIM profile.
8. Nothing works → :class:`IdfActivationError` with preinstall.md §3 hint.
9. ``activate()`` does not mutate the caller's dict / ``os.environ``.
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.esp32 import activate as act_mod  # noqa: E402
from tools.esp32.activate import (  # noqa: E402
    IdfActivationError,
    IdfEnv,
    activate,
    is_shell_ready,
)


def _fake_completed(stdout: str = "", stderr: str = "", returncode: int = 0):
    cp = subprocess.CompletedProcess(args=[], returncode=returncode)
    cp.stdout = stdout
    cp.stderr = stderr
    return cp


def _make_fake_idf_root(tmp: Path) -> Path:
    """Create a directory with ``tools/idf.py`` inside so _validate_idf_path passes."""
    root = tmp / "esp-idf-v6.0.1"
    (root / "tools").mkdir(parents=True)
    (root / "tools" / "idf.py").write_text("# fake\n", encoding="utf-8")
    return root


class TestIsShellReady(unittest.TestCase):
    def test_already_ready_returns_version(self):
        with tempfile.TemporaryDirectory() as td:
            root = _make_fake_idf_root(Path(td))
            environ = {
                "IDF_PATH": str(root),
                "PATH": str(root / "tools"),
            }

            def fake_which(name, path=None):
                if "idf.py" in name:
                    return str(root / "tools" / "idf.py")
                return None

            def fake_run(cmd, *a, **kw):
                return _fake_completed(stdout="ESP-IDF v6.0.1-dirty\n", returncode=0)

            with mock.patch("shutil.which", side_effect=fake_which):
                with mock.patch("subprocess.run", side_effect=fake_run):
                    ok, info = is_shell_ready(environ)
        self.assertTrue(ok, info)
        self.assertIn("6.0.1", info)

    def test_shim_banner_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root = _make_fake_idf_root(Path(td))
            environ = {"IDF_PATH": str(root), "PATH": str(root / "tools")}

            with mock.patch("shutil.which", return_value=str(root / "tools" / "idf.py")):
                with mock.patch(
                    "subprocess.run",
                    return_value=_fake_completed(
                        stdout="ESP-IDF Tools Installer v1.0.3\n",
                        returncode=0,
                    ),
                ):
                    ok, reason = is_shell_ready(environ)
        self.assertFalse(ok)
        self.assertIn("shim", (reason or "").lower())

    def test_missing_idf_rejected(self):
        with mock.patch("shutil.which", return_value=None):
            ok, reason = is_shell_ready({"PATH": ""})
        self.assertFalse(ok)
        self.assertIsNotNone(reason)

    def test_wrong_major_version_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            root = _make_fake_idf_root(Path(td))
            environ = {"IDF_PATH": str(root), "PATH": str(root / "tools")}

            with mock.patch("shutil.which", return_value=str(root / "tools" / "idf.py")):
                with mock.patch(
                    "subprocess.run",
                    return_value=_fake_completed(
                        stdout="ESP-IDF v5.1.2\n",
                        returncode=0,
                    ),
                ):
                    ok, reason = is_shell_ready(environ)
        self.assertFalse(ok)
        self.assertIn("5", reason or "")

    def test_idf_path_unset_rejected(self):
        # idf.py on PATH but no IDF_PATH — must fail.
        with mock.patch("shutil.which", return_value="/tmp/idf.py"):
            with mock.patch(
                "subprocess.run",
                return_value=_fake_completed(stdout="ESP-IDF v6.0.1\n", returncode=0),
            ):
                ok, reason = is_shell_ready({})
        self.assertFalse(ok)
        self.assertIn("IDF_PATH", reason or "")


class TestActivate(unittest.TestCase):
    def test_already_ready_returns_env(self):
        with tempfile.TemporaryDirectory() as td:
            root = _make_fake_idf_root(Path(td))
            environ = {
                "IDF_PATH": str(root),
                "PATH": str(root / "tools"),
                "SOMEKEY": "keep-me",
            }

            with mock.patch("shutil.which", return_value=str(root / "tools" / "idf.py")):
                with mock.patch(
                    "subprocess.run",
                    return_value=_fake_completed(
                        stdout="ESP-IDF v6.0.1\n",
                        returncode=0,
                    ),
                ) as m_run:
                    env = activate(environ)
        self.assertIsInstance(env, IdfEnv)
        self.assertEqual(env.source, "path")
        self.assertEqual(env.version, "6.0.1")
        self.assertEqual(env.environ["SOMEKEY"], "keep-me")
        # No PowerShell subprocess: only the idf.py --version probe.
        for call in m_run.call_args_list:
            args = call.args[0] if call.args else call.kwargs.get("args", [])
            # Never invoke powershell.exe in the hot-start path.
            joined = " ".join(str(x) for x in args)
            self.assertNotIn("powershell", joined.lower())

    def test_eim_profile_sourced_when_not_ready(self):
        with tempfile.TemporaryDirectory() as td:
            root = _make_fake_idf_root(Path(td))
            fake_profile = Path("C:/Espressif/tools/Microsoft.v6.0.PowerShell_profile.ps1")
            environ = {"PATH": ""}

            # Not ready initially: shutil.which returns None.
            def fake_which(name, path=None):
                return None

            def fake_run(cmd, *a, **kw):
                # PowerShell subprocess for EIM: emit banner + KV lines.
                return _fake_completed(
                    stdout=(
                        "ESP-IDF v6.0.1\n"
                        f"IDF_PATH={root}\n"
                        f"IDF_TOOLS_PATH={td}\\Espressif\n"
                        f"IDF_PYTHON_ENV_PATH={td}\\Espressif\\python_env\n"
                        "ESP_IDF_VERSION=6.0.1\n"
                        f"PATH={root / 'tools'};{td}\\Espressif\\python_env\\Scripts\n"
                    ),
                    returncode=0,
                )

            with mock.patch("shutil.which", side_effect=fake_which):
                with mock.patch.object(
                    act_mod, "_find_eim_profiles", return_value=[fake_profile]
                ):
                    with mock.patch("subprocess.run", side_effect=fake_run) as m_run:
                        env = activate(environ)
        self.assertEqual(env.source, "eim-profile")
        self.assertEqual(env.version, "6.0.1")
        self.assertEqual(env.environ["IDF_PATH"], str(root))
        self.assertIn("IDF_TOOLS_PATH", env.environ)
        self.assertIn("Espressif", env.environ["PATH"])
        # Confirm PowerShell was invoked with -NoProfile -Command.
        found_ps = False
        for call in m_run.call_args_list:
            args = call.args[0] if call.args else []
            if args and "powershell" in str(args[0]).lower():
                self.assertIn("-NoProfile", args)
                self.assertIn("-Command", args)
                # Body must dot-source the profile.
                cmd_body = args[-1]
                self.assertIn(str(fake_profile), cmd_body)
                # PS concatenation pattern (no embedded double-quotes around the KV).
                self.assertIn("('IDF_PATH=' + $env:IDF_PATH)", cmd_body)
                found_ps = True
        self.assertTrue(found_ps, "powershell.exe was not invoked")

    def test_eim_profile_failure_tries_next(self):
        with tempfile.TemporaryDirectory() as td:
            root = _make_fake_idf_root(Path(td))
            p1 = Path("C:/Espressif/tools/Microsoft.v6.0.PowerShell_profile.ps1")
            p2 = Path("C:/Espressif/tools/Microsoft.v6.1.PowerShell_profile.ps1")

            calls = {"n": 0}

            def fake_run(cmd, *a, **kw):
                # First powershell call fails, second succeeds.
                calls["n"] += 1
                if calls["n"] == 1:
                    return _fake_completed(stdout="oops\n", returncode=1)
                return _fake_completed(
                    stdout=(
                        "ESP-IDF v6.0.1\n"
                        f"IDF_PATH={root}\n"
                        f"IDF_TOOLS_PATH={td}\n"
                        "IDF_PYTHON_ENV_PATH=\n"
                        "ESP_IDF_VERSION=6.0.1\n"
                        f"PATH={root / 'tools'}\n"
                    ),
                    returncode=0,
                )

            with mock.patch("shutil.which", return_value=None):
                with mock.patch.object(
                    act_mod, "_find_eim_profiles", return_value=[p1, p2]
                ):
                    with mock.patch("subprocess.run", side_effect=fake_run):
                        env = activate({"PATH": ""})
        self.assertEqual(env.source, "eim-profile")
        self.assertEqual(calls["n"], 2)

    def test_idf_path_export_ps1_fallback(self):
        with tempfile.TemporaryDirectory() as td:
            root = _make_fake_idf_root(Path(td))
            # Simulate export.ps1 sitting in IDF_PATH.
            (root / "export.ps1").write_text("# fake\n", encoding="utf-8")
            environ = {"IDF_PATH": str(root), "PATH": ""}

            def fake_run(cmd, *a, **kw):
                # Must be a powershell invocation dot-sourcing export.ps1.
                joined = " ".join(str(x) for x in cmd)
                if "powershell" in joined.lower():
                    self.assertIn(str(root / "export.ps1").replace("\\", "/").split("/")[-1], joined + " " + cmd[-1])
                    return _fake_completed(
                        stdout=(
                            "ESP-IDF v6.0.1\n"
                            f"IDF_PATH={root}\n"
                            "IDF_TOOLS_PATH=\n"
                            "IDF_PYTHON_ENV_PATH=\n"
                            "ESP_IDF_VERSION=6.0.1\n"
                            f"PATH={root / 'tools'}\n"
                        ),
                        returncode=0,
                    )
                return _fake_completed(stdout="", returncode=0)

            with mock.patch("shutil.which", return_value=None):
                with mock.patch.object(act_mod, "_find_eim_profiles", return_value=[]):
                    with mock.patch("subprocess.run", side_effect=fake_run) as m_run:
                        env = activate(environ)
        self.assertEqual(env.source, "export-script")
        self.assertEqual(env.environ["IDF_PATH"], str(root))
        # The powershell call body must dot-source export.ps1.
        body_seen = False
        for call in m_run.call_args_list:
            args = call.args[0] if call.args else []
            if args and "powershell" in str(args[0]).lower():
                self.assertIn("export.ps1", args[-1])
                body_seen = True
        self.assertTrue(body_seen)

    def test_activation_error_when_nothing_works(self):
        with mock.patch("shutil.which", return_value=None):
            with mock.patch.object(act_mod, "_find_eim_profiles", return_value=[]):
                with self.assertRaises(IdfActivationError) as cm:
                    activate({"PATH": ""})
        msg = str(cm.exception)
        self.assertIn("preinstall.md", msg)
        self.assertIn("§", msg)  # § U+00A7
        self.assertIn("3", msg)
        # Must NOT mention the shim's own version literal in the give-up message.
        self.assertNotIn("v1.0.3", msg)

    def test_environ_merge_does_not_mutate_caller_dict(self):
        with tempfile.TemporaryDirectory() as td:
            root = _make_fake_idf_root(Path(td))
            original = {"PATH": "", "KEEP": "value"}
            snapshot = dict(original)
            fake_profile = Path("C:/Espressif/tools/Microsoft.v6.0.PowerShell_profile.ps1")

            def fake_run(cmd, *a, **kw):
                return _fake_completed(
                    stdout=(
                        "ESP-IDF v6.0.1\n"
                        f"IDF_PATH={root}\n"
                        "IDF_TOOLS_PATH=\n"
                        "IDF_PYTHON_ENV_PATH=\n"
                        "ESP_IDF_VERSION=6.0.1\n"
                        f"PATH={root / 'tools'}\n"
                    ),
                    returncode=0,
                )

            with mock.patch("shutil.which", return_value=None):
                with mock.patch.object(
                    act_mod, "_find_eim_profiles", return_value=[fake_profile]
                ):
                    with mock.patch("subprocess.run", side_effect=fake_run):
                        env = activate(original)
        self.assertEqual(original, snapshot, "caller dict was mutated")
        # And the returned dict is a NEW dict, not the same object.
        self.assertIsNot(env.environ, original)
        # os.environ untouched (spot-check IDF_PATH).
        # We do NOT rely on IDF_PATH being absent from real environ;
        # just confirm activate didn't inject the fake tempdir root.
        self.assertNotEqual(os.environ.get("IDF_PATH"), str(root))


if __name__ == "__main__":
    unittest.main()
