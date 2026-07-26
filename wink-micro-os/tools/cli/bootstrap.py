"""tools.cli.bootstrap — Environment initialization and AppContext creation."""
from __future__ import annotations

import ctypes
import json
import os
import sys
from pathlib import Path
from typing import Any, Dict, Optional

from tools.cli.context import AppContext


def _color_supported() -> bool:
    """Decide whether to emit ANSI colour escape sequences."""
    if os.environ.get("NO_COLOR"):
        return False
    fc = os.environ.get("FORCE_COLOR")
    if fc is not None and fc not in ("0", ""):
        return True
    if os.environ.get("TERM") == "dumb":
        return False
    if not hasattr(sys.stdout, "isatty") or not sys.stdout.isatty():
        return False
    if sys.platform != "win32":
        return True
    # Windows: enable VT processing on conhost.
    try:
        from ctypes import wintypes
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.GetStdHandle.argtypes = [wintypes.DWORD]
        kernel32.GetStdHandle.restype = wintypes.HANDLE
        kernel32.GetConsoleMode.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        kernel32.GetConsoleMode.restype = wintypes.BOOL
        kernel32.SetConsoleMode.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        kernel32.SetConsoleMode.restype = wintypes.BOOL
        ENABLE_VT = 0x0004
        STD_OUTPUT_HANDLE = -11 & 0xFFFFFFFF
        STD_ERROR_HANDLE = -12 & 0xFFFFFFFF
        INVALID_HANDLE = wintypes.HANDLE(-1).value
        ok = False
        for handle_id in (STD_OUTPUT_HANDLE, STD_ERROR_HANDLE):
            h = kernel32.GetStdHandle(handle_id)
            if not h or h == INVALID_HANDLE:
                continue
            mode = wintypes.DWORD(0)
            if kernel32.GetConsoleMode(h, ctypes.byref(mode)):
                if kernel32.SetConsoleMode(h, mode.value | ENABLE_VT):
                    ok = True
        return ok
    except Exception:
        return False


def _preparse_app_dir(argv: list[str]) -> Optional[Path]:
    """Scan argv for --app and return its resolved directory, if it's a path."""
    for i, arg in enumerate(argv):
        if arg == "--app" and i + 1 < len(argv):
            p = Path(argv[i + 1])
            if p.exists() and p.is_dir():
                return p.resolve()
        elif arg.startswith("--app="):
            p = Path(arg.split("=", 1)[1])
            if p.exists() and p.is_dir():
                return p.resolve()
    return None


def _derive_app_workspace_root(app_dir: Path) -> Optional[Path]:
    """Walk up from app_dir to find the workspace root."""
    if app_dir.parent.name == "wink-micro-app":
        return app_dir.parent.parent
    cur = app_dir.parent
    for _ in range(5):
        if (cur / "wink-workspace.json").exists():
            return cur
        parent = cur.parent
        if parent == cur:
            break
        cur = parent
    return None


def load_workspace_config(sdk_root: Path, workspace_root: Path, app_dir: Optional[Path], app_ws_root: Optional[Path]) -> dict:
    """Loads workspace config from 'wink-workspace.json' if present."""
    candidates = [
        Path(os.getcwd()) / "wink-workspace.json",
    ]
    if app_dir is not None:
        candidates.append(app_dir / "wink-workspace.json")
        if app_ws_root:
            candidates.append(app_ws_root / "wink-workspace.json")
    candidates.extend([
        workspace_root / "wink-workspace.json",
        sdk_root / "wink-workspace.json",
    ])
    for c in candidates:
        if c.exists():
            try:
                with c.open("r", encoding="utf-8") as fp:
                    return json.load(fp)
            except Exception as e:
                print(f"[wink] Warning: failed to parse '{c}': {e}", file=sys.stderr)
    return {}


def bootstrap(argv: list[str] | None = None) -> AppContext:
    """Initialize console encoding, sys.path, resolve paths/config/env, and return AppContext."""
    if argv is None:
        argv = sys.argv[1:]

    # UTF-8 Win32 console setup
    if sys.platform == "win32":
        for _stream in (sys.stdout, sys.stderr):
            if hasattr(_stream, "reconfigure"):
                try:
                    _stream.reconfigure(encoding="utf-8", errors="replace")
                except Exception:
                    pass
        try:
            _kernel32 = ctypes.windll.kernel32
            _kernel32.SetConsoleOutputCP(65001)
            _kernel32.SetConsoleCP(65001)
        except Exception:
            pass

    # SDK root = parent of tools/ directory
    sdk_root = Path(__file__).resolve().parent.parent.parent
    workspace_root = sdk_root.parent

    if str(sdk_root) not in sys.path:
        sys.path.insert(0, str(sdk_root))

    app_dir = _preparse_app_dir(argv)
    app_ws_root = _derive_app_workspace_root(app_dir) if app_dir else None
    config = load_workspace_config(sdk_root, workspace_root, app_dir, app_ws_root)

    # Environment snapshot
    env_snapshot = dict(os.environ)

    return AppContext(
        sdk_root=sdk_root,
        workspace_root=workspace_root,
        app_dir=app_dir,
        config=config,
        env=env_snapshot,
    )
