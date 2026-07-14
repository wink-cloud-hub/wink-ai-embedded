"""Contract tests for toolchain core types and Provider ABC."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.toolchain.providers.base import Provider  # noqa: E402
from tools.toolchain.types import DetectResult, UnsupportedError  # noqa: E402


class FakeCtx:
    pass


class TestProviderContract(unittest.TestCase):
    def test_install_default_unsupported(self):
        class P(Provider):
            id = "fake"

            def detect(self, ctx):
                return DetectResult(False, None, None, "x", None)

            def hint(self, ctx):
                return "hint"

        with self.assertRaises(UnsupportedError):
            P().install(FakeCtx())


if __name__ == "__main__":
    unittest.main()
