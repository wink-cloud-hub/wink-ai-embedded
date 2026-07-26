"""tools.cli.commands.setup — SetupCommand for inspecting and editing tools.json."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Optional

from tools.cli.base import CommandBase
from tools.cli.context import AppContext
from tools.cli.commands.doctor import _probe_all_for_setup

_TOOL_CAP_KEYS = {"python", "jinja2", "gcc", "cmake", "make", "emsdk", "idf", "node"}
_TOOLS_HOME_KEY = "tools_home"
_WORKSPACE_PATH_KEYS = {"sdk_dir", "frontend_dir", "esp32_dir", "scripts_dir"}


def _handle_setup_install(cap: str, ctx: AppContext) -> None:
    from tools.toolchain import providers as providers_mod
    from tools.toolchain.resolve import ResolveContext

    if cap not in providers_mod.REGISTRY:
        print(
            f"[wink] Error: unknown capability {cap!r}. Known: {', '.join(sorted(providers_mod.REGISTRY))}.",
            file=sys.stderr,
        )
        sys.exit(2)

    provider = providers_mod.REGISTRY[cap]
    probe_ctx = ResolveContext.snapshot(ctx.workspace_root)
    try:
        hint_text = provider.hint(probe_ctx)
    except Exception:
        hint_text = "(no hint available)"

    if cap == "idf":
        print(
            "[wink] ESP-IDF is never auto-installed by Wink (ADR-0030). "
            "Please install via Espressif IDF Manager (EIM); see wink-micro-os/tools/preinstall.md §3."
        )
        print(f"[wink] hint: {hint_text}")
        return

    print(f"[wink] Automatic install is not yet implemented (phase B). Manual step: {hint_text}")


def _handle_setup_set(kv: str, *, workspace: bool, ctx: AppContext) -> None:
    from tools.toolchain import providers as providers_mod
    from tools.toolchain.config import (
        save_user_path,
        save_workspace_layout_key,
        save_workspace_path,
    )
    from tools.toolchain.resolve import CAP_ENV_VARS, ResolveContext

    if "=" not in kv:
        print(f"[wink] Error: --set expects key=value, got {kv!r}.", file=sys.stderr)
        sys.exit(2)
    key, _, value = kv.partition("=")
    key = key.strip()
    value = value.strip()

    if not key or not value:
        print("[wink] Error: --set key and value must both be non-empty.", file=sys.stderr)
        sys.exit(2)

    if key not in _TOOL_CAP_KEYS and key != _TOOLS_HOME_KEY and key not in _WORKSPACE_PATH_KEYS:
        allowed = sorted(_TOOL_CAP_KEYS | {_TOOLS_HOME_KEY} | _WORKSPACE_PATH_KEYS)
        print(f"[wink] Error: unknown key {key!r}. Allowed: {', '.join(allowed)}.", file=sys.stderr)
        sys.exit(2)

    if key in _TOOL_CAP_KEYS:
        provider = providers_mod.REGISTRY.get(key)
        if provider is None:
            print(f"[wink] Error: no provider registered for {key!r}.", file=sys.stderr)
            sys.exit(2)

        env_var = CAP_ENV_VARS.get(key)
        probe_ctx = ResolveContext.snapshot(ctx.workspace_root)
        if env_var:
            probe_ctx.environ[env_var] = value
        else:
            probe_ctx.workspace_paths[key] = value
        result = provider.detect(probe_ctx)
        if not result.found:
            reason = result.reason or "detect() reported not-found"
            print(
                f"[wink] Error: validation failed for {key}={value!r}: {reason}. Config NOT written.",
                file=sys.stderr,
            )
            sys.exit(1)

        if workspace:
            ws_root = ctx.workspace_root
            if ws_root is None:
                print("[wink] Error: --workspace requested but no workspace root resolved.", file=sys.stderr)
                sys.exit(1)
            target = save_workspace_path(ws_root, key, value)
        else:
            target = save_user_path(key, value)
        print(f"[wink] wrote {key}={value} to {target}")
        return

    if key in _WORKSPACE_PATH_KEYS:
        p = Path(value)
        if not p.exists() or not p.is_dir():
            print(
                f"[wink] Error: workspace path {key}={value!r} does not exist or is not a directory. Config NOT written.",
                file=sys.stderr,
            )
            sys.exit(1)
        ws_root = ctx.workspace_root
        if ws_root is None:
            print("[wink] Error: cannot resolve workspace root to write wink-workspace.json.", file=sys.stderr)
            sys.exit(1)
        target = save_workspace_layout_key(ws_root, key, value)
        print(f"[wink] wrote {key}={value} to {target}")
        return

    if key == _TOOLS_HOME_KEY:
        p = Path(value)
        if not p.exists():
            print(
                f"[wink] Error: tools_home={value!r} does not exist. Create it first or point at an existing directory. Config NOT written.",
                file=sys.stderr,
            )
            sys.exit(1)
        if workspace:
            ws_root = ctx.workspace_root
            if ws_root is None:
                print("[wink] Error: --workspace requested but no workspace root resolved.", file=sys.stderr)
                sys.exit(1)
            target = save_workspace_path(ws_root, key, value)
        else:
            target = save_user_path(key, value)
        print(f"[wink] wrote {key}={value} to {target}")
        return


def _handle_setup_noargs(ctx: AppContext) -> None:
    from tools.toolchain.config import _user_config_path, _workspace_config_path

    rows, _ = _probe_all_for_setup(ctx)
    print("Wink toolchain resolution:")
    for cap_id, result, source_tag in rows:
        if result.found:
            path_str = str(result.path) if result.path else "(no path)"
            ver_str = f"({result.version})" if result.version else ""
            print(f"  {cap_id:<8}: {path_str}   {ver_str}   [{source_tag}]")
        else:
            reason = result.reason or "not detected"
            print(f"  {cap_id:<8}: not detected — {reason}   [{source_tag}]")

    print()
    print("Config files:")
    user_p = _user_config_path()
    ws_root = ctx.workspace_root
    ws_p = _workspace_config_path(ws_root) if ws_root else None
    user_state = "present" if user_p.exists() else "not present"
    print(f"  user      : {user_p} ({user_state})")
    if ws_p is not None:
        ws_state = "present" if ws_p.exists() else "not present"
        print(f"  workspace : {ws_p} ({ws_state})")


class SetupCommand(CommandBase):
    name = "setup"
    help = "Inspect or edit ~/.wink/tools.json"

    def register_args(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--set", dest="set_kv", metavar="KEY=VALUE", default=None, help="Validate and write paths[KEY]=VALUE.")
        parser.add_argument("--workspace", action="store_true", help="With --set, write to <workspace>/.wink/tools.json instead of user config.")
        parser.add_argument("--install", dest="install_cap", metavar="CAP", default=None, help="Attempt automatic install.")

    def run(self, ctx: AppContext, args: argparse.Namespace) -> Optional[int]:
        if args.set_kv and args.install_cap:
            print("[wink] Error: --set and --install are mutually exclusive.", file=sys.stderr)
            sys.exit(2)

        if args.install_cap:
            _handle_setup_install(args.install_cap, ctx)
            return 0

        if args.set_kv:
            _handle_setup_set(args.set_kv, workspace=args.workspace, ctx=ctx)
            return 0

        _handle_setup_noargs(ctx)
        return 0
