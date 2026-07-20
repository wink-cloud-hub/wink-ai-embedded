"""Tests for allow_paths / until graded expiry and run_lint orchestration."""
from __future__ import annotations

import os
import sys
import tempfile
import unittest
from datetime import date, timedelta
from pathlib import Path
from unittest import mock

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.allowlist import (  # noqa: E402
    apply_allowlist,
    evaluate_until,
    path_matches_allow,
    resolve_today,
)
from tools.lint.engine.config import LintConfig  # noqa: E402
from tools.lint.engine.models import Finding  # noqa: E402
from tools.lint.engine.runner import run_lint  # noqa: E402


def _finding(**kwargs) -> Finding:
    defaults = dict(
        rule_id="BAL-HDR-NO-PAL",
        severity="error",
        path="bal/include/x.h",
        line=1,
        column=None,
        message="no pal headers",
        snippet='#include "pal/pal_hal.h"',
        help=None,
        refs=("ADR-0023",),
        allowlisted=False,
        rule_source="sdk",
    )
    defaults.update(kwargs)
    return Finding(**defaults)


def _rule_with_allow(until: str | None, path: str = "bal/include/**") -> dict:
    entry: dict = {"path": path, "reason": "test debt"}
    if until is not None:
        entry["until"] = until
    return {
        "id": "BAL-HDR-NO-PAL",
        "in": ["bal_public"],
        "deny": [
            {
                "match": "basename",
                "pattern": r"pal_.*\.h",
                "except_basename": ["pal_log.h"],
                "include_forms": ["quote"],
            }
        ],
        "allow_paths": [entry],
        "message": "no pal headers",
        "severity": "error",
        "immutable": True,
        "refs": ["ADR-0023"],
        "rule_source": "sdk",
    }


class TestEvaluateUntil(unittest.TestCase):
    def test_active_far_future(self):
        today = date(2026, 7, 20)
        status = evaluate_until({"until": "2026-12-31"}, today)
        self.assertEqual(status, "active")

    def test_no_until_is_active(self):
        self.assertEqual(evaluate_until({"path": "x"}, date(2026, 7, 20)), "active")

    def test_expiring_notice_within_30(self):
        today = date(2026, 7, 20)
        status = evaluate_until({"until": (today + timedelta(days=15)).isoformat()}, today)
        self.assertEqual(status, "expiring_notice")

    def test_expiring_soon_within_7(self):
        today = date(2026, 7, 20)
        status = evaluate_until({"until": (today + timedelta(days=3)).isoformat()}, today)
        self.assertEqual(status, "expiring_soon")

    def test_expired(self):
        today = date(2026, 7, 20)
        status = evaluate_until({"until": (today - timedelta(days=1)).isoformat()}, today)
        self.assertEqual(status, "expired")


class TestAllowlistApply(unittest.TestCase):
    def setUp(self):
        self.today = date(2026, 7, 20)

    def test_allow_path_active_suppresses(self):
        until = (self.today + timedelta(days=60)).isoformat()
        cfg = LintConfig(include_rules=[_rule_with_allow(until)])
        findings = [_finding()]
        out = apply_allowlist(findings, cfg, self.today)
        self.assertEqual(len(out), 1)
        self.assertTrue(out[0].allowlisted)
        self.assertEqual(out[0].severity, "error")

    def test_expiring_notice_emits_info(self):
        until = (self.today + timedelta(days=15)).isoformat()
        cfg = LintConfig(include_rules=[_rule_with_allow(until)])
        out = apply_allowlist([_finding()], cfg, self.today)
        self.assertEqual(len(out), 2)
        primary = out[0]
        companion = out[1]
        self.assertTrue(primary.allowlisted)
        self.assertEqual(companion.severity, "info")
        self.assertIn("expir", companion.message.lower())

    def test_expiring_soon_emits_warning(self):
        until = (self.today + timedelta(days=3)).isoformat()
        cfg = LintConfig(include_rules=[_rule_with_allow(until)])
        out = apply_allowlist([_finding()], cfg, self.today)
        self.assertEqual(len(out), 2)
        self.assertTrue(out[0].allowlisted)
        self.assertEqual(out[1].severity, "warning")

    def test_expired_until_still_errors(self):
        until = (self.today - timedelta(days=1)).isoformat()
        cfg = LintConfig(include_rules=[_rule_with_allow(until)])
        out = apply_allowlist([_finding()], cfg, self.today)
        self.assertEqual(len(out), 1)
        self.assertFalse(out[0].allowlisted)
        self.assertIn("allowlist expired", out[0].message.lower())

    def test_path_glob_match(self):
        self.assertTrue(path_matches_allow("bal/include/x.h", "bal/include/**"))
        self.assertFalse(path_matches_allow("bal/src/x.c", "bal/include/**"))


class TestResolveToday(unittest.TestCase):
    def test_today_env_override(self):
        with mock.patch.dict(os.environ, {"WINK_LINT_TODAY": "2027-01-01"}):
            self.assertEqual(resolve_today(None), date(2027, 1, 1))

    def test_explicit_today_wins_over_env(self):
        with mock.patch.dict(os.environ, {"WINK_LINT_TODAY": "2027-01-01"}):
            self.assertEqual(resolve_today(date(2026, 1, 1)), date(2026, 1, 1))


class TestRunLint(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        hdr = self.root / "bal" / "include"
        hdr.mkdir(parents=True)
        (hdr / "x.h").write_text('#include "pal/pal_hal.h"\n', encoding="utf-8")

    def tearDown(self):
        self._tmp.cleanup()

    def test_run_lint_emits_include_finding(self):
        cfg = LintConfig(
            layers={
                "bal_public": {"roots": ["bal/include"], "kind": "public_header"},
            },
            include_rules=[
                {
                    "id": "BAL-HDR-NO-PAL",
                    "in": ["bal_public"],
                    "deny": [
                        {
                            "match": "basename",
                            "pattern": r"pal_.*\.h",
                            "except_basename": ["pal_log.h"],
                            "include_forms": ["quote"],
                        }
                    ],
                    "allow_paths": [],
                    "message": "no pal",
                    "severity": "error",
                    "rule_source": "sdk",
                }
            ],
        )
        findings = run_lint(self.root, cfg, packs=["layering"], today=date(2026, 7, 20))
        errors = [f for f in findings if f.severity == "error" and not f.allowlisted]
        self.assertEqual(len(errors), 1)
        self.assertEqual(errors[0].rule_id, "BAL-HDR-NO-PAL")

    def test_run_lint_allowlist_active(self):
        until = (date(2026, 7, 20) + timedelta(days=60)).isoformat()
        cfg = LintConfig(
            layers={
                "bal_public": {"roots": ["bal/include"], "kind": "public_header"},
            },
            include_rules=[_rule_with_allow(until)],
        )
        findings = run_lint(self.root, cfg, packs=["layering"], today=date(2026, 7, 20))
        errors = [f for f in findings if f.severity == "error" and not f.allowlisted]
        self.assertEqual(errors, [])
        self.assertTrue(any(f.allowlisted for f in findings))


if __name__ == "__main__":
    unittest.main()
