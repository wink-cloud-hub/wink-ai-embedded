"""Tests for lint lexical preprocessing (comments, strings, includes)."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.lexer import strip_comments_and_strings  # noqa: E402
from tools.lint.engine.includes import extract_includes  # noqa: E402


class TestLexer(unittest.TestCase):
    def test_lexer_strips_line_comment_keeps_line_no(self):
        out = strip_comments_and_strings('a // #include "pal_hal.h"\nb\n')
        self.assertNotIn('#include "pal_hal.h"', out)
        self.assertEqual(out.count("\n"), 2)

    def test_lexer_strips_block_comment_multiline(self):
        src = '/* #include "pal_hal.h"\n */\n#include "wink_status.h"\n'
        out = strip_comments_and_strings(src)
        self.assertNotIn('"pal_hal.h"', out)
        self.assertIn('"wink_status.h"', out)

    def test_extract_include_with_spaces_and_continuation(self):
        text = '#  include \\\n "pal_hal.h"\n'
        incs = extract_includes(text)
        self.assertTrue(incs)
        self.assertEqual(incs[0][1], "pal_hal.h")


if __name__ == "__main__":
    unittest.main()
