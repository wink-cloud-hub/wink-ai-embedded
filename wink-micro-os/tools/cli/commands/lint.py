"""tools.cli.commands.lint — LintCommand for static architecture linting."""
from __future__ import annotations

import argparse
import sys
from typing import Optional

from tools.cli.base import CommandBase
from tools.cli.context import AppContext


class LintCommand(CommandBase):
    name = "lint"
    help = "Run YAML layer/API/Arduino lints (ADR-0043)"

    def register_args(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--root", default=None, help="SDK root to scan (default: wink-micro-os/)")
        parser.add_argument("--config", action="append", default=[], help="Extra YAML config path (repeatable)")
        parser.add_argument("--pack", action="append", default=None, help="Rule pack id to run")
        parser.add_argument("--rule", default=None, help="Only report findings for this rule id")
        parser.add_argument("--paths", nargs="*", default=None, help="Incremental scan: only these files")
        parser.add_argument("--changed", nargs="?", const="HEAD", default=None, help="Derive --paths from git diff")
        parser.add_argument("--format", choices=["text", "json", "sarif"], default="text", help="Output format")
        parser.add_argument("--output", default=None, help="Write report to FILE instead of stdout")
        parser.add_argument("--strict", action="store_true", help="Treat warnings as failures")
        parser.add_argument("--explain", metavar="RULE_ID", default=None, help="Print rule explanation and exit 0")
        parser.add_argument("--report-allowlist", action="store_true", help="Report allowlisted / expiring allow_paths entries")
        parser.add_argument("--baseline", default=None, help="Optional baseline file for fingerprint diff")
        parser.add_argument("--today", default=None, help="Override today for until expiry (YYYY-MM-DD)")

    def run(self, ctx: AppContext, args: argparse.Namespace) -> Optional[int]:
        try:
            from tools.lint.cli import handle_lint
        except ModuleNotFoundError as exc:
            if exc.name == "yaml":
                print(
                    "[wink lint] PyYAML is required. Install it with:\n"
                    "    python -m pip install -r wink-micro-os/tools/requirements-lint.txt\n"
                    '  (or: python -m pip install "PyYAML>=6")',
                    file=sys.stderr,
                )
                return 2
            raise
        handle_lint(args)
        return 0
