"""tools.cli.dispatcher — Dispatch CLI commands and apply toolchain gating."""
from __future__ import annotations

import argparse
import sys
from typing import Optional

from tools.cli.context import AppContext
from tools.cli.registry import CommandRegistry


def _resolve_gate_command(args: argparse.Namespace) -> str:
    """Map an argparse Namespace to the ensure_for() command key."""
    command = getattr(args, "command", None)
    if command == "build":
        target = getattr(args, "target", None)
        if target in ("host", "wasm"):
            return target
        return command
    return command


def _apply_toolchain_gate(ctx: AppContext, gate_command: str, skip: bool) -> None:
    """Invoke tools.toolchain.ensure_for for the given profile-selector key."""
    from tools.toolchain.check import ensure_for
    from tools.toolchain.profiles import WORKSPACE_DEPS

    profile_map = {
        "gen": "codegen",
        "host": "host",
        "wasm": "wasm",
        "test": "test",
        "esp32": "esp32",
        "web": "web",
    }
    profile_name = profile_map.get(gate_command, gate_command)
    keys = WORKSPACE_DEPS.get(profile_name, [])

    def _cb() -> dict:
        result = {}
        if "sdk_dir" in keys:
            result["sdk_dir"] = ctx.sdk_root
        if "frontend_dir" in keys:
            fe = ctx.workspace_root / "embedded-frontend"
            result["frontend_dir"] = fe if fe.exists() else None
        if "esp32_dir" in keys:
            esp = ctx.workspace_root / "esp32_firmware"
            result["esp32_dir"] = esp if esp.exists() else None
        if "scripts_dir" in keys:
            sc = ctx.workspace_root / "scripts"
            result["scripts_dir"] = sc if sc.exists() else None
        return result

    ensure_for(
        gate_command,
        workspace_root=ctx.workspace_root,
        resolve_workspace_paths=_cb,
        skip=skip,
    )


def dispatch(ctx: AppContext, argv: list[str] | None = None) -> Optional[int]:
    """Build parser from registered commands, apply toolchain gate, and execute command."""
    if argv is None:
        argv = sys.argv[1:]

    global_parent = argparse.ArgumentParser(add_help=False)
    global_parent.add_argument(
        "--skip-toolchain-check",
        dest="skip_toolchain_check",
        action="store_true",
        help="Bypass toolchain gating (emergency escape hatch; prints WARN).",
    )

    p = argparse.ArgumentParser(
        prog="wink",
        description="Unified build orchestrator CLI for Wink Micro OS.",
        parents=[global_parent],
    )
    sub = p.add_subparsers(dest="command", required=True, help="Subcommand to execute")

    # Dynamically populate subparser arguments from registry
    for name in CommandRegistry.names():
        cmd_inst = CommandRegistry.create(name)
        cmd_help = CommandRegistry.get_help(name) or cmd_inst.help
        sub_p = sub.add_parser(name, parents=[global_parent], help=cmd_help)
        cmd_inst.register_args(sub_p)

    # Argparse quirk handling: preserve OR semantics for --skip-toolchain-check
    skip_from_prepass = "--skip-toolchain-check" in argv

    args = p.parse_args(argv)
    if skip_from_prepass:
        args.skip_toolchain_check = True

    # Doctor, setup, and lint bypass gating check
    if args.command not in ("doctor", "setup", "lint"):
        gate_command = _resolve_gate_command(args)
        _apply_toolchain_gate(ctx, gate_command, skip=args.skip_toolchain_check)

    cmd_obj = CommandRegistry.create(args.command)
    return cmd_obj.run(ctx, args)
