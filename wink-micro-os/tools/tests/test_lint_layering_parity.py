"""Unit tests for layering rule pack coverage (Task 6)."""
from __future__ import annotations

import sys
import unittest
from datetime import date
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.config import load_configs  # noqa: E402
from tools.lint.engine.runner import run_lint  # noqa: E402

RULES = SDK / "tools" / "lint" / "rules" / "layering.yaml"
BAD = SDK / "tools" / "lint" / "testdata" / "layering_rules" / "bad"


class TestLayeringRules(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cfg = load_configs([RULES])

    def test_bal_name_1_helper_filename(self):
        findings = run_lint(
            BAD,
            self.cfg,
            packs=["layering"],
            paths=[BAD / "bal/include/motion/motion_helper.h"],
            today=date(2026, 7, 20),
        )
        ids = {f.rule_id for f in findings if not f.allowlisted}
        self.assertIn("BAL-NAME-1", ids)
        f = next(x for x in findings if x.rule_id == "BAL-NAME-1")
        self.assertIsNone(f.line)

    def test_bal_math_1_first_hit_line(self):
        findings = run_lint(
            BAD,
            self.cfg,
            packs=["layering"],
            paths=[BAD / "bal/include/math/leaky.h"],
            today=date(2026, 7, 20),
        )
        f = next(x for x in findings if x.rule_id == "BAL-MATH-1")
        self.assertEqual(f.line, 3)

    def test_bal_name_2_sonar(self):
        findings = run_lint(
            BAD,
            self.cfg,
            packs=["layering"],
            paths=[BAD / "bal/include/motion/sonar_probe.h"],
            today=date(2026, 7, 20),
        )
        f = next(x for x in findings if x.rule_id == "BAL-NAME-2")
        self.assertEqual(f.line, 3)

    def test_dal_hdr_no_hal_error(self):
        findings = run_lint(
            BAD,
            self.cfg,
            packs=["layering"],
            paths=[BAD / "dal/include/actuator/dal_leaky.h"],
            today=date(2026, 7, 20),
        )
        f = next(x for x in findings if x.rule_id == "DAL-HDR-NO-HAL")
        self.assertEqual(f.severity, "error")
        self.assertEqual(f.line, 3)
        self.assertFalse(f.allowlisted)


if __name__ == "__main__":
    unittest.main()
