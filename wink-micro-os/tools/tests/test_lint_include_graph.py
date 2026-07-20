"""Tests for include_graph lint pack."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.config import LintConfig  # noqa: E402
from tools.lint.packs.include_graph import check_includes  # noqa: E402


def _bal_hdr_rule_cfg(**deny_overrides) -> LintConfig:
    deny = {
        "match": "basename",
        "pattern": r"pal_.*\.h",
        "except_basename": ["pal_log.h"],
        "include_forms": ["quote"],
    }
    deny.update(deny_overrides)
    return LintConfig(
        include_rules=[
            {
                "id": "BAL-HDR-NO-PAL",
                "in": ["bal_public"],
                "deny": [deny],
                "message": (
                    "ADR-0023 §8 red-line: bal/include/**/*.h must not include "
                    "pal_*.h (pal_log.h only). Move pal_ usage to the .c file."
                ),
                "severity": "error",
                "immutable": True,
                "refs": ["ADR-0023"],
                "rule_source": "sdk",
            }
        ]
    )


class TestIncludeGraph(unittest.TestCase):
    def test_bal_public_denies_pal_hal_basename(self):
        cfg = _bal_hdr_rule_cfg()
        text = '#include "pal/pal_hal.h"\n'
        findings = check_includes("bal/include/x.h", text, "bal_public", cfg)
        self.assertEqual(len(findings), 1)
        f = findings[0]
        self.assertEqual(f.rule_id, "BAL-HDR-NO-PAL")
        self.assertEqual(f.line, 1)
        self.assertEqual(f.path, "bal/include/x.h")
        self.assertEqual(f.rule_source, "sdk")
        self.assertTrue(f.fingerprint)

    def test_bal_public_allows_pal_log_via_except_basename(self):
        cfg = _bal_hdr_rule_cfg()
        text = '#include "pal_log.h"\n'
        findings = check_includes("bal/include/x.h", text, "bal_public", cfg)
        self.assertEqual(findings, [])

    def test_include_forms_quote_only_skips_angle(self):
        cfg = _bal_hdr_rule_cfg(include_forms=["quote"])
        text = "#include <pal_hal.h>\n"
        findings = check_includes("bal/include/x.h", text, "bal_public", cfg)
        self.assertEqual(findings, [])

    def test_testdata_bad_fixture_violates(self):
        bad = SDK / "tools/lint/testdata/bal_hdr_bad/bal/include/x.h"
        text = bad.read_text(encoding="utf-8")
        cfg = _bal_hdr_rule_cfg(include_forms=["quote", "angle"])
        findings = check_includes("bal/include/x.h", text, "bal_public", cfg)
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].rule_id, "BAL-HDR-NO-PAL")

    def test_testdata_ok_fixture_clean(self):
        ok = SDK / "tools/lint/testdata/bal_hdr_ok/bal/include/x.h"
        text = ok.read_text(encoding="utf-8")
        cfg = _bal_hdr_rule_cfg(include_forms=["quote", "angle"])
        findings = check_includes("bal/include/x.h", text, "bal_public", cfg)
        self.assertEqual(findings, [])


if __name__ == "__main__":
    unittest.main()
