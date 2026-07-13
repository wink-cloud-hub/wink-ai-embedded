"""Tests for :mod:`tools.esp32.build`.

All ``subprocess.run`` and ``activate()`` calls are mocked; these tests
must pass on any machine with no ESP-IDF installed. They cover:

1. MSYS/EMSDK env vars are stripped exactly per the PS1 list.
2. UTF-8 env vars are set BEFORE activation.
3. ``activate()`` is called with the pre-stripped, UTF-8-armed environ.
4. UTF-8 env vars are re-asserted AFTER activation (EIM profiles reset
   these).
5. The subprocess command line is ``["idf.py", "-C", <fw>, *idf_args]``.
6. Exit code from subprocess is returned verbatim.
7. Default cwd is the parent of ``esp32_firmware_dir`` (repo root).
8. Explicit ``cwd`` argument overrides the default.
9. ``main(argv)`` CLI parses ``--esp32-firmware-dir`` and REMAINDER args.
10. :class:`IdfActivationError` propagates out of ``run_idf``.
11. ``shell=True`` is NOT used (we control the env directly).
12. ``capture_output=True`` is NOT used (stdout/stderr stream to console).
"""
from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock


SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.esp32 import build as build_mod  # noqa: E402
from tools.esp32.activate import IdfActivationError, IdfEnv  # noqa: E402


def _fake_idf_env(environ: dict[str, str] | None = None) -> IdfEnv:
    """Build a fake IdfEnv. The environ acts as the merged post-activation env."""
    env = dict(environ) if environ is not None else {}
    return IdfEnv(
        idf_path=Path(r"C:\fake\idf"),
        environ=env,
        version="6.0.1",
        source="path",
    )


class _RecordingRun:
    """Callable mock for subprocess.run that records call args and returns rc."""

    def __init__(self, returncode: int = 0):
        self.returncode = returncode
        self.calls: list[dict] = []

    def __call__(self, cmd, *args, **kwargs):
        self.calls.append({"cmd": cmd, "args": args, "kwargs": kwargs})
        cp = subprocess.CompletedProcess(args=cmd, returncode=self.returncode)
        return cp


class TestRunIdfEnvHygiene(unittest.TestCase):
    def test_strips_msys_emsdk_vars(self):
        starting = {
            "MSYSTEM": "MSYS",
            "MSYS": "1",
            "MINGW_PREFIX": "/mingw64",
            "MSYSTEM_PREFIX": "/mingw64",
            "EMSDK": "/emsdk",
            "EMSDK_NODE": "/emsdk/node",
            "EMSDK_PYTHON": "/emsdk/python",
            "PATH": "/some/path",
        }

        captured: dict = {}

        def fake_activate(env):
            # Record what activate saw (should already be clean of contamination).
            captured["activate_env"] = dict(env)
            return _fake_idf_env(dict(env))

        rec = _RecordingRun(returncode=0)
        with mock.patch("tools.esp32.build.activate_idf", side_effect=fake_activate):
            with mock.patch("subprocess.run", side_effect=rec):
                rc = build_mod.run_idf(
                    esp32_firmware_dir=Path(r"C:\repo\esp32_firmware"),
                    idf_args=["build"],
                    environ=starting,
                )
        self.assertEqual(rc, 0)
        # Env passed to activate is stripped.
        seen = captured["activate_env"]
        for key in ("MSYSTEM", "MSYS", "MINGW_PREFIX", "MSYSTEM_PREFIX",
                    "EMSDK", "EMSDK_NODE", "EMSDK_PYTHON"):
            self.assertNotIn(key, seen, f"{key} should have been popped")
        # Env passed to subprocess is likewise stripped.
        sub_env = rec.calls[0]["kwargs"]["env"]
        for key in ("MSYSTEM", "MSYS", "MINGW_PREFIX", "MSYSTEM_PREFIX",
                    "EMSDK", "EMSDK_NODE", "EMSDK_PYTHON"):
            self.assertNotIn(key, sub_env, f"{key} should have been popped")

    def test_sets_utf8_before_activate(self):
        captured: dict = {}

        def fake_activate(env):
            captured["activate_env"] = dict(env)
            return _fake_idf_env(dict(env))

        rec = _RecordingRun(returncode=0)
        with mock.patch("tools.esp32.build.activate_idf", side_effect=fake_activate):
            with mock.patch("subprocess.run", side_effect=rec):
                build_mod.run_idf(
                    esp32_firmware_dir=Path(r"C:\repo\esp32_firmware"),
                    idf_args=["build"],
                    environ={},
                )
        self.assertEqual(captured["activate_env"].get("PYTHONUTF8"), "1")
        self.assertEqual(captured["activate_env"].get("PYTHONIOENCODING"), "utf-8")

    def test_re_asserts_utf8_after_activate(self):
        """Even if activate returns an env with PYTHONUTF8 clobbered, subprocess
        should see UTF-8 restored."""

        def fake_activate(env):
            # Simulate EIM profile resetting these.
            merged = dict(env)
            merged["PYTHONUTF8"] = "0"
            merged["PYTHONIOENCODING"] = "cp936"
            return _fake_idf_env(merged)

        rec = _RecordingRun(returncode=0)
        with mock.patch("tools.esp32.build.activate_idf", side_effect=fake_activate):
            with mock.patch("subprocess.run", side_effect=rec):
                build_mod.run_idf(
                    esp32_firmware_dir=Path(r"C:\repo\esp32_firmware"),
                    idf_args=["build"],
                    environ={},
                )
        sub_env = rec.calls[0]["kwargs"]["env"]
        self.assertEqual(sub_env.get("PYTHONUTF8"), "1")
        self.assertEqual(sub_env.get("PYTHONIOENCODING"), "utf-8")


