"""Tests for ensure_for orchestration + env injection (Task 8).

These tests monkey-patch the provider REGISTRY with fakes returning canned
DetectResult values, then exercise the profile → env matrix in check.py.
No real subprocess calls, no real filesystem probes.
"""
from __future__ import annotations

import io
import os
import sys
import unittest
from contextlib import redirect_stderr
from pathlib import Path
from unittest import mock

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.toolchain import providers as providers_mod  # noqa: E402
from tools.toolchain.check import ensure_for  # noqa: E402
from tools.toolchain.providers.base import Provider  # noqa: E402
from tools.toolchain.types import DetectResult  # noqa: E402


class _FakeProvider(Provider):
    """Provider stub with a scripted detect() outcome and call counter."""

    def __init__(self, cap_id: str, result: DetectResult, hint_text: str = "install me"):
        self.id = cap_id
        self._result = result
        self._hint = hint_text
        self.call_count = 0

    def detect(self, ctx):  # noqa: D401
        self.call_count += 1
        return self._result

    def hint(self, ctx):
        return self._hint


def _found(path_str: str, version: str = "1.0.0", source: str = "path") -> DetectResult:
    return DetectResult(
        found=True,
        path=Path(path_str),
        version=version,
        reason=None,
        source=source,
    )


def _missing(reason: str) -> DetectResult:
    return DetectResult(
        found=False,
        path=None,
        version=None,
        reason=reason,
        source=None,
    )


def _all_found_registry(gcc_bin: str = "C:/tools/mingw/bin/gcc.exe",
                        cmake_bin: str = "C:/tools/cmake/bin/cmake.exe",
                        make_bin: str = "C:/tools/make/bin/make.exe",
                        idf_root: str = "C:/Espressif/frameworks/esp-idf-v6.0",
                        node_bin: str = "C:/Program Files/nodejs/node.exe",
                        emsdk_root: str = "D:/emsdk",
                        ) -> dict[str, _FakeProvider]:
    """Return a REGISTRY-shape dict where every capability is present."""
    return {
        "python":     _FakeProvider("python",     _found("C:/py/python.exe", "3.11.4")),
        "jinja2":     _FakeProvider("jinja2",     _found("C:/py/Lib/site-packages/jinja2", "3.1.2")),
        "cmake":      _FakeProvider("cmake",      _found(cmake_bin, "3.28.0")),
        "make":       _FakeProvider("make",       _found(make_bin, "4.4.1")),
        "gcc":        _FakeProvider("gcc",        _found(gcc_bin, "16.1.0")),
        "emsdk":      _FakeProvider("emsdk",      _found(emsdk_root, "3.1.55")),
        "idf":        _FakeProvider("idf",        _found(idf_root, "6.0.1", source="eim-profile")),
        "node":       _FakeProvider("node",       _found(node_bin, "20.11.1")),
        "powershell": _FakeProvider("powershell", _found("C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe", "5.1")),
    }


