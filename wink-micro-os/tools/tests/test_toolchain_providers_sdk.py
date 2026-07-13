"""Tests for SDK capability providers (emsdk, idf, node, powershell).

All external tool invocations, filesystem probes, and platform lookups are
mocked so these tests run without any real SDK installed and are
platform-agnostic.
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

from tools.toolchain.providers import REGISTRY  # noqa: E402
from tools.toolchain.providers.base import Provider  # noqa: E402
from tools.toolchain.providers.emsdk import EmsdkProvider  # noqa: E402
from tools.toolchain.providers.idf import IdfProvider  # noqa: E402
from tools.toolchain.providers.node import NodeProvider  # noqa: E402
from tools.toolchain.providers.powershell import PowerShellProvider  # noqa: E402
from tools.toolchain.resolve import ResolveContext  # noqa: E402
from tools.toolchain.types import PROBE_TIMEOUT_SEC, UnsupportedError  # noqa: E402


def _make_ctx(
    environ: dict[str, str] | None = None,
    user_paths: dict[str, str] | None = None,
    workspace_paths: dict[str, str] | None = None,
    os_name: str = "nt",
) -> ResolveContext:
    return ResolveContext(
        environ=dict(environ or {}),
        user_paths=dict(user_paths or {}),
        workspace_paths=dict(workspace_paths or {}),
        tools_home=None,
        workspace_root=None,
        os_name=os_name,
    )


def _fake_completed(stdout: str = "", stderr: str = "", returncode: int = 0):
    cp = subprocess.CompletedProcess(args=[], returncode=returncode)
    cp.stdout = stdout
    cp.stderr = stderr
    return cp


# ---------------------------------------------------------------------------
# emsdk
# ---------------------------------------------------------------------------


class TestEmsdkProvider(unittest.TestCase):
    def setUp(self):
        self.p = EmsdkProvider()

    def test_id(self):
        self.assertEqual(self.p.id, "emsdk")

    def test_detect_happy_path(self):
        ctx = _make_ctx(environ={"EMSDK": "C:/emsdk"})

        def fake_which(name):
            if "emcc" in name:
                return "C:/emsdk/upstream/emscripten/emcc.bat"
            if "emcmake" in name:
                return "C:/emsdk/upstream/emscripten/emcmake.bat"
            return None

        def fake_run(cmd, *a, **kw):
            args = cmd if isinstance(cmd, list) else [cmd]
            joined = " ".join(str(x) for x in args)
            if "emcmake" in joined:
                return _fake_completed(stdout="emcmake, a helper to configure emscripten builds\n")
            # emcc --version
            return _fake_completed(
                stdout=(
                    "emcc (Emscripten gcc/clang-like replacement + linker "
                    "emulating GNU ld) 3.1.74 (abcdef)\n"
                )
            )

        with mock.patch("shutil.which", side_effect=fake_which):
            with mock.patch("subprocess.run", side_effect=fake_run):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.version, "3.1.74")
        self.assertEqual(r.source, "env:EMSDK")

    def test_detect_high_version_still_passes(self):
        # emcc bundled with IDF may report 6.x — still >= floor 3.1.45.
        ctx = _make_ctx(environ={"EMSDK": "C:/emsdk"})

        def fake_which(name):
            return "C:/emsdk/tool.bat"

        def fake_run(cmd, *a, **kw):
            args = cmd if isinstance(cmd, list) else [cmd]
            joined = " ".join(str(x) for x in args)
            if "emcmake" in joined:
                return _fake_completed(stdout="ok\n")
            return _fake_completed(stdout="emcc (Emscripten ...) 6.0.1 (xyz)\n")

        with mock.patch("shutil.which", side_effect=fake_which):
            with mock.patch("subprocess.run", side_effect=fake_run):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.version, "6.0.1")

    def test_detect_version_below_floor(self):
        ctx = _make_ctx(environ={"EMSDK": "C:/emsdk"})

        def fake_which(name):
            return "C:/emsdk/tool.bat"

        def fake_run(cmd, *a, **kw):
            args = cmd if isinstance(cmd, list) else [cmd]
            joined = " ".join(str(x) for x in args)
            if "emcmake" in joined:
                return _fake_completed(stdout="ok\n")
            return _fake_completed(stdout="emcc (Emscripten ...) 3.1.10 (old)\n")

        with mock.patch("shutil.which", side_effect=fake_which):
            with mock.patch("subprocess.run", side_effect=fake_run):
                r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("3.1.10", r.reason)
        self.assertIn("below", r.reason.lower())

    def test_detect_emsdk_set_but_emcc_missing(self):
        ctx = _make_ctx(environ={"EMSDK": "C:/emsdk"})

        with mock.patch("shutil.which", return_value=None):
            r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("not on PATH", r.reason)

    def test_detect_emsdk_set_but_emcc_subprocess_fails(self):
        ctx = _make_ctx(environ={"EMSDK": "C:/emsdk"})

        def fake_which(name):
            return "C:/emsdk/tool.bat"

        def fake_run(cmd, *a, **kw):
            raise FileNotFoundError("emcc.bat gone")

        with mock.patch("shutil.which", side_effect=fake_which):
            with mock.patch("subprocess.run", side_effect=fake_run):
                r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("not on PATH", r.reason)

    def test_detect_emsdk_not_set(self):
        ctx = _make_ctx(environ={})
        r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("not activated", r.reason.lower())

    def test_hint_mentions_emsdk(self):
        h = self.p.hint(_make_ctx(os_name="nt"))
        self.assertIn("emsdk", h.lower())

    def test_install_raises_unsupported(self):
        with self.assertRaises(UnsupportedError):
            self.p.install(_make_ctx())


# ---------------------------------------------------------------------------
# idf
# ---------------------------------------------------------------------------


class TestIdfProvider(unittest.TestCase):
    def setUp(self):
        self.p = IdfProvider()

    def test_id(self):
        self.assertEqual(self.p.id, "idf")

    def test_install_always_raises_unsupported(self):
        with self.assertRaises(UnsupportedError) as cm:
            self.p.install(_make_ctx(os_name="nt"))
        self.assertIn("never auto-installed", str(cm.exception))
        self.assertIn("EIM", str(cm.exception))

    def test_detect_happy_path_on_path(self):
        ctx = _make_ctx(
            os_name="nt",
            environ={"IDF_PATH": "C:/Espressif/frameworks/esp-idf-v6.0.1"},
        )

        def fake_which(name):
            if "idf.py" in name:
                return "C:/Espressif/frameworks/esp-idf-v6.0.1/tools/idf.py"
            return None

        def fake_run(cmd, *a, **kw):
            return _fake_completed(
                stdout="ESP-IDF v6.0.1-dirty\n",
                returncode=0,
            )

        def fake_is_dir(self):
            return str(self) == "C:/Espressif/frameworks/esp-idf-v6.0.1" or str(self).endswith("esp-idf-v6.0.1")

        def fake_is_file(self):
            return "idf.py" in str(self)

        with mock.patch("shutil.which", side_effect=fake_which):
            with mock.patch("subprocess.run", side_effect=fake_run):
                with mock.patch("pathlib.Path.is_dir", fake_is_dir):
                    with mock.patch("pathlib.Path.is_file", fake_is_file):
                        r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.version, "6.0.1")
        self.assertEqual(r.source, "path")

    def test_detect_version_out_of_range(self):
        # v5.x is not accepted (>=6.0,<7.0 required).
        ctx = _make_ctx(
            os_name="nt",
            environ={"IDF_PATH": "C:/Espressif/frameworks/esp-idf-v5.1.2"},
        )

        def fake_which(name):
            if "idf.py" in name:
                return "C:/Espressif/frameworks/esp-idf-v5.1.2/tools/idf.py"
            return None

        def fake_run(cmd, *a, **kw):
            return _fake_completed(stdout="ESP-IDF v5.1.2\n")

        def fake_is_dir(self):
            return True

        def fake_is_file(self):
            return True

        with mock.patch("shutil.which", side_effect=fake_which):
            with mock.patch("subprocess.run", side_effect=fake_run):
                with mock.patch("pathlib.Path.is_dir", fake_is_dir):
                    with mock.patch("pathlib.Path.is_file", fake_is_file):
                        r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("5.1.2", r.reason)

    def test_detect_idf_path_set_but_not_on_path(self):
        ctx = _make_ctx(
            os_name="nt",
            environ={"IDF_PATH": "C:/Espressif/frameworks/esp-idf-v6.0.1"},
        )

        # idf.py not on PATH and no EIM profile discoverable.
        with mock.patch("shutil.which", return_value=None):
            with mock.patch(
                "tools.toolchain.providers.idf._find_eim_profiles",
                return_value=[],
            ):
                r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("IDF_PATH", r.reason)
        self.assertIn("PATH", r.reason)

    def test_detect_not_installed(self):
        ctx = _make_ctx(os_name="nt", environ={})
        with mock.patch("shutil.which", return_value=None):
            with mock.patch(
                "tools.toolchain.providers.idf._find_eim_profiles",
                return_value=[],
            ):
                r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("never auto-install", r.reason.lower())

    def test_detect_non_windows_fails(self):
        ctx = _make_ctx(os_name="posix")
        r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("Windows", r.reason)

    def test_detect_via_eim_profile(self):
        # No idf.py on PATH, but an EIM v6 profile exists and, when sourced
        # in a PowerShell subprocess, exposes idf.py and IDF_PATH.
        ctx = _make_ctx(os_name="nt", environ={})
        profile = Path("C:/Espressif/tools/Microsoft.v6.PowerShell_profile.ps1")

        def fake_run(cmd, *a, **kw):
            # We only expect the PowerShell subprocess path here.
            return _fake_completed(
                stdout=(
                    "ESP-IDF v6.0.1\n"
                    "IDF_PATH=C:\\Espressif\\frameworks\\esp-idf-v6.0.1\n"
                    "IDF_TOOLS_PATH=C:\\Espressif\n"
                ),
                returncode=0,
            )

        with mock.patch("shutil.which", return_value=None):
            with mock.patch(
                "tools.toolchain.providers.idf._find_eim_profiles",
                return_value=[profile],
            ):
                with mock.patch("subprocess.run", side_effect=fake_run) as m_run:
                    r = self.p.detect(ctx)

        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.version, "6.0.1")
        self.assertEqual(r.source, "eim-profile")
        self.assertIsNotNone(r.extra_env)
        self.assertEqual(r.extra_env.get("IDF_TOOLS_PATH"), r"C:\Espressif")
        # Verify the subprocess used a bounded timeout. The EIM probe uses
        # a longer per-call timeout than the shared PROBE_TIMEOUT_SEC (10s)
        # because sourcing the profile activates a Python venv (~12s
        # real-world).
        _, kwargs = m_run.call_args
        self.assertIn("timeout", kwargs)
        self.assertLessEqual(kwargs["timeout"], 60)

    def test_hint_mentions_eim(self):
        h = self.p.hint(_make_ctx(os_name="nt"))
        self.assertIn("EIM", h)
        self.assertIn("never", h.lower())


# ---------------------------------------------------------------------------
# node
# ---------------------------------------------------------------------------


class TestNodeProvider(unittest.TestCase):
    def setUp(self):
        self.p = NodeProvider()

    def test_id(self):
        self.assertEqual(self.p.id, "node")

    def test_detect_happy_path(self):
        ctx = _make_ctx(os_name="nt")
        with mock.patch("shutil.which", return_value="C:/Program Files/nodejs/node.exe"):
            with mock.patch(
                "subprocess.run",
                return_value=_fake_completed(stdout="v20.11.0\n"),
            ):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.version, "20.11.0")
        self.assertEqual(r.source, "path")

    def test_detect_missing(self):
        ctx = _make_ctx(os_name="nt")
        with mock.patch("shutil.which", return_value=None):
            r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIsNone(r.path)

    def test_hint_mentions_nodejs(self):
        h = self.p.hint(_make_ctx(os_name="nt"))
        self.assertIn("node", h.lower())

    def test_probe_uses_timeout(self):
        ctx = _make_ctx(os_name="nt")
        with mock.patch("shutil.which", return_value="C:/nodejs/node.exe"):
            with mock.patch(
                "subprocess.run",
                return_value=_fake_completed(stdout="v20.11.0\n"),
            ) as m_run:
                self.p.detect(ctx)
        # subprocess.run must have been called with a bounded timeout.
        _, kwargs = m_run.call_args
        self.assertEqual(kwargs.get("timeout"), PROBE_TIMEOUT_SEC)


# ---------------------------------------------------------------------------
# powershell
# ---------------------------------------------------------------------------


class TestPowerShellProvider(unittest.TestCase):
    def setUp(self):
        self.p = PowerShellProvider()

    def test_id(self):
        self.assertEqual(self.p.id, "powershell")

    def test_detect_windows_ok(self):
        ctx = _make_ctx(os_name="nt")
        with mock.patch("pathlib.Path.exists", return_value=True):
            r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertIn("powershell.exe", str(r.path).lower())

    def test_detect_windows_missing(self):
        ctx = _make_ctx(os_name="nt")
        with mock.patch("pathlib.Path.exists", return_value=False):
            r = self.p.detect(ctx)
        self.assertFalse(r.found)

    def test_detect_non_windows_fails(self):
        ctx = _make_ctx(os_name="posix")
        r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("Windows", r.reason)

    def test_hint_non_windows(self):
        h = self.p.hint(_make_ctx(os_name="posix"))
        self.assertIn("Windows", h)


# ---------------------------------------------------------------------------
# Registry
# ---------------------------------------------------------------------------


class TestRegistry(unittest.TestCase):
    def test_all_nine_providers_registered(self):
        expected = (
            "python",
            "jinja2",
            "cmake",
            "make",
            "gcc",
            "emsdk",
            "idf",
            "node",
            "powershell",
        )
        for cap_id in expected:
            self.assertIn(cap_id, REGISTRY, f"missing {cap_id!r}")
            self.assertIsInstance(REGISTRY[cap_id], Provider)
            self.assertEqual(REGISTRY[cap_id].id, cap_id)


if __name__ == "__main__":
    unittest.main()
