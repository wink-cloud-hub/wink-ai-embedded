"""tools.cli.commands.gen — GenCommand for running device tree and config codegen."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Optional

from tools.cli._shared import run_cmd
from tools.cli.base import CommandBase
from tools.cli.context import AppContext


def resolve_app_dir(ctx: AppContext, app: str) -> Path:
    """Resolve app directory from path, wink-micro-app, or samples."""
    path_opt = Path(app)
    micro_app_dir = ctx.workspace_root / "wink-micro-app" / app
    samples_dir = ctx.sdk_root / "samples" / app

    if path_opt.exists() and path_opt.is_dir() and (path_opt.resolve() / "wink-app.json").exists():
        return path_opt.resolve()
    if micro_app_dir.exists() and micro_app_dir.is_dir() and (micro_app_dir.resolve() / "wink-app.json").exists():
        return micro_app_dir.resolve()
    if samples_dir.exists() and samples_dir.is_dir() and (samples_dir.resolve() / "wink-app.json").exists():
        return samples_dir.resolve()

    if path_opt.exists() and path_opt.is_dir():
        return path_opt.resolve()
    if micro_app_dir.exists() and micro_app_dir.is_dir():
        return micro_app_dir.resolve()
    if samples_dir.exists() and samples_dir.is_dir():
        return samples_dir.resolve()

    print(f"[wink] Error: Cannot resolve App '{app}' as a path or as a sample in '{ctx.sdk_root}/samples/'", file=sys.stderr)
    sys.exit(1)


class GenCommand(CommandBase):
    name = "gen"
    help = "Run device tree & config macro codegen"

    def register_args(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--app", default="oled_dashboard", help="App name in samples/ or path to app directory")

    def run(self, ctx: AppContext, args: argparse.Namespace) -> Optional[int]:
        app_dir = resolve_app_dir(ctx, args.app)
        config_json = app_dir / "wink-app.json"

        if not config_json.exists():
            print(f"[wink] Error: 'wink-app.json' not found in App directory: {app_dir}", file=sys.stderr)
            return 1

        out_dir = ctx.workspace_root / "build" / "generated"
        codegen_script = ctx.sdk_root / "tools" / "codegen" / "app_codegen.py"

        try:
            run_cmd([
                sys.executable,
                str(codegen_script),
                "--config", str(config_json),
                "--out-dir", str(out_dir),
            ])
            print("[wink] Success: Codegen and API documentation generation complete!")
            return 0
        except Exception as e:
            print(f"[wink] Error: Codegen execution failed: {e}", file=sys.stderr)
            return 1
