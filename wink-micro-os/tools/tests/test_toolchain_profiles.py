"""Tests for profile DAG expansion."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.toolchain.profiles import (  # noqa: E402
    OPTIONAL_CAPS,
    PROFILES,
    WORKSPACE_DEPS,
    expand_profile,
)


class TestProfileConstants(unittest.TestCase):
    def test_profiles_contains_expected_keys(self):
        for name in ("codegen", "host", "wasm", "test", "esp32", "web"):
            self.assertIn(name, PROFILES)

    def test_workspace_deps_declares_esp32_and_web(self):
        self.assertIn("esp32", WORKSPACE_DEPS)
        self.assertIn("web", WORKSPACE_DEPS)
        self.assertIn("esp32_dir", WORKSPACE_DEPS["esp32"])
        self.assertIn("scripts_dir", WORKSPACE_DEPS["esp32"])
        self.assertIn("frontend_dir", WORKSPACE_DEPS["web"])

    def test_optional_caps_declares_test_and_wasm(self):
        self.assertIn("test", OPTIONAL_CAPS)
        self.assertIn("wasm", OPTIONAL_CAPS)
        self.assertIn("emsdk", OPTIONAL_CAPS["test"])
        self.assertIn("node", OPTIONAL_CAPS["test"])
        self.assertIn("node", OPTIONAL_CAPS["wasm"])


class TestExpandProfile(unittest.TestCase):
    def test_codegen_leaf(self):
        self.assertEqual(expand_profile("codegen"), ["python", "jinja2"])

    def test_host_puts_codegen_deps_first(self):
        # host references codegen + gcc/cmake/make; codegen caps must come first
        result = expand_profile("host")
        self.assertEqual(result, ["python", "jinja2", "gcc", "cmake", "make"])

    def test_wasm_dedupes_transitive_python(self):
        # wasm -> host -> codegen -> python; python must appear once
        result = expand_profile("wasm")
        self.assertEqual(result.count("python"), 1)
        self.assertEqual(result.count("jinja2"), 1)
        # emsdk should be at the end (added after host's expansion)
        self.assertIn("emsdk", result)
        # host caps must precede emsdk
        self.assertLess(result.index("gcc"), result.index("emsdk"))

    def test_test_profile_includes_host_only(self):
        # test profile: just host (emsdk/node are optional, not in required list)
        result = expand_profile("test")
        self.assertIn("python", result)
        self.assertIn("gcc", result)
        # optional caps must NOT be in the required expansion
        self.assertNotIn("emsdk", result)
        self.assertNotIn("node", result)

    def test_esp32_does_not_include_host_tools(self):
        result = expand_profile("esp32")
        self.assertEqual(result, ["python", "powershell", "idf"])
        # esp32 must NOT include host/wasm-only tools
        for forbidden in ("gcc", "cmake", "make", "emsdk", "jinja2"):
            self.assertNotIn(forbidden, result)

    def test_web_profile(self):
        self.assertEqual(expand_profile("web"), ["node"])

    def test_unknown_profile_raises_keyerror(self):
        with self.assertRaises(KeyError):
            expand_profile("nonexistent")

    def test_cycle_detection_raises_valueerror(self):
        # Manually create a cyclic profile map by monkey-patching PROFILES
        # then calling expand_profile with a helper that reads the module.
        import tools.toolchain.profiles as prof_mod

        original = prof_mod.PROFILES
        try:
            prof_mod.PROFILES = {
                "a": ["b"],
                "b": ["a"],
            }
            with self.assertRaises(ValueError):
                prof_mod.expand_profile("a")
        finally:
            prof_mod.PROFILES = original


if __name__ == "__main__":
    unittest.main()
