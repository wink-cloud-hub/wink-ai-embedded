"""Tests for host capability providers (python, jinja2, cmake, make, gcc).

All external tool invocations are mocked so these tests run without any
real toolchain installed.
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
from tools.toolchain.providers._version import (  # noqa: E402
    parse_version,
    version_ge,
)
from tools.toolchain.providers.base import Provider  # noqa: E402
from tools.toolchain.providers.cmake import CMakeProvider  # noqa: E402
from tools.toolchain.providers.gcc import GccProvider  # noqa: E402
from tools.toolchain.providers.make import MakeProvider  # noqa: E402
from tools.toolchain.providers.python_interp import PythonProvider  # noqa: E402
from tools.toolchain.providers.python_pkgs import Jinja2Provider  # noqa: E402
from tools.toolchain.resolve import ResolveContext  # noqa: E402


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


class TestVersionHelpers(unittest.TestCase):
    def test_parse_basic(self):
        self.assertEqual(parse_version("cmake version 3.28.1"), (3, 28, 1))

    def test_parse_two_component(self):
        # 14.2 with missing patch resolves to (14, 2)
        self.assertEqual(parse_version("gcc.exe (MinGW-W64) 14.2"), (14, 2))

    def test_parse_none_when_no_number(self):
        self.assertIsNone(parse_version("no version here"))

    def test_version_ge_true(self):
        self.assertTrue(version_ge("3.28.1", (3, 15)))

    def test_version_ge_false(self):
        self.assertFalse(version_ge("3.14.0", (3, 15)))

    def test_version_ge_equal(self):
        self.assertTrue(version_ge("14.0.0", (14,)))


class TestCMakeProvider(unittest.TestCase):
    def setUp(self):
        self.p = CMakeProvider()

    def test_id(self):
        self.assertEqual(self.p.id, "cmake")

    def test_detect_via_which_ok(self):
        ctx = _make_ctx()
        with mock.patch("shutil.which", return_value="C:/Program Files/CMake/bin/cmake.exe"):
            with mock.patch(
                "subprocess.run",
                return_value=_fake_completed(
                    stdout="cmake version 3.28.1\n\nCMake suite maintained and supported by Kitware (kitware.com/cmake).\n"
                ),
            ):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.version, "3.28.1")
        self.assertEqual(r.source, "path")

    def test_detect_version_below_floor(self):
        ctx = _make_ctx()
        with mock.patch("shutil.which", return_value="/usr/bin/cmake"):
            with mock.patch(
                "subprocess.run",
                return_value=_fake_completed(stdout="cmake version 3.14.5\n"),
            ):
                r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("3.14.5", r.reason)
        self.assertIn("3.15", r.reason)

    def test_detect_not_on_path(self):
        ctx = _make_ctx()
        with mock.patch("shutil.which", return_value=None):
            r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIsNone(r.path)

    def test_detect_explicit_config_wins_over_path(self):
        ctx = _make_ctx(workspace_paths={"cmake": "D:/ws/cmake.exe"})
        with mock.patch("subprocess.run") as m_run:
            m_run.return_value = _fake_completed(stdout="cmake version 3.30.0\n")
            with mock.patch("pathlib.Path.exists", return_value=True):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.source, "config:workspace")
        self.assertEqual(r.version, "3.30.0")

    def test_hint_mentions_cmake(self):
        h = self.p.hint(_make_ctx(os_name="nt"))
        self.assertIn("cmake", h.lower())


class TestGccProvider(unittest.TestCase):
    def setUp(self):
        self.p = GccProvider()

    def test_id(self):
        self.assertEqual(self.p.id, "gcc")

    def test_detect_windows_mingw_ok(self):
        ctx = _make_ctx(os_name="nt")

        def fake_run(cmd, *a, **kw):
            args = cmd if isinstance(cmd, list) else [cmd]
            if any("dumpmachine" in str(x) for x in args):
                return _fake_completed(stdout="x86_64-w64-mingw32\n")
            return _fake_completed(stdout="gcc.exe (MinGW-W64) 14.2.0\n")

        with mock.patch("shutil.which", return_value="C:/mingw/bin/gcc.exe"):
            with mock.patch("subprocess.run", side_effect=fake_run):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.version, "14.2.0")

    def test_detect_windows_wrong_triplet(self):
        ctx = _make_ctx(os_name="nt")

        def fake_run(cmd, *a, **kw):
            args = cmd if isinstance(cmd, list) else [cmd]
            if any("dumpmachine" in str(x) for x in args):
                return _fake_completed(stdout="x86_64-msys-junk\n")
            return _fake_completed(stdout="gcc.exe (MSYS) 14.2.0\n")

        with mock.patch("shutil.which", return_value="C:/msys/bin/gcc.exe"):
            with mock.patch("subprocess.run", side_effect=fake_run):
                r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("mingw", r.reason.lower())

    def test_detect_version_below_floor(self):
        ctx = _make_ctx(os_name="nt")

        def fake_run(cmd, *a, **kw):
            args = cmd if isinstance(cmd, list) else [cmd]
            if any("dumpmachine" in str(x) for x in args):
                return _fake_completed(stdout="x86_64-w64-mingw32\n")
            return _fake_completed(stdout="gcc.exe (MinGW-W64) 13.2.0\n")

        with mock.patch("shutil.which", return_value="C:/mingw/bin/gcc.exe"):
            with mock.patch("subprocess.run", side_effect=fake_run):
                r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("13.2.0", r.reason)
        self.assertIn("14", r.reason)

    def test_detect_posix_no_triplet_check(self):
        ctx = _make_ctx(os_name="posix")
        with mock.patch("shutil.which", return_value="/usr/bin/gcc"):
            with mock.patch(
                "subprocess.run",
                return_value=_fake_completed(stdout="gcc (Ubuntu 14.1.0-1) 14.1.0\n"),
            ):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.version, "14.1.0")

    def test_detect_env_prefix(self):
        # WINK_GCC_PREFIX points at a directory whose bin/gcc.exe is the
        # real compiler. We mock the file-system so is_dir(prefix) and
        # is_file(prefix/bin/gcc.exe) both return True.
        ctx = _make_ctx(
            os_name="nt",
            environ={"WINK_GCC_PREFIX": "D:/winlibs"},
        )

        def fake_run(cmd, *a, **kw):
            args = cmd if isinstance(cmd, list) else [cmd]
            if any("dumpmachine" in str(x) for x in args):
                return _fake_completed(stdout="x86_64-w64-mingw32\n")
            return _fake_completed(stdout="gcc.exe (MinGW-W64) 14.2.0\n")

        expected_exe = Path("D:/winlibs/bin/gcc.exe")

        def fake_is_file(self):
            return str(self) == str(expected_exe)

        def fake_is_dir(self):
            return str(self) == str(Path("D:/winlibs"))

        with mock.patch("subprocess.run", side_effect=fake_run):
            with mock.patch("pathlib.Path.is_file", fake_is_file):
                with mock.patch("pathlib.Path.is_dir", fake_is_dir):
                    r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.source, "env:WINK_GCC_PREFIX")

    def test_hint_windows(self):
        h = self.p.hint(_make_ctx(os_name="nt"))
        self.assertIn("winlibs", h.lower())

    def test_hint_posix(self):
        h = self.p.hint(_make_ctx(os_name="posix"))
        self.assertIn("gcc", h.lower())


class TestMakeProvider(unittest.TestCase):
    def setUp(self):
        self.p = MakeProvider()

    def test_id(self):
        self.assertEqual(self.p.id, "make")

    def test_detect_windows_mingw32_make(self):
        ctx = _make_ctx(os_name="nt")

        def fake_which(name):
            if "mingw32-make" in name:
                return "C:/mingw/bin/mingw32-make.exe"
            return None

        with mock.patch("shutil.which", side_effect=fake_which):
            with mock.patch(
                "subprocess.run",
                return_value=_fake_completed(stdout="GNU Make 4.4.1\nBuilt for ...\n"),
            ):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertIn("mingw32-make", str(r.path))

    def test_detect_posix_prefers_make_over_gmake(self):
        ctx = _make_ctx(os_name="posix")

        def fake_which(name):
            if name == "make":
                return "/usr/bin/make"
            if name == "gmake":
                return "/usr/local/bin/gmake"
            return None

        with mock.patch("shutil.which", side_effect=fake_which):
            with mock.patch(
                "subprocess.run",
                return_value=_fake_completed(stdout="GNU Make 4.3\n"),
            ):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.path.as_posix(), "/usr/bin/make")

    def test_detect_posix_falls_back_to_gmake(self):
        ctx = _make_ctx(os_name="posix")

        def fake_which(name):
            if name == "make":
                return None
            if name == "gmake":
                return "/usr/local/bin/gmake"
            return None

        with mock.patch("shutil.which", side_effect=fake_which):
            with mock.patch(
                "subprocess.run",
                return_value=_fake_completed(stdout="GNU Make 4.3\n"),
            ):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.path.as_posix(), "/usr/local/bin/gmake")

    def test_detect_missing(self):
        ctx = _make_ctx(os_name="nt")
        with mock.patch("shutil.which", return_value=None):
            r = self.p.detect(ctx)
        self.assertFalse(r.found)


class TestPythonProvider(unittest.TestCase):
    def setUp(self):
        self.p = PythonProvider()

    def test_id(self):
        self.assertEqual(self.p.id, "python")

    def test_detect_uses_sys_executable_by_default(self):
        ctx = _make_ctx()
        # No env, no config -> defaults to sys.executable.
        # Instead of running subprocess, PythonProvider should honor sys.version_info
        # as an optimization for the running interpreter. Either way the result
        # should be found=True.
        r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(str(r.path), sys.executable)

    def test_detect_env_override(self):
        ctx = _make_ctx(environ={"WINK_PYTHON": "C:/tools/python/python.exe"})
        with mock.patch(
            "subprocess.run",
            return_value=_fake_completed(stdout="Python 3.11.5\n", stderr=""),
        ):
            with mock.patch("pathlib.Path.exists", return_value=True):
                r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.source, "env:WINK_PYTHON")
        self.assertEqual(r.version, "3.11.5")

    def test_detect_version_below_floor(self):
        ctx = _make_ctx(environ={"WINK_PYTHON": "C:/old/python.exe"})
        with mock.patch(
            "subprocess.run",
            return_value=_fake_completed(stdout="Python 3.9.0\n"),
        ):
            with mock.patch("pathlib.Path.exists", return_value=True):
                r = self.p.detect(ctx)
        self.assertFalse(r.found)
        self.assertIn("3.9.0", r.reason)
        self.assertIn("3.10", r.reason)


class TestJinja2Provider(unittest.TestCase):
    def setUp(self):
        self.p = Jinja2Provider()

    def test_id(self):
        self.assertEqual(self.p.id, "jinja2")

    def test_detect_import_ok(self):
        ctx = _make_ctx()
        with mock.patch(
            "subprocess.run",
            return_value=_fake_completed(stdout="3.1.4\n", returncode=0),
        ):
            r = self.p.detect(ctx)
        self.assertTrue(r.found, r.reason)
        self.assertEqual(r.version, "3.1.4")

    def test_detect_import_fails(self):
        ctx = _make_ctx()
        with mock.patch(
            "subprocess.run",
            return_value=_fake_completed(
                stdout="",
                stderr="ModuleNotFoundError: No module named 'jinja2'\n",
                returncode=1,
            ),
        ):
            r = self.p.detect(ctx)
        self.assertFalse(r.found)

    def test_hint_mentions_pip(self):
        h = self.p.hint(_make_ctx())
        self.assertIn("pip", h.lower())
        self.assertIn("jinja2", h.lower())


class TestRegistry(unittest.TestCase):
    def test_all_host_providers_registered(self):
        for cap_id in ("python", "jinja2", "cmake", "make", "gcc"):
            self.assertIn(cap_id, REGISTRY, f"missing {cap_id!r}")
            self.assertIsInstance(REGISTRY[cap_id], Provider)
            self.assertEqual(REGISTRY[cap_id].id, cap_id)


class TestPlatformHints(unittest.TestCase):
    def test_get_hints_returns_platform_hints(self):
        from tools.toolchain.platform import get_hints
        from tools.toolchain.platform.base import PlatformHints

        h_nt = get_hints("nt")
        self.assertIsInstance(h_nt, PlatformHints)
        # Windows hint for gcc mentions winlibs
        self.assertIn("winlibs", h_nt.install_hint("gcc").lower())

        h_posix = get_hints("posix")
        self.assertIsInstance(h_posix, PlatformHints)
        # posix falls back to a generic package-manager hint
        posix_gcc = h_posix.install_hint("gcc").lower()
        self.assertTrue("gcc" in posix_gcc or "package manager" in posix_gcc)


if __name__ == "__main__":
    unittest.main()
