"""Tests for api_surface pack."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.config import LintConfig  # noqa: E402
from tools.lint.packs.api_surface import check_api_surface  # noqa: E402


def _malloc_rule(**extra) -> dict:
    rule = {
        "id": "NO-MALLOC-HOTPATH",
        "in": ["bal_src", "dal_src"],
        "deny_regex": [{"pattern": r"\b(malloc|calloc|realloc|free)\s*\("}],
        "context": {
            "strip_comments": True,
            "strip_strings": True,
            "scope_by_kind": {"source": "full"},
        },
        "except_regex": [r"\bfree_slot\s*\(", r"\bfree_\w+_slot\s*\("],
        "message": "hotpath malloc",
        "severity": "warning",
        "rule_source": "sdk",
    }
    rule.update(extra)
    return rule


def _ops_rule() -> dict:
    return {
        "id": "NO-OPS-VTABLE",
        "in": ["bal_public", "dal_public", "bal_src", "dal_src"],
        "deny_regex": [
            {"pattern": r"\b\w+_ops\s*\{"},
            {"pattern": r"\bcontainer_of\s*\("},
        ],
        "context": {
            "strip_comments": True,
            "strip_strings": True,
            "scope_by_kind": {
                "public_header": "declarations_only",
                "source": "full",
            },
        },
        "except_regex": [],
        "message": "no ops",
        "severity": "error",
        "rule_source": "sdk",
    }


class TestApiSurface(unittest.TestCase):
    def test_no_malloc_ignores_comments(self):
        cfg = LintConfig(api_rules=[_malloc_rule()])
        text = "// malloc(64) is forbidden on hot path\nint x;\n"
        findings = check_api_surface("bal/src/x.c", text, "bal_src", "source", cfg)
        self.assertEqual(findings, [])

    def test_no_malloc_ignores_string_literal(self):
        cfg = LintConfig(api_rules=[_malloc_rule()])
        text = 'const char* s = "malloc";\n'
        findings = check_api_surface("bal/src/x.c", text, "bal_src", "source", cfg)
        self.assertEqual(findings, [])

    def test_no_malloc_ignores_named_free_slot(self):
        cfg = LintConfig(api_rules=[_malloc_rule()])
        text = "static void free_slot(void *p) { (void)p; }\n"
        findings = check_api_surface("bal/src/x.c", text, "bal_src", "source", cfg)
        self.assertEqual(findings, [])

    def test_no_malloc_detects_real_call(self):
        cfg = LintConfig(api_rules=[_malloc_rule()])
        text = "void *p = malloc(16);\n"
        findings = check_api_surface("bal/src/x.c", text, "bal_src", "source", cfg)
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].rule_id, "NO-MALLOC-HOTPATH")
        self.assertEqual(findings[0].line, 1)

    def test_no_ops_declarations_only_in_public_header(self):
        cfg = LintConfig(api_rules=[_ops_rule()])
        text = (
            "struct foo_ops {\n"
            "  int (*run)(void);\n"
            "};\n"
            "static inline void helper(void) {\n"
            "  struct bar_ops local = {0};\n"
            "  (void)local;\n"
            "}\n"
        )
        findings = check_api_surface(
            "dal/include/x.h", text, "dal_public", "public_header", cfg
        )
        self.assertTrue(any(f.rule_id == "NO-OPS-VTABLE" for f in findings))
        # Struct foo_ops { should fire; local bar_ops inside function should not.
        lines = {f.line for f in findings if f.rule_id == "NO-OPS-VTABLE"}
        self.assertIn(1, lines)
        self.assertNotIn(5, lines)


if __name__ == "__main__":
    unittest.main()
