#!/usr/bin/env python3
"""Static assertion test ensuring tools/cli/ commands never import tools.wink directly."""
import re
from pathlib import Path
import unittest


class TestCLINoReverseImport(unittest.TestCase):
    def test_no_command_imports_wink_module(self):
        cli_dir = Path(__file__).resolve().parent.parent / "cli"
        pattern = re.compile(r"^\s*(from|import)\s+tools\.wink\b", re.MULTILINE)

        for py_file in cli_dir.rglob("*.py"):
            content = py_file.read_text(encoding="utf-8")
            match = pattern.search(content)
            self.assertIsNone(
                match,
                f"File {py_file.name} contains reverse import of 'tools.wink' at: {match.group(0) if match else ''}",
            )


if __name__ == "__main__":
    unittest.main()