class TestRunIdfCommandLine(unittest.TestCase):
    def test_command_line_shape(self):
        """cmd is ``<python> <idf.py> -C <fw> <args...>``; we invoke via the
        venv python + IDF's ``tools/idf.py`` script (not bare ``idf.py``) so
        subprocess.run with shell=False can resolve idf.py without file
        associations or PATH launching tricks."""
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            idf_path = root / "esp-idf"
            (idf_path / "tools").mkdir(parents=True)
            (idf_path / "tools" / "idf.py").write_text("#!/usr/bin/env python\n")
            venv = root / "venv"
            (venv / "Scripts").mkdir(parents=True)
            venv_py = venv / "Scripts" / "python.exe"
            venv_py.write_bytes(b"")

            def fake_activate(env):
                from tools.esp32.activate import IdfEnv
                env = dict(env)
                env["IDF_PYTHON_ENV_PATH"] = str(venv)
                return IdfEnv(
                    idf_path=idf_path,
                    environ=env,
                    version="6.0.1",
                    source="eim-profile",
                )

            rec = _RecordingRun(returncode=0)
            with mock.patch("tools.esp32.build.activate_idf", side_effect=fake_activate):
                with mock.patch("subprocess.run", side_effect=rec):
                    build_mod.run_idf(
                        esp32_firmware_dir=root / "esp32_firmware",
                        idf_args=["-DFOO=1", "build", "-v"],
                        environ={},
                    )
            cmd = rec.calls[0]["cmd"]
            # First arg is the venv python.
            self.assertEqual(Path(cmd[0]), venv_py)
            # Second arg is the idf.py script.
            self.assertEqual(Path(cmd[1]), idf_path / "tools" / "idf.py")
            self.assertEqual(cmd[2], "-C")
            self.assertEqual(Path(cmd[3]), root / "esp32_firmware")
            self.assertEqual(cmd[4:], ["-DFOO=1", "build", "-v"])

    def test_returns_subprocess_exit_code(self):
        def fake_activate(env):
            return _fake_idf_env(dict(env))

        rec = _RecordingRun(returncode=2)
        with mock.patch("tools.esp32.build.activate_idf", side_effect=fake_activate):
            with mock.patch("subprocess.run", side_effect=rec):
                rc = build_mod.run_idf(
                    esp32_firmware_dir=Path(r"C:\repo\esp32_firmware"),
                    idf_args=["build"],
                    environ={},
                )
        self.assertEqual(rc, 2)

    def test_default_cwd_is_repo_root(self):
        def fake_activate(env):
            return _fake_idf_env(dict(env))

        rec = _RecordingRun(returncode=0)
        with mock.patch("tools.esp32.build.activate_idf", side_effect=fake_activate):
            with mock.patch("subprocess.run", side_effect=rec):
                build_mod.run_idf(
                    esp32_firmware_dir=Path(r"C:\repo\esp32_firmware"),
                    idf_args=["build"],
                    environ={},
                )
        cwd = rec.calls[0]["kwargs"].get("cwd")
        self.assertEqual(Path(cwd), Path(r"C:\repo"))

    def test_explicit_cwd_overrides_default(self):
        def fake_activate(env):
            return _fake_idf_env(dict(env))

        rec = _RecordingRun(returncode=0)
        with mock.patch("tools.esp32.build.activate_idf", side_effect=fake_activate):
            with mock.patch("subprocess.run", side_effect=rec):
                build_mod.run_idf(
                    esp32_firmware_dir=Path(r"C:\repo\esp32_firmware"),
                    idf_args=["build"],
                    environ={},
                    cwd=Path(r"C:\somewhere\else"),
                )
        cwd = rec.calls[0]["kwargs"].get("cwd")
        self.assertEqual(Path(cwd), Path(r"C:\somewhere\else"))

    def test_does_not_use_shell(self):
        def fake_activate(env):
            return _fake_idf_env(dict(env))

        rec = _RecordingRun(returncode=0)
        with mock.patch("tools.esp32.build.activate_idf", side_effect=fake_activate):
            with mock.patch("subprocess.run", side_effect=rec):
                build_mod.run_idf(
                    esp32_firmware_dir=Path(r"C:\repo\esp32_firmware"),
                    idf_args=["build"],
                    environ={},
                )
        kwargs = rec.calls[0]["kwargs"]
        # shell must NOT be True (may be absent or False).
        self.assertFalse(kwargs.get("shell", False))

    def test_does_not_capture_output(self):
        def fake_activate(env):
            return _fake_idf_env(dict(env))

        rec = _RecordingRun(returncode=0)
        with mock.patch("tools.esp32.build.activate_idf", side_effect=fake_activate):
            with mock.patch("subprocess.run", side_effect=rec):
                build_mod.run_idf(
                    esp32_firmware_dir=Path(r"C:\repo\esp32_firmware"),
                    idf_args=["build"],
                    environ={},
                )
        kwargs = rec.calls[0]["kwargs"]
        # capture_output must not be True (stdout/stderr should inherit).
        self.assertFalse(kwargs.get("capture_output", False))
        # Also assert neither stdout nor stderr were pinned to PIPE.
        self.assertIn(kwargs.get("stdout"), (None, sys.stdout))
        self.assertIn(kwargs.get("stderr"), (None, sys.stderr))


