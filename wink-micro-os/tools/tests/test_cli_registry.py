#!/usr/bin/env python3
"""Unit tests for CommandRegistry in tools/cli/registry.py."""
import unittest

from tools.cli.base import CommandBase
from tools.cli.registry import CommandRegistry, register_default_commands


class TestCLICommandRegistry(unittest.TestCase):
    def test_registered_names_contains_defaults(self):
        names = CommandRegistry.names()
        expected = ["build", "doctor", "esp32", "gen", "lint", "setup", "test", "web"]
        for name in expected:
            self.assertIn(name, names)

    def test_duplicate_registration_raises_error(self):
        with self.assertRaises(ValueError):
            CommandRegistry.register("gen", "Duplicate gen", lambda: None)

    def test_create_returns_command_instance(self):
        cmd = CommandRegistry.create("doctor")
        self.assertIsNotNone(cmd)
        self.assertEqual(getattr(cmd, "name", "doctor"), "doctor")


if __name__ == "__main__":
    unittest.main()
