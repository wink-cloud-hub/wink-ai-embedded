"""tools.cli.commands.build — BuildCommand for Host and WASM simulator builds."""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path
from typing import Optional

from tools.cli._shared import run_cmd
from tools.cli.base import CommandBase
from tools.cli.commands.gen import resolve_app_dir
from tools.cli.context import AppContext


def _is_in_tree_source_sdk(sdk_root: Path) -> bool:
    return (
        (sdk_root / "dal").is_dir()
        and (sdk_root / "runtime").is_dir()
        and (sdk_root / "CMakeLists.txt").is_file()
    )


def resolve_sdk_dir_for_build(ctx: AppContext, sdk_mode: Optional[str] = None) -> Path:
    if sdk_mode == "binary":
        env_path = ctx.env.get("WINK_SDK_PATH")
        if env_path:
            return Path(env_path).resolve()
        candidate = ctx.sdk_root
        if (candidate / "libs").is_dir():
            return candidate
        print(
            "[wink] Error: --sdk-mode binary but no Binary SDK found. "
            "Set WINK_SDK_PATH to an extracted wink-micro-os-sdk-binary tarball.",
            file=sys.stderr,
        )
        sys.exit(1)

    if _is_in_tree_source_sdk(ctx.sdk_root):
        return ctx.sdk_root

    env_val = ctx.env.get("WINK_SDK_PATH")
    if env_val:
        return Path(env_val).resolve()
    config_val = ctx.config.get("sdk_dir")
    if config_val:
        return Path(config_val).resolve()
    return ctx.sdk_root


class BuildCommand(CommandBase):
    name = "build"
    help = "Build Host or WASM simulators"

    def register_args(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("target", choices=["host", "wasm"], help="Build target platform")
        parser.add_argument("--app", default="oled_dashboard", help="App name in samples/ or path to app directory")
        parser.add_argument("--clean", action="store_true", help="Clean the build directory before building (host only)")
        parser.add_argument(
            "--sdk-mode",
            choices=["source", "binary"],
            default=None,
            help="SDK mode: 'source' (build from source) or 'binary' (use precompiled .a).",
        )

    def run(self, ctx: AppContext, args: argparse.Namespace) -> Optional[int]:
        app_dir = resolve_app_dir(ctx, args.app)

        if args.target == "host":
            ws_root = ctx.workspace_root
            build_dir = ws_root / "build" / "host"
            sdk_mode = getattr(args, "sdk_mode", None)
            micro_os_dir = resolve_sdk_dir_for_build(ctx, sdk_mode)
            codegen_dir = micro_os_dir / "tools" / "codegen"

            if sdk_mode == "binary" and not (micro_os_dir / "libs" / "host").exists():
                print("[wink] Error: --sdk-mode binary but libs/host/ not found in SDK tree.", file=sys.stderr)
                return 1

            configure_cmd = [
                "cmake",
                "-S", str(micro_os_dir),
                "-B", str(build_dir),
                "-DTARGET_PLATFORM=host",
                f"-DWINK_APP_DIR={app_dir.as_posix()}",
                f"-DWINK_CODEGEN_ROOT={codegen_dir.as_posix()}",
            ]
            if sdk_mode:
                configure_cmd.append(f"-DWINK_SDK_MODE={sdk_mode}")
            if sys.platform == "win32":
                configure_cmd.extend(["-G", "MinGW Makefiles"])

            if args.clean and build_dir.exists():
                print(f"[wink] Cleaning build directory: {build_dir}")
                shutil.rmtree(build_dir)

            try:
                run_cmd(configure_cmd)
                run_cmd(["cmake", "--build", str(build_dir)])
                print(f"[wink] Success: Host simulator build complete. Output is in {build_dir}")
                return 0
            except Exception as e:
                print(f"[wink] Error: Host build failed: {e}", file=sys.stderr)
                return 1

        elif args.target == "wasm":
            ws_root = ctx.workspace_root
            app_name = app_dir.name
            build_dir = ws_root / "build" / "wasm" / app_name
            sdk_mode = getattr(args, "sdk_mode", None)
            micro_os_dir = resolve_sdk_dir_for_build(ctx, sdk_mode)
            codegen_dir = micro_os_dir / "tools" / "codegen"

            if sdk_mode == "binary" and not (micro_os_dir / "libs" / "wasm").exists():
                print("[wink] Error: --sdk-mode binary but libs/wasm/ not found in SDK tree.", file=sys.stderr)
                return 1

            configure_cmd = [
                "emcmake", "cmake",
                "-S", str(micro_os_dir),
                "-B", str(build_dir),
                "-DTARGET_PLATFORM=wasm",
                f"-DWINK_APP_DIR={app_dir.as_posix()}",
                f"-DWINK_CODEGEN_ROOT={codegen_dir.as_posix()}",
            ]
            if sdk_mode:
                configure_cmd.append(f"-DWINK_SDK_MODE={sdk_mode}")
            if app_name == "unisim_smoke":
                configure_cmd.append("-DWINK_STRICT_NONBLOCKING=0")

            if args.clean and build_dir.exists():
                print(f"[wink] Cleaning build directory: {build_dir}")
                shutil.rmtree(build_dir)

            try:
                run_cmd(configure_cmd)
                run_cmd(["cmake", "--build", str(build_dir)])
                print(f"[wink] Success: WASM build complete for '{app_name}'. Output in {build_dir}")
                return 0
            except Exception as e:
                print(f"[wink] Error: WASM build failed: {e}", file=sys.stderr)
                return 1

        return 0
