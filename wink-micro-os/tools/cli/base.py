"""tools.cli.base — Abstract base class for CLI commands."""
from __future__ import annotations

import argparse
from abc import ABC, abstractmethod
from typing import Optional

from tools.cli.context import AppContext


class CommandBase(ABC):
    """Abstract base class for all CLI subcommands.

    Rules:
    - Must NOT import heavy dependencies (PyYAML, Jinja2, toolchain providers) at module top-level.
    - All business logic execution should happen in run(ctx, args).
    """

    name: str = ""
    help: str = ""

    @abstractmethod
    def register_args(self, parser: argparse.ArgumentParser) -> None:
        """Register command-specific CLI flags."""

    @abstractmethod
    def run(self, ctx: AppContext, args: argparse.Namespace) -> Optional[int]:
        """Execute command business logic."""
