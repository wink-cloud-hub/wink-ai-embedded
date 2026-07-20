"""Tests for lint Finding model."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.models import Finding  # noqa: E402


class TestFinding(unittest.TestCase):
    def test_finding_allows_none_line_for_path_only(self):
        f = Finding(
            rule_id="BAL-NAME-1",
            severity="error",
            path="bal/include/x_helper.h",
            line=None,
            column=None,
            message="m",
            snippet=None,
            help=None,
            refs=("ADR-0038",),
            allowlisted=False,
            rule_source="sdk",
        )
        self.assertIsNone(f.line)
        self.assertTrue(f.fingerprint)  # 非空

    def test_finding_fingerprint_stable_under_whitespace(self):
        f1 = Finding(
            rule_id="X",
            severity="error",
            path="a.h",
            line=7,
            column=1,
            message="m",
            snippet='  #include  "pal_hal.h"  ',
            help=None,
            refs=(),
            allowlisted=False,
            rule_source="sdk",
        )
        f2 = Finding(
            rule_id="X",
            severity="error",
            path="a.h",
            line=7,
            column=1,
            message="m",
            snippet='#include "pal_hal.h"',
            help=None,
            refs=(),
            allowlisted=False,
            rule_source="sdk",
        )
        self.assertEqual(f1.fingerprint, f2.fingerprint)


if __name__ == "__main__":
    unittest.main()
