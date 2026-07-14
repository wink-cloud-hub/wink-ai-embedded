"""Contract tests for the collect-all toolchain missing-dep report."""
from __future__ import annotations

import io
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

SDK = Path(__file__).resolve().parents[1].parent  # wink-micro-os
if str(SDK) not in sys.path:
    sys.path.insert(0, str(SDK))

from tools.toolchain.report import (  # noqa: E402
    ReportItem,
    exit_for_report,
    render_report,
)


class TestRenderReport(unittest.TestCase):
    def test_empty_all_clear(self):
        buf = io.StringIO()
        text = render_report([], file=buf)
        # returned string equals what was written
        self.assertEqual(text, buf.getvalue())
        # all-clear phrasing
        lowered = text.lower()
        self.assertTrue(
            "all" in lowered and "satisfied" in lowered,
            f"expected all-clear phrasing, got: {text!r}",
        )

    def test_two_required_one_optional(self):
        items = [
            ReportItem(
                kind="required_tool",
                id="gcc",
                message="not found on PATH",
                hint="install via winget install ...",
                min_version=">=14",
            ),
            ReportItem(
                kind="required_workspace",
                id="wink-micro-app",
                message="workspace missing",
                hint="clone the workspace next to wink-ai-embedded",
            ),
            ReportItem(
                kind="optional",
                id="clang-format",
                message="not found",
                hint="optional - install for formatting",
            ),
        ]
        buf = io.StringIO()
        text = render_report(items, file=buf)

        # Blocking items use "✗"
        self.assertIn("✗", text)
        gcc_lines = [l for l in text.splitlines() if "gcc" in l]
        self.assertTrue(any(l.lstrip().startswith("✗") for l in gcc_lines))
        ws_lines = [l for l in text.splitlines() if "wink-micro-app" in l]
        self.assertTrue(any(l.lstrip().startswith("✗") for l in ws_lines))

        # Optional uses "!"
        opt_lines = [l for l in text.splitlines() if "clang-format" in l]
        self.assertTrue(any(l.lstrip().startswith("!") for l in opt_lines))

        # Summary: 2 errors, 1 warning (singular "warning")
        self.assertIn("2 error", text)
        self.assertIn("1 warning", text)

        # Hint should be included
        self.assertIn("winget install", text)

        # min_version surfaced for gcc
        self.assertIn(">=14", text)

    def test_summary_singular_plural(self):
        items = [
            ReportItem(kind="required_tool", id="gcc", message="missing"),
            ReportItem(kind="optional", id="a", message="missing"),
            ReportItem(kind="optional", id="b", message="missing"),
        ]
        text = render_report(items, file=io.StringIO())
        # 1 error (singular), 2 warnings (plural)
        self.assertRegex(text, r"1 error(?!s)")
        self.assertIn("2 warnings", text)

    def test_idf_notice(self):
        items = [
            ReportItem(
                kind="required_tool",
                id="idf",
                message="ESP-IDF not detected",
                hint="see preinstall.md",
            ),
        ]
        text = render_report(items, file=io.StringIO())
        self.assertIn("never auto-installed", text)
        # Mentions EIM / preinstall
        self.assertTrue("EIM" in text or "preinstall" in text)

    def test_found_version_below_floor(self):
        items = [
            ReportItem(
                kind="required_tool",
                id="gcc",
                message="version below required floor",
                found_path=Path("C:/bin/gcc.exe"),
                found_version="12.2.0",
                min_version=">=14",
            ),
        ]
        text = render_report(items, file=io.StringIO())
        self.assertIn("12.2.0", text)
        self.assertIn(">=14", text)

    def test_render_writes_to_file(self):
        buf = io.StringIO()
        items = [ReportItem(kind="required_tool", id="x", message="missing")]
        text = render_report(items, file=buf)
        self.assertEqual(text, buf.getvalue())


class TestExitForReport(unittest.TestCase):
    def test_empty_exits_zero(self):
        with patch("sys.stderr", new_callable=io.StringIO):
            with self.assertRaises(SystemExit) as cm:
                exit_for_report([])
        self.assertEqual(cm.exception.code, 0)

    def test_only_optional_exits_zero(self):
        items = [ReportItem(kind="optional", id="a", message="missing")]
        with patch("sys.stderr", new_callable=io.StringIO):
            with self.assertRaises(SystemExit) as cm:
                exit_for_report(items)
        self.assertEqual(cm.exception.code, 0)

    def test_required_tool_exits_one(self):
        items = [ReportItem(kind="required_tool", id="gcc", message="missing")]
        with patch("sys.stderr", new_callable=io.StringIO):
            with self.assertRaises(SystemExit) as cm:
                exit_for_report(items)
        self.assertEqual(cm.exception.code, 1)

    def test_required_workspace_exits_one(self):
        items = [
            ReportItem(kind="required_workspace", id="wink-x", message="missing")
        ]
        with patch("sys.stderr", new_callable=io.StringIO):
            with self.assertRaises(SystemExit) as cm:
                exit_for_report(items)
        self.assertEqual(cm.exception.code, 1)


if __name__ == "__main__":
    unittest.main()
