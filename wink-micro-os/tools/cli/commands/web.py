"""tools.cli.commands.web — WebCommand for running Vite frontend server."""
from __future__ import annotations

import argparse
import sys
from typing import Optional

from tools.cli._shared import run_cmd
from tools.cli.base import CommandBase
from tools.cli.context import AppContext


class WebCommand(CommandBase):
    name = "web"
    help = "Start Vue Vite frontend web server"

    def register_args(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--port", type=int, default=5173, help="Vite server port (default: 5173)")

    def run(self, ctx: AppContext, args: argparse.Namespace) -> Optional[int]:
        frontend_dir = ctx.workspace_root / "embedded-frontend"
        if not frontend_dir.exists():
            print(
                "[wink] Error: Cannot resolve embedded-frontend directory. "
                "Set WINK_FRONTEND_PATH environment variable, frontend_dir in wink-workspace.json, or run in the monorepo.",
                file=sys.stderr,
            )
            return 1

        print("[wink] Starting Vue/Vite frontend dev server (press Ctrl+C to stop)...")
        try:
            run_cmd(["npm", "run", "dev", "--", "--port", str(args.port)], cwd=frontend_dir)
            return 0
        except Exception as e:
            print(f"[wink] Error: failed to start web dev server: {e}", file=sys.stderr)
            return 1
