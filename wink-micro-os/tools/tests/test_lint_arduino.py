"""Smoke test for Arduino isolation pack adapter."""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.lint.packs.legacy_arduino import check_arduino_isolation  # noqa: E402


class TestLegacyArduino(unittest.TestCase):
    def test_clean_sdk_has_no_arduino_leaks(self):
        findings = check_arduino_isolation(SDK)
        self.assertEqual(findings, [])

    def test_detects_arduino_include(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            pal = root / "pal"
            pal.mkdir()
            bad = pal / "leak.c"
            bad.write_text('#include "Arduino.h"\nvoid f(void) {}\n', encoding="utf-8")
            findings = check_arduino_isolation(root)
            self.assertEqual(len(findings), 1)
            self.assertEqual(findings[0].rule_id, "ARDUINO-ISOLATION")
            self.assertEqual(findings[0].line, 1)


if __name__ == "__main__":
    unittest.main()
