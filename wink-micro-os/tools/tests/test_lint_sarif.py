"""Tests for SARIF output and --explain."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.models import Finding  # noqa: E402
from tools.lint.engine.report import format_sarif  # noqa: E402

WINK_PY = SDK / "tools" / "wink.py"


class TestSarif(unittest.TestCase):
    def test_sarif_has_physical_location_region(self):
        f = Finding(
            rule_id="DAL-HDR-NO-HAL",
            severity="error",
            path="dal/include/actuator/dal_motor.h",
            line=7,
            column=1,
            message="no HAL",
            snippet='#include "pal_hal.h"',
            help=None,
            refs=("ADR-0043",),
        )
        doc = json.loads(format_sarif([f]))
        self.assertEqual(doc["version"], "2.1.0")
        result = doc["runs"][0]["results"][0]
        loc = result["locations"][0]["physicalLocation"]
        self.assertEqual(loc["artifactLocation"]["uri"], "dal/include/actuator/dal_motor.h")
        self.assertEqual(loc["region"]["startLine"], 7)

    def test_sarif_path_only_rule_marks_locator(self):
        f = Finding(
            rule_id="BAL-NAME-1",
            severity="error",
            path="bal/include/motion/motion_helper.h",
            line=None,
            column=None,
            message="bad name",
            snippet=None,
            help=None,
            refs=(),
        )
        doc = json.loads(format_sarif([f]))
        result = doc["runs"][0]["results"][0]
        self.assertEqual(result["properties"]["locator"], "filename")
        self.assertEqual(
            result["locations"][0]["physicalLocation"]["region"]["startLine"], 1
        )


class TestExplain(unittest.TestCase):
    def test_explain_prints_template_and_exits_zero(self):
        env = os.environ.copy()
        env["PYTHONPATH"] = str(SDK) + (
            os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else ""
        )
        result = subprocess.run(
            [
                sys.executable,
                str(WINK_PY),
                "lint",
                "--root",
                str(SDK),
                "--explain",
                "DAL-HDR-NO-HAL",
            ],
            capture_output=True,
            text=True,
            timeout=30,
            env=env,
            encoding="utf-8",
            errors="replace",
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("Rule: DAL-HDR-NO-HAL", result.stdout)
        self.assertIn("Allowlist policy:", result.stdout)
        self.assertIn("Active allowlist:", result.stdout)


if __name__ == "__main__":
    unittest.main()
