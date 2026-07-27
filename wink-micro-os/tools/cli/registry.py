"""tools.cli.registry — Central command registry supporting lazy loading."""
from __future__ import annotations

from typing import Callable, Dict, List, Optional
from tools.cli.base import CommandBase


class CommandRegistry:
    """Command registry preserving lazy-loading semantics."""

    _factories: Dict[str, Callable[[], CommandBase]] = {}
    _help_texts: Dict[str, str] = {}

    @classmethod
    def register(cls, name: str, help_text: str, factory: Callable[[], CommandBase]) -> None:
        """Register a command by name with a lazy factory."""
        if name in cls._factories:
            raise ValueError(f"Duplicate command registration for name: {name!r}")
        cls._factories[name] = factory
        cls._help_texts[name] = help_text

    @classmethod
    def create(cls, name: str) -> CommandBase:
        """Instantiate a command by lazy importing it via factory."""
        if name not in cls._factories:
            raise KeyError(f"Command {name!r} not registered.")
        return cls._factories[name]()

    @classmethod
    def get_help(cls, name: str) -> str:
        return cls._help_texts.get(name, "")

    @classmethod
    def names(cls) -> List[str]:
        """Return sorted list of registered command names."""
        return sorted(cls._factories.keys())

    @classmethod
    def clear(cls) -> None:
        """Reset registry (primarily for testing)."""
        cls._factories.clear()
        cls._help_texts.clear()


def register_default_commands() -> None:
    """Register lazy factories for standard Wink CLI subcommands."""

    def _gen_factory():
        from tools.cli.commands.gen import GenCommand
        return GenCommand()

    def _build_factory():
        from tools.cli.commands.build import BuildCommand
        return BuildCommand()

    def _esp32_factory():
        from tools.cli.commands.esp32 import Esp32Command
        return Esp32Command()

    def _web_factory():
        from tools.cli.commands.web import WebCommand
        return WebCommand()

    def _test_factory():
        from tools.cli.commands.test import TestCommand
        return TestCommand()

    def _doctor_factory():
        from tools.cli.commands.doctor import DoctorCommand
        return DoctorCommand()

    def _setup_factory():
        from tools.cli.commands.setup import SetupCommand
        return SetupCommand()

    def _lint_factory():
        from tools.cli.commands.lint import LintCommand
        return LintCommand()

    def _new_dal_factory():
        from tools.cli.commands.new_dal import NewDalCommand
        return NewDalCommand()

    CommandRegistry.register("gen", "Run device tree & config macro codegen", _gen_factory)
    CommandRegistry.register("build", "Build Host or WASM simulators", _build_factory)
    CommandRegistry.register("esp32", "Build, flash, or monitor ESP32 firmware", _esp32_factory)
    CommandRegistry.register("web", "Start Vue Vite frontend web server", _web_factory)
    CommandRegistry.register("test", "Run Python, C unit tests, sanitizer pass matrix, and lints", _test_factory)
    CommandRegistry.register("doctor", "Probe every registered toolchain capability", _doctor_factory)
    CommandRegistry.register("setup", "Inspect or edit ~/.wink/tools.json", _setup_factory)
    CommandRegistry.register("lint", "Run YAML layer/API/Arduino lints (ADR-0043)", _lint_factory)
    CommandRegistry.register(
        "new-dal",
        "Scaffold DAL .h/.c + codegen driver plugin (ADR-0046)",
        _new_dal_factory,
    )


# Populate default commands on module load
register_default_commands()
