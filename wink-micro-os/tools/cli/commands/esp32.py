"""tools.cli.commands.esp32 — Esp32Command for building and flashing ESP32 firmware."""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Optional

from tools.cli._shared import run_cmd
from tools.cli.base import CommandBase
from tools.cli.commands.gen import resolve_app_dir
from tools.cli.context import AppContext


class Esp32Command(CommandBase):
    name = "esp32"
    help = "Build, flash, or monitor ESP32 firmware"

    def register_args(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--app", default="devkitc_smoke", help="App name in samples/ or path to app directory")
        parser.add_argument("idf_args", nargs="*", default=["build"], help="Arguments forwarded to idf.py")

    def run(self, ctx: AppContext, args: argparse.Namespace) -> Optional[int]:
        app_dir = resolve_app_dir(ctx, args.app)

        sdk_dir = ctx.sdk_root
        esp32_dir = ctx.workspace_root / "esp32_firmware"
        if not esp32_dir.exists():
            print(
                "[wink] Error: Cannot resolve esp32_firmware directory. "
                "Set WINK_ESP32_PATH environment variable, esp32_dir in wink-workspace.json, or run in the monorepo.",
                file=sys.stderr,
            )
            return 1

        codegen_dir = ctx.sdk_root / "tools" / "codegen"

        os.environ["WINK_APP_DIR"] = app_dir.as_posix()
        os.environ["WINK_SDK_PATH"] = sdk_dir.as_posix()
        os.environ.setdefault("PYTHONUTF8", "1")
        os.environ.setdefault("PYTHONIOENCODING", "utf-8")

        gen_script = sdk_dir / "tools" / "esp32" / "generate_app_sources.py"
        try:
            run_cmd([
                sys.executable,
                str(gen_script),
                "--esp32-firmware-dir", str(esp32_dir),
                "--app-dir", str(app_dir),
            ])
        except Exception as e:
            print(f"[wink] Error: failed to generate app sources for ESP32: {e}", file=sys.stderr)
            return 1

        build_script = sdk_dir / "tools" / "esp32" / "build.py"
        idf_args = args.idf_args if args.idf_args else ["build"]
        if idf_args and idf_args[0] == "--":
            idf_args = idf_args[1:]

        cmake_app_def = f"-DWINK_APP_DIR={app_dir.as_posix()}"
        cmake_sdk_def = f"-DWINK_SDK_PATH={sdk_dir.as_posix()}"
        cmake_codegen_def = f"-DWINK_CODEGEN_ROOT={codegen_dir.as_posix()}"
        idf_args = [cmake_app_def, cmake_sdk_def, cmake_codegen_def] + idf_args

        try:
            run_cmd([
                sys.executable,
                str(build_script),
                "--esp32-firmware-dir", str(esp32_dir),
                "--",
            ] + idf_args)
            print("[wink] Success: ESP32 Firmware build step complete!")
            return 0
        except Exception as e:
            print(f"[wink] Error: ESP32 build step failed: {e}", file=sys.stderr)
            return 1
