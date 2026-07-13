"""Tests for wink.py CLI wiring (Task 9).

We avoid launching real builds; instead we import the module, monkey-patch
the provider REGISTRY where needed, and drive helpers like
``_probe_all_for_setup`` / ``_handle_setup_set`` directly.

A single subprocess smoke test verifies ``wink.py doctor`` at least gets to
its own report renderer without ImportError.
"""
from __future__ import annotations

import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))
if str(SDK / "tools") not in sys.path:
    sys.path.insert(0, str(SDK / "tools"))

REPO_ROOT = SDK.parent
WINK_PY = SDK / "tools" / "wink.py"

# Import the module under test. wink.py runs top-level side effects (sets
# os.environ["WINK_SDK_PATH"] etc.); that's fine for tests — we just don't
# want tests to depend on cwd having a workspace.
import importlib.util  # noqa: E402

spec = importlib.util.spec_from_file_location("wink_cli_under_test", WINK_PY)
wink = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wink)  # type: ignore[union-attr]

from tools.toolchain import providers as providers_mod  # noqa: E402
from tools.toolchain.providers.base import Provider  # noqa: E402
from tools.toolchain.types import DetectResult  # noqa: E402


# ── Helpers ────────────────────────────────────────────────────────────


class _FakeProvider(Provider):
    """Provider stub with a scripted detect() outcome."""

    def __init__(self, cap_id: str, result: DetectResult, hint_text: str = "install me"):
        self.id = cap_id
        self._result = result
        self._hint = hint_text

    def detect(self, ctx):
        return self._result

    def hint(self, ctx):
        return self._hint


def _found(path: str = "C:\\bin\\tool.exe", version: str = "1.2.3") -> DetectResult:
    return DetectResult(
        found=True, path=Path(path), version=version, reason=None, source="test",
    )


def _missing(reason: str = "not found") -> DetectResult:
    return DetectResult(found=False, path=None, version=None, reason=reason, source=None)


# ── Static-content tests (no execution) ───────────────────────────────


class TestWinkPyStaticShape(unittest.TestCase):
    """Read the source to enforce shape invariants without running it."""

    def test_no_winlibs_hardcode(self):
        """Task 9 mandates the WinLibs hardcode block is deleted."""
        src = WINK_PY.read_text(encoding="utf-8")
        self.assertNotIn("WinLibs", src)
        self.assertNotIn("BrechtSanders", src)
        self.assertNotIn("mingw_bin", src)

    def test_no_hardcoded_machine_paths(self):
        """The user's home / AppData paths must not leak into the source."""
        src = WINK_PY.read_text(encoding="utf-8")
        # A few well-known offender substrings that used to appear here.
        for needle in (
            r"C:\Users\77174\AppData",
            r"D:\software\embedded\emsdk",
        ):
            self.assertNotIn(
                needle, src, f"hardcoded machine path leaked back into wink.py: {needle}",
            )

    def test_help_lists_doctor_and_setup(self):
        """Argparse should register the two new subcommands."""
        parser = wink._build_parser()
        # Enumerate subparser choices from the subparsers action.
        subparser_actions = [
            a for a in parser._actions if isinstance(a, __import__("argparse")._SubParsersAction)
        ]
        self.assertEqual(len(subparser_actions), 1)
        choices = set(subparser_actions[0].choices.keys())
        self.assertIn("doctor", choices)
        self.assertIn("setup", choices)
        # And the original commands still exist.
        for cmd in ("gen", "build", "esp32", "web", "test"):
            self.assertIn(cmd, choices)

    def test_global_skip_flag_present(self):
        parser = wink._build_parser()
        # Flag is accepted in both positions; per-subcommand its .dest may
        # be shadowed by the subparser's default (argparse quirk with
        # parents=), which main() compensates for via an argv pre-pass.
        args = parser.parse_args(["doctor", "--skip-toolchain-check"])
        self.assertTrue(args.skip_toolchain_check)
        # And it also parses (without error) when placed before the subcommand;
        # main() does the OR compensation there.
        args2 = parser.parse_args(["--skip-toolchain-check", "doctor"])
        self.assertEqual(args2.command, "doctor")


# ── Handler-level tests (no subprocess) ────────────────────────────────


class TestSetupNoArgs(unittest.TestCase):
    """`wink setup` with no args prints a table and never exits."""

    def test_prints_table_with_all_caps(self):
        # Swap REGISTRY with a small fake set.
        fake_registry = {
            "python": _FakeProvider("python", _found("py.exe", "3.11.0"), "install python"),
            "gcc":    _FakeProvider("gcc", _missing("no gcc"), "install gcc"),
        }
        with mock.patch.object(providers_mod, "REGISTRY", fake_registry):
            buf = io.StringIO()
            with redirect_stdout(buf):
                wink._handle_setup_noargs()
        out = buf.getvalue()
        self.assertIn("Wink toolchain resolution:", out)
        self.assertIn("python", out)
        self.assertIn("3.11.0", out)
        self.assertIn("gcc", out)
        self.assertIn("not detected", out)
        # Config files section is always printed.
        self.assertIn("Config files:", out)


