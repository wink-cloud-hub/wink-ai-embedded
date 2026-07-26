#!/usr/bin/env python3
"""Unit tests ensuring CLI registry initialization does not trigger heavy imports."""
import sys
import unittest


class TestCLILazyImport(unittest.TestCase):
    def test_registry_import_does_not_pull_heavy_deps(self):
        # Heavy dependencies that must remain lazy until command run()
        heavy_deps = ["yaml", "jinja2"]
        for dep in heavy_deps:
            self.assertNotIn(dep, sys.modules, f"Heavy dependency {dep!r} was loaded prematurely")


if __name__ == "__main__":
    unittest.main()