class TestRunIdfActivateContract(unittest.TestCase):
    def test_activation_error_propagates(self):
        def fake_activate(env):
            raise IdfActivationError("no IDF")

        rec = _RecordingRun(returncode=0)
        with mock.patch("tools.esp32.build.activate_idf", side_effect=fake_activate):
            with mock.patch("subprocess.run", side_effect=rec):
                with self.assertRaises(IdfActivationError):
                    build_mod.run_idf(
                        esp32_firmware_dir=Path(r"C:\repo\esp32_firmware"),
                        idf_args=["build"],
                        environ={},
                    )
        # subprocess should NOT have been called.
        self.assertEqual(len(rec.calls), 0)


class TestBuildCli(unittest.TestCase):
    def test_main_parses_args_and_calls_run_idf(self):
        recorded = {}

        def fake_run_idf(*, esp32_firmware_dir, idf_args, cwd=None, environ=None):
            recorded["esp32_firmware_dir"] = esp32_firmware_dir
            recorded["idf_args"] = list(idf_args)
            recorded["cwd"] = cwd
            return 0

        with mock.patch("tools.esp32.build.run_idf", side_effect=fake_run_idf):
            rc = build_mod.main([
                "--esp32-firmware-dir", r"C:\tmp\fw",
                "--",
                "build",
            ])
        self.assertEqual(rc, 0)
        self.assertEqual(Path(recorded["esp32_firmware_dir"]), Path(r"C:\tmp\fw"))
        self.assertEqual(recorded["idf_args"], ["build"])

    def test_main_forwards_multi_args(self):
        recorded = {}

        def fake_run_idf(*, esp32_firmware_dir, idf_args, cwd=None, environ=None):
            recorded["idf_args"] = list(idf_args)
            return 0

        with mock.patch("tools.esp32.build.run_idf", side_effect=fake_run_idf):
            build_mod.main([
                "--esp32-firmware-dir", r"C:\tmp\fw",
                "--",
                "-DFOO=1", "-DBAR=2", "build",
            ])
        self.assertEqual(recorded["idf_args"], ["-DFOO=1", "-DBAR=2", "build"])

    def test_main_returns_exit_code(self):
        with mock.patch("tools.esp32.build.run_idf", return_value=7):
            rc = build_mod.main([
                "--esp32-firmware-dir", r"C:\tmp\fw",
                "--",
                "build",
            ])
        self.assertEqual(rc, 7)

    def test_main_surfaces_activation_error(self):
        def fake_run_idf(**kwargs):
            raise IdfActivationError("no IDF")

        with mock.patch("tools.esp32.build.run_idf", side_effect=fake_run_idf):
            rc = build_mod.main([
                "--esp32-firmware-dir", r"C:\tmp\fw",
                "--",
                "build",
            ])
        self.assertNotEqual(rc, 0)


if __name__ == "__main__":
    unittest.main()