class TestSetupSetValidation(unittest.TestCase):
    """`wink setup --set` validates before writing."""

    def test_bad_kv_format_exits(self):
        with self.assertRaises(SystemExit) as cm:
            wink._handle_setup_set("no-equals-sign", workspace=False)
        self.assertEqual(cm.exception.code, 2)

    def test_empty_value_exits(self):
        with self.assertRaises(SystemExit) as cm:
            wink._handle_setup_set("gcc=", workspace=False)
        self.assertEqual(cm.exception.code, 2)

    def test_unknown_key_exits(self):
        with self.assertRaises(SystemExit) as cm:
            wink._handle_setup_set("bogus=x", workspace=False)
        self.assertEqual(cm.exception.code, 2)

    def test_valid_write_calls_save_user_path(self):
        """When provider.detect() returns found, we call save_user_path."""
        fake_registry = {
            "gcc": _FakeProvider("gcc", _found("gcc.exe", "14.2.0")),
        }
        # Point HOME at a temp dir so save_user_path writes to a sandbox.
        with tempfile.TemporaryDirectory() as tmp:
            with mock.patch.object(providers_mod, "REGISTRY", fake_registry), \
                 mock.patch.dict(os.environ, {"HOME": tmp, "USERPROFILE": tmp}):
                buf = io.StringIO()
                with redirect_stdout(buf):
                    wink._handle_setup_set(r"gcc=C:\some\gcc.exe", workspace=False)
                out = buf.getvalue()
                self.assertIn("wrote gcc=", out)
                # The user config file should now exist.
                cfg = Path(tmp) / ".wink" / "tools.json"
                self.assertTrue(cfg.exists())
                data = json.loads(cfg.read_text(encoding="utf-8"))
                self.assertEqual(data["paths"]["gcc"], r"C:\some\gcc.exe")

    def test_invalid_detect_rejects_write(self):
        """When provider.detect() returns not-found, config is NOT written."""
        fake_registry = {
            "gcc": _FakeProvider("gcc", _missing("bad binary")),
        }
        with tempfile.TemporaryDirectory() as tmp:
            with mock.patch.object(providers_mod, "REGISTRY", fake_registry), \
                 mock.patch.dict(os.environ, {"HOME": tmp, "USERPROFILE": tmp}):
                buf = io.StringIO()
                with self.assertRaises(SystemExit) as cm, redirect_stderr(buf):
                    wink._handle_setup_set(r"gcc=C:\bogus.exe", workspace=False)
                self.assertEqual(cm.exception.code, 1)
                self.assertIn("validation failed", buf.getvalue())
                # No config file was created.
                cfg = Path(tmp) / ".wink" / "tools.json"
                self.assertFalse(cfg.exists())


class TestSetupInstallStub(unittest.TestCase):
    """`wink setup --install` is a phase-B stub."""

    def test_idf_prints_never_auto_install(self):
        buf = io.StringIO()
        with redirect_stdout(buf):
            wink._handle_setup_install("idf")
        out = buf.getvalue()
        self.assertIn("ESP-IDF is never auto-installed", out)
        self.assertIn("ADR-0030", out)

    def test_unknown_cap_exits(self):
        with self.assertRaises(SystemExit) as cm:
            wink._handle_setup_install("bogus")
        self.assertEqual(cm.exception.code, 2)

    def test_known_cap_prints_hint(self):
        buf = io.StringIO()
        with redirect_stdout(buf):
            wink._handle_setup_install("gcc")
        out = buf.getvalue()
        self.assertIn("phase b", out.lower())


class TestWorkspacePathsCallback(unittest.TestCase):
    def test_callback_returns_dict_with_expected_keys(self):
        cb = wink._make_workspace_paths_callback(["sdk_dir", "esp32_dir"])
        result = cb()
        self.assertEqual(set(result), {"sdk_dir", "esp32_dir"})
        # sdk_dir should always resolve (it's this repo).
        self.assertIsNotNone(result["sdk_dir"])


# ── Subprocess smoke test ─────────────────────────────────────────────


class TestSubprocessSmoke(unittest.TestCase):
    """A single subprocess check that `python wink.py doctor` at least runs.

    Exit code can be 0 (all good) or 1 (missing tool) — either is fine.
    An ImportError or unrelated crash is not.
    """

    def test_doctor_does_not_crash(self):
        env = os.environ.copy()
        env["PYTHONPATH"] = str(SDK) + (os.pathsep + env.get("PYTHONPATH", "") if env.get("PYTHONPATH") else "")
        result = subprocess.run(
            [sys.executable, str(WINK_PY), "doctor"],
            capture_output=True, text=True, timeout=60, env=env,
        )
        # Accept 0 or 1; reject 2 (argparse error) / any other unexpected code.
        self.assertIn(result.returncode, (0, 1),
                      f"unexpected exit={result.returncode}\nstdout=\n{result.stdout}\nstderr=\n{result.stderr}")
        # No ImportError anywhere.
        combined = result.stdout + result.stderr
        self.assertNotIn("ImportError", combined)
        self.assertNotIn("ModuleNotFoundError", combined)
        # Doctor renders the report header.
        self.assertIn("Toolchain status", combined)

    def test_help_lists_subcommands(self):
        env = os.environ.copy()
        env["PYTHONPATH"] = str(SDK) + (os.pathsep + env.get("PYTHONPATH", "") if env.get("PYTHONPATH") else "")
        result = subprocess.run(
            [sys.executable, str(WINK_PY), "--help"],
            capture_output=True, text=True, timeout=30, env=env,
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("doctor", result.stdout)
        self.assertIn("setup", result.stdout)
        self.assertIn("--skip-toolchain-check", result.stdout)


if __name__ == "__main__":
    unittest.main()
