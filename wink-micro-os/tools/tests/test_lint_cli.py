"""Tests for wink lint report formatting and CLI wiring."""
from __future__ import annotations

import os
import subprocess
import sys
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.models import Finding  # noqa: E402
from tools.lint.engine.report import exit_code_for, format_text  # noqa: E402

WINK_PY = SDK / "tools" / "wink.py"


class TestTextReport(unittest.TestCase):
    def test_text_report_has_location(self):
        f = Finding(
            rule_id="DAL-HDR-NO-HAL",
            severity="error",
            path="dal/include/actuator/dal_motor.h",
            line=7,
            column=1,
            message="DAL public headers must not leak HAL",
            snippet='#include "pal_hal.h"',
            help="keep HAL includes in .c only",
            refs=("ADR-0043",),
            allowlisted=False,
            rule_source="sdk",
        )
        out = format_text([f])
        self.assertIn("dal/include/actuator/dal_motor.h:7", out)
        self.assertTrue(
            "error[" in out.lower() or "DAL-HDR-NO-HAL" in out
        )
        self.assertIn("-->", out)

    def test_path_only_location_omits_line(self):
        f = Finding(
            rule_id="BAL-NAME-1",
            severity="error",
            path="bal/include/motion/motion_controller.h",
            line=None,
            column=None,
            message="forbidden name",
            snippet=None,
            help=None,
            refs=(),
            allowlisted=False,
            rule_source="sdk",
        )
        out = format_text([f])
        self.assertIn("--> bal/include/motion/motion_controller.h", out)
        self.assertNotIn(":None", out)

    def test_exit_code_error_fails(self):
        f = Finding(
            rule_id="X",
            severity="error",
            path="a.h",
            line=1,
            column=None,
            message="m",
            snippet=None,
            help=None,
            refs=(),
        )
        self.assertEqual(exit_code_for([f], strict=False), 1)

    def test_exit_code_warning_strict(self):
        f = Finding(
            rule_id="X",
            severity="warning",
            path="a.h",
            line=1,
            column=None,
            message="m",
            snippet=None,
            help=None,
            refs=(),
        )
        self.assertEqual(exit_code_for([f], strict=False), 0)
        self.assertEqual(exit_code_for([f], strict=True), 1)

    def test_allowlisted_error_does_not_fail(self):
        f = Finding(
            rule_id="X",
            severity="error",
            path="a.h",
            line=1,
            column=None,
            message="m",
            snippet=None,
            help=None,
            refs=(),
            allowlisted=True,
        )
        self.assertEqual(exit_code_for([f], strict=False), 0)


class TestLintCliSmoke(unittest.TestCase):
    def test_lint_help(self):
        env = os.environ.copy()
        env["PYTHONPATH"] = str(SDK) + (
            os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else ""
        )
        result = subprocess.run(
            [sys.executable, str(WINK_PY), "lint", "--help"],
            capture_output=True,
            text=True,
            timeout=30,
            env=env,
            encoding="utf-8",
            errors="replace",
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("--pack", result.stdout)
        self.assertIn("--today", result.stdout)

    def test_lint_pack_layering_runs_without_toolchain(self):
        env = os.environ.copy()
        env["PYTHONPATH"] = str(SDK) + (
            os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else ""
        )
        result = subprocess.run(
            [
                sys.executable,
                str(WINK_PY),
                "lint",
                "--pack",
                "layering",
                "--root",
                str(SDK),
            ],
            capture_output=True,
            text=True,
            timeout=120,
            env=env,
            encoding="utf-8",
            errors="replace",
        )
        # May exit 0 or 1 depending on tree debt; must not crash or ask toolchain.
        self.assertNotIn("ensure_for", result.stderr)
        self.assertNotIn("ModuleNotFoundError", result.stderr + result.stdout)
        self.assertIn(result.returncode, (0, 1))


if __name__ == "__main__":
    unittest.main()