class TestEnsureFor(unittest.TestCase):
    def setUp(self):
        # Fresh environ per test so PATH mutations don't bleed.
        self._env_patch = mock.patch.dict(os.environ, {}, clear=False)
        self._env_patch.start()
        # Snapshot PATH so we can restore afterwards.
        self._saved_path = os.environ.get("PATH", "")

    def tearDown(self):
        os.environ["PATH"] = self._saved_path
        self._env_patch.stop()

    # ---- 1. Missing required tool on 'host' → SystemExit(1) + report ---------

    def test_missing_cmake_on_host_exits_and_reports(self):
        reg = _all_found_registry()
        reg["cmake"] = _FakeProvider("cmake", _missing("cmake not found on PATH"))
        buf = io.StringIO()
        with mock.patch.dict(providers_mod.REGISTRY, reg, clear=True):
            with redirect_stderr(buf):
                with self.assertRaises(SystemExit) as cm:
                    ensure_for("host", workspace_root=None)
        self.assertEqual(cm.exception.code, 1)
        out = buf.getvalue()
        self.assertIn("cmake", out)
        # Should surface as a required_tool failure.
        self.assertIn("Missing required tools", out)

    # ---- 2. All host deps present → PATH prepended, no exit ------------------

    def test_host_profile_prepends_bin_dirs(self):
        gcc = "C:/tools/mingw/bin/gcc.exe"
        cmake = "C:/tools/cmake/bin/cmake.exe"
        make = "C:/tools/make/bin/make.exe"
        reg = _all_found_registry(gcc_bin=gcc, cmake_bin=cmake, make_bin=make)
        with mock.patch.dict(providers_mod.REGISTRY, reg, clear=True):
            ensure_for("host", workspace_root=None)  # should not raise
        path = os.environ.get("PATH", "")
        # Prepended in some order; each parent bin dir must be present at head.
        head = path.split(os.pathsep)
        self.assertIn(str(Path(gcc).parent), head)
        self.assertIn(str(Path(cmake).parent), head)
        self.assertIn(str(Path(make).parent), head)

    # ---- 3. esp32 profile with IDF via EIM → no host bin prepend + UTF-8 env -

    def test_esp32_profile_no_host_bin_prepend_sets_utf8(self):
        # Ensure PYTHONUTF8 not preset.
        os.environ.pop("PYTHONUTF8", None)
        os.environ.pop("PYTHONIOENCODING", None)
        # Save PATH sentinel.
        os.environ["PATH"] = "SENTINEL_PATH_ONLY"

        idf_root = "C:/Espressif/frameworks/esp-idf-v6.0"
        idf_tools = "C:/Espressif"
        reg = _all_found_registry(idf_root=idf_root)
        # Simulate that idf provider surfaced IDF_TOOLS_PATH via source too:
        # We store both via a monkey-patched detect that mutates ctx.environ?
        # Simpler: check.py inspects DetectResult.path (IDF_PATH). IDF_TOOLS_PATH
        # is optional; verify only that IDF_PATH is set.
        with mock.patch.dict(providers_mod.REGISTRY, reg, clear=True):
            def _resolve_ws():
                return {"esp32_dir": Path("."), "scripts_dir": Path(".")}
            ensure_for(
                "esp32",
                workspace_root=None,
                resolve_workspace_paths=_resolve_ws,
            )
        # UTF-8 env keys unconditionally set.
        self.assertEqual(os.environ.get("PYTHONUTF8"), "1")
        self.assertEqual(os.environ.get("PYTHONIOENCODING"), "utf-8")
        # IDF_PATH populated from detect result. Compare as Path (str form
        # may use OS-native separators).
        got_idf_path = os.environ.get("IDF_PATH")
        self.assertIsNotNone(got_idf_path)
        self.assertEqual(Path(got_idf_path), Path(idf_root))
        # No host bin prepend: PATH untouched.
        self.assertEqual(os.environ.get("PATH"), "SENTINEL_PATH_ONLY")

    # ---- 4. 'test' profile with emsdk missing → optional, no exit ------------

    def test_test_profile_optional_emsdk_missing_no_exit(self):
        reg = _all_found_registry()
        reg["emsdk"] = _FakeProvider("emsdk", _missing("emsdk not found"))
        reg["node"] = _FakeProvider("node", _missing("node not found"))
        buf = io.StringIO()
        with mock.patch.dict(providers_mod.REGISTRY, reg, clear=True):
            with redirect_stderr(buf):
                # Should NOT raise: emsdk/node are optional for 'test'.
                ensure_for("test", workspace_root=None)
        # Report is not required to be printed on the success path.

    # ---- 5. skip=True → warning to stderr, no probing ------------------------

    def test_skip_bypasses_probes(self):
        reg = _all_found_registry()
        # Even if all fail:
        for cap in reg:
            reg[cap] = _FakeProvider(cap, _missing(f"{cap} missing"))
        buf = io.StringIO()
        with mock.patch.dict(providers_mod.REGISTRY, reg, clear=True):
            with redirect_stderr(buf):
                ensure_for("host", workspace_root=None, skip=True)  # returns
        out = buf.getvalue()
        self.assertIn("skip", out.lower())
        # No provider should have been probed.
        for cap, prov in reg.items():
            self.assertEqual(prov.call_count, 0,
                             f"provider {cap} should not have been probed when skip=True")

    # ---- 6. Missing required workspace path → SystemExit(1) ------------------

    def test_missing_workspace_path_exits(self):
        reg = _all_found_registry()
        buf = io.StringIO()
        with mock.patch.dict(providers_mod.REGISTRY, reg, clear=True):
            with redirect_stderr(buf):
                with self.assertRaises(SystemExit) as cm:
                    ensure_for(
                        "esp32",
                        workspace_root=None,
                        resolve_workspace_paths=lambda: {
                            "esp32_dir": None,
                            "scripts_dir": Path("."),
                        },
                    )
        self.assertEqual(cm.exception.code, 1)
        out = buf.getvalue()
        self.assertIn("esp32_dir", out)
        self.assertIn("Missing required workspaces", out)

    # ---- 7. Probe cache: each provider called exactly once per ensure_for ---

    def test_probe_cache_single_call_per_cap(self):
        # 'wasm' expands via 'host' → 'codegen' so python appears once, gcc once.
        # We assert each provider called exactly once regardless of DAG shape.
        reg = _all_found_registry()
        with mock.patch.dict(providers_mod.REGISTRY, reg, clear=True):
            ensure_for("wasm", workspace_root=None)
        for cap, prov in reg.items():
            # Optional caps might not be probed depending on profile membership;
            # required caps must be probed at least once. We assert <= 1.
            self.assertLessEqual(prov.call_count, 1,
                                 f"{cap} probed more than once ({prov.call_count}) — cache broken")

    # ---- 8. doctor probes everything -----------------------------------------

    def test_doctor_probes_all_capabilities(self):
        reg = _all_found_registry()
        buf = io.StringIO()
        with mock.patch.dict(providers_mod.REGISTRY, reg, clear=True):
            with redirect_stderr(buf):
                # Successful all-green doctor should not raise.
                try:
                    ensure_for("doctor", workspace_root=None)
                except SystemExit as e:
                    self.fail(f"doctor unexpectedly exited: code={e.code}")
        # Every provider probed once.
        for cap, prov in reg.items():
            self.assertEqual(prov.call_count, 1,
                             f"{cap} not probed by doctor (count={prov.call_count})")

    def test_doctor_exits_when_required_cap_missing(self):
        reg = _all_found_registry()
        reg["gcc"] = _FakeProvider("gcc", _missing("gcc missing"))
        buf = io.StringIO()
        with mock.patch.dict(providers_mod.REGISTRY, reg, clear=True):
            with redirect_stderr(buf):
                with self.assertRaises(SystemExit) as cm:
                    ensure_for("doctor", workspace_root=None)
        self.assertEqual(cm.exception.code, 1)


if __name__ == "__main__":
    unittest.main()
