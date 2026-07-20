"""Tests for lint layer path classifier."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.engine.classify import classify_file  # noqa: E402

LAYERS = {
    "bal_public": {"roots": ["bal/include"], "kind": "public_header"},
    "bal_src": {"roots": ["bal/src"], "kind": "source"},
}


class TestClassifyFile(unittest.TestCase):
    def test_classify_bal_public_header(self):
        self.assertEqual(
            classify_file("bal/include/input/wink_button_events.h", LAYERS, []),
            ("bal_public", "public_header"),
        )

    def test_classify_longest_prefix_wins(self):
        layers = {
            "bal_public": {"roots": ["bal/include"], "kind": "public_header"},
            "bal_math": {"roots": ["bal/include/math"], "kind": "public_header"},
        }
        self.assertEqual(
            classify_file("bal/include/math/wink_pid.h", layers, [])[0],
            "bal_math",
        )

    def test_classify_ignore_scope_classify(self):
        ignore = [{"path": "third_party/**", "scope": ["classify"]}]
        self.assertIsNone(classify_file("third_party/foo/bar.h", LAYERS, ignore))


if __name__ == "__main__":
    unittest.main()
