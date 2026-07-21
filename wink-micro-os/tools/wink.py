#!/usr/bin/env python3
"""wink.py — Unified build orchestrator and CLI gateway for Wink Micro OS.

Supports Host, WASM, and ESP32 platforms with unified custom application path support.
Supports workspace config 'wink-workspace.json' for full directory relocatability.

This CLI lives inside the SDK tree (wink-micro-os/tools/). Sibling monorepo
dirs (frontend, esp32_firmware, apps) are resolved via env / workspace config /
SDK parent defaults.

Toolchain gating (Phase 2, ADR-0029/0030):
- Every non-diagnostic subcommand runs ``tools.toolchain.ensure_for(profile)``
  after argparse and before the handler. On failure a collect-all report is
  printed to stderr and the process exits 1.
- ``--skip-toolchain-check`` bypasses the gate with a prominent stderr warning
  (escape hatch, not the normal path).
- ``wink doctor`` probes every registered capability regardless of profile.
- ``wink setup`` reads/writes ``~/.wink/tools.json`` (or ``<ws>/.wink/tools.json``
  with ``--workspace``); validates via provider.detect() before writing.
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

# Ensure UTF-8 output on Windows to avoid mojibake for Unicode symbols
# (e.g. ✗, §) in toolchain reports. On zh-CN Windows the console defaults to
# cp936, which can't encode these glyphs and produces "??" or garbled bytes.
if sys.platform == "win32":
    for _stream in (sys.stdout, sys.stderr):
        if hasattr(_stream, "reconfigure"):
            try:
                _stream.reconfigure(encoding="utf-8", errors="replace")
            except Exception:
                pass
    # Also flip the Win32 console code page to UTF-8 so downstream child
    # processes and the terminal render our output correctly.
    try:
        import ctypes
        _kernel32 = ctypes.windll.kernel32
        _kernel32.SetConsoleOutputCP(65001)  # CP_UTF8
        _kernel32.SetConsoleCP(65001)
    except Exception:
        pass


def _color_supported() -> bool:
    """Decide whether to emit ANSI colour escape sequences.

    Rules (highest priority first):
    - ``NO_COLOR`` env var set (non-empty) → disabled (https://no-color.org/)
    - ``FORCE_COLOR`` env var set to a non-``0`` value → enabled
    - ``TERM`` == ``dumb`` → disabled
    - Non-TTY stdout (pipe/redirect) → disabled (no junk in log files)
    - Windows: try to flip ENABLE_VIRTUAL_TERMINAL_PROCESSING (0x0004) on the
      conhost stdout/stderr handles; require both the GetConsoleMode call to
      succeed and SetConsoleMode to succeed (a pipe returns INVALID_HANDLE
      from GetStdHandle → GetConsoleMode fails → we fall back to plain marks).
    - Non-Windows TTY → enabled.
    """
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
        import ctypes
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


# Resolve colour support once at startup. Doctor and any other coloured output
# read this; other subcommands are unaffected.
_VT_ENABLED = _color_supported()

# SDK root = wink-micro-os/ (parent of tools/)
SDK_ROOT = Path(__file__).resolve().parent.parent
# Default monorepo / workspace root = parent of the SDK package
WORKSPACE_ROOT = SDK_ROOT.parent

# Ensure the SDK package root is importable so we can pull in
# ``tools.toolchain`` regardless of how wink.py was invoked.
if str(SDK_ROOT) not in sys.path:
    sys.path.insert(0, str(SDK_ROOT))


def _preparse_app_dir() -> Path | None:
    """Scan sys.argv for --app and return its resolved directory, if it's a path."""
    for i, arg in enumerate(sys.argv):
        if arg == "--app" and i + 1 < len(sys.argv):
            p = Path(sys.argv[i + 1])
            if p.exists() and p.is_dir():
                return p.resolve()
        elif arg.startswith("--app="):
            p = Path(arg.split("=", 1)[1])
            if p.exists() and p.is_dir():
                return p.resolve()
    return None


def _derive_app_workspace_root(app_dir: Path) -> Path | None:
    """Walk up from app_dir to find the workspace root.

    Recognises two patterns:
      - app_dir is <ws>/wink-micro-app/<name>  →  ws
      - an ancestor directory contains wink-workspace.json
    """
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


_APP_DIR = _preparse_app_dir()
_APP_WORKSPACE_ROOT = _derive_app_workspace_root(_APP_DIR) if _APP_DIR else None


def load_workspace_config() -> dict:
    """Loads workspace config from 'wink-workspace.json' if present."""
    candidates = [
        Path(os.getcwd()) / "wink-workspace.json",
    ]
    if _APP_DIR is not None:
        candidates.append(_APP_DIR / "wink-workspace.json")
        if _APP_WORKSPACE_ROOT:
            candidates.append(_APP_WORKSPACE_ROOT / "wink-workspace.json")
    candidates.extend([
        WORKSPACE_ROOT / "wink-workspace.json",
        SDK_ROOT / "wink-workspace.json",
    ])
    for c in candidates:
        if c.exists():
            try:
                with c.open("r", encoding="utf-8") as fp:
                    return json.load(fp)
            except Exception as e:
                print(f"[wink] Warning: failed to parse '{c}': {e}", file=sys.stderr)
    return {}


CONFIG = load_workspace_config()


def _is_in_tree_source_sdk(sdk_root: Path) -> bool:
    """True when sdk_root looks like the full monorepo wink-micro-os source tree."""
    return (
        (sdk_root / "dal").is_dir()
        and (sdk_root / "runtime").is_dir()
        and (sdk_root / "CMakeLists.txt").is_file()
    )


def resolve_sdk_dir_for_build(sdk_mode: str | None = None) -> Path:
    """Resolve SDK root for build commands (source vs binary consumption)."""
    if sdk_mode == "binary":
        env_path = os.environ.get("WINK_SDK_PATH")
        if env_path:
            return Path(env_path).resolve()
        candidate = resolve_sdk_dir()
        if (candidate / "libs").is_dir():
            return candidate
        print(
            "[wink] Error: --sdk-mode binary but no Binary SDK found. "
            "Set WINK_SDK_PATH to an extracted wink-micro-os-sdk-binary tarball.",
            file=sys.stderr,
        )
        sys.exit(1)

    # Monorepo default: source builds use in-tree SDK even if WINK_SDK_PATH
    # still points at a Binary SDK tarball from prior experiments.
    if _is_in_tree_source_sdk(SDK_ROOT):
        return SDK_ROOT

    return resolve_sdk_dir()


def resolve_sdk_dir() -> Path:
    """Resolve the SDK (wink-micro-os) directory."""
    env_val = os.environ.get("WINK_SDK_PATH")
    if env_val:
        return Path(env_val).resolve()
    config_val = CONFIG.get("sdk_dir")
    if config_val:
        return Path(config_val).resolve()
    # Default: this script already lives inside the SDK
    return SDK_ROOT


def resolve_frontend_dir(required: bool = True) -> Path:
    """Resolve the embedded-frontend directory."""
    env_val = os.environ.get("WINK_FRONTEND_PATH")
    if env_val:
        return Path(env_val).resolve()
    config_val = CONFIG.get("frontend_dir")
    if config_val:
        return Path(config_val).resolve()
    default_path = WORKSPACE_ROOT / "embedded-frontend"
    if default_path.exists():
        return default_path
    if _APP_WORKSPACE_ROOT:
        app_ws_path = _APP_WORKSPACE_ROOT / "embedded-frontend"
        if app_ws_path.exists():
            return app_ws_path
    if not required:
        return default_path
    print("[wink] Error: Cannot resolve embedded-frontend directory. Set WINK_FRONTEND_PATH environment variable, frontend_dir in wink-workspace.json, or run in the monorepo.", file=sys.stderr)
    sys.exit(1)


def resolve_esp32_dir(required: bool = True) -> Path:
    """Resolve the esp32_firmware directory."""
    env_val = os.environ.get("WINK_ESP32_PATH")
    if env_val:
        return Path(env_val).resolve()
    config_val = CONFIG.get("esp32_dir")
    if config_val:
        return Path(config_val).resolve()
    default_path = WORKSPACE_ROOT / "esp32_firmware"
    if default_path.exists():
        return default_path
    if _APP_WORKSPACE_ROOT:
        app_ws_path = _APP_WORKSPACE_ROOT / "esp32_firmware"
        if app_ws_path.exists():
            return app_ws_path
    if not required:
        return default_path
    print("[wink] Error: Cannot resolve esp32_firmware directory. Set WINK_ESP32_PATH environment variable, esp32_dir in wink-workspace.json, or run in the monorepo.", file=sys.stderr)
    sys.exit(1)


def resolve_scripts_dir(required: bool = True) -> Path:
    """Resolve the build scripts directory."""
    env_val = os.environ.get("WINK_SCRIPTS_PATH")
    if env_val:
        return Path(env_val).resolve()
    config_val = CONFIG.get("scripts_dir")
    if config_val:
        return Path(config_val).resolve()
    default_path = WORKSPACE_ROOT / "scripts"
    if default_path.exists():
        return default_path
    if _APP_WORKSPACE_ROOT:
        app_ws_path = _APP_WORKSPACE_ROOT / "scripts"
        if app_ws_path.exists():
            return app_ws_path
    if not required:
        return default_path
    print("[wink] Error: Cannot resolve scripts directory. Set WINK_SCRIPTS_PATH environment variable, scripts_dir in wink-workspace.json, or run in the monorepo.", file=sys.stderr)
    sys.exit(1)


# Export SDK path always; sibling dirs are optional until a command needs them
# (Source SDK tarball has no embedded-frontend / esp32_firmware siblings).
os.environ["WINK_SDK_PATH"] = str(resolve_sdk_dir().as_posix())
_fe = resolve_frontend_dir(required=False)
if _fe.exists():
    os.environ["WINK_FRONTEND_PATH"] = str(_fe.as_posix())
_esp = resolve_esp32_dir(required=False)
if _esp.exists():
    os.environ["WINK_ESP32_PATH"] = str(_esp.as_posix())
_scripts = resolve_scripts_dir(required=False)
if _scripts.exists():
    os.environ["WINK_SCRIPTS_PATH"] = str(_scripts.as_posix())
os.environ["WINK_CODEGEN_ROOT"] = str((Path(__file__).resolve().parent / "codegen").as_posix())


def run_cmd(cmd: list[str] | str, cwd: Path = WORKSPACE_ROOT, shell: bool = False, check: bool = True) -> subprocess.CompletedProcess:
    """Helper to run a system command and print output."""
    cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
    try:
        rel = cwd.relative_to(WORKSPACE_ROOT) if cwd != WORKSPACE_ROOT and WORKSPACE_ROOT in cwd.parents else cwd
    except ValueError:
        rel = cwd
    print(f"\n[wink] Running: {cmd_str} (in {rel})")
    try:
        is_win = sys.platform == "win32"
        use_shell = shell or is_win
        return subprocess.run(cmd, cwd=cwd, shell=use_shell, check=check, env=os.environ)
    except subprocess.CalledProcessError as e:
        print(f"[wink] Error: Command failed with exit code {e.returncode}: {cmd_str}", file=sys.stderr)
        sys.exit(e.returncode)


def resolve_app_dir(app: str) -> Path:
    """Resolves the App directory.
    If app is a path that exists and contains 'wink-app.json', returns its absolute path.
    Otherwise, defaults to wink-micro-app/<app> or SDK samples/<app>.
    If no directory containing 'wink-app.json' matches, falls back to the first existing directory.
    """
    path_opt = Path(app)
    micro_app_dir = WORKSPACE_ROOT / "wink-micro-app" / app
    sdk_dir = resolve_sdk_dir()
    samples_dir = sdk_dir / "samples" / app

    # 1. Prefer directories that contain 'wink-app.json'
    if path_opt.exists() and path_opt.is_dir() and (path_opt.resolve() / "wink-app.json").exists():
        return path_opt.resolve()
    if micro_app_dir.exists() and micro_app_dir.is_dir() and (micro_app_dir.resolve() / "wink-app.json").exists():
        return micro_app_dir.resolve()
    if samples_dir.exists() and samples_dir.is_dir() and (samples_dir.resolve() / "wink-app.json").exists():
        return samples_dir.resolve()

    # 2. Fall back to any directory that exists (e.g. Arduino apps without 'wink-app.json')
    if path_opt.exists() and path_opt.is_dir():
        return path_opt.resolve()
    if micro_app_dir.exists() and micro_app_dir.is_dir():
        return micro_app_dir.resolve()
    if samples_dir.exists() and samples_dir.is_dir():
        return samples_dir.resolve()

    print(f"[wink] Error: Cannot resolve App '{app}' as a path or as a sample in '{sdk_dir}/samples/'", file=sys.stderr)
    sys.exit(1)


# ── Toolchain gate helpers ─────────────────────────────────────────────

def _current_workspace_root() -> Path:
    """Workspace root used for toolchain config discovery and ensure_for()."""
    return _APP_WORKSPACE_ROOT or WORKSPACE_ROOT


# Workspace-path keys understood by the toolchain gate and `wink setup`.
_WORKSPACE_KEY_RESOLVERS = {
    "sdk_dir":      lambda: resolve_sdk_dir(),
    "frontend_dir": lambda: resolve_frontend_dir(required=False),
    "esp32_dir":    lambda: resolve_esp32_dir(required=False),
    "scripts_dir":  lambda: resolve_scripts_dir(required=False),
}


def _workspace_key_probe(key: str) -> Path | None:
    """Return the resolved workspace path if it exists on disk, else None."""
    resolver = _WORKSPACE_KEY_RESOLVERS.get(key)
    if resolver is None:
        return None
    try:
        p = resolver()
    except SystemExit:
        return None
    if p is None:
        return None
    return p if p.exists() else None


def _make_workspace_paths_callback(keys: list[str]):
    """Build the resolve_workspace_paths callback ensure_for expects."""
    def _cb() -> dict[str, Path | None]:
        return {k: _workspace_key_probe(k) for k in keys}
    return _cb


def _resolve_gate_command(args) -> str:
    """Map an argparse Namespace to the ensure_for() command key.

    ensure_for() speaks in profile-selector keys — ``gen``, ``host``, ``wasm``,
    ``test``, ``esp32``, ``web``, ``doctor`` — not raw subparser names. The
    ``build`` subparser splits into two profiles depending on its positional
    ``target`` (host vs wasm), so we resolve that here rather than pushing the
    conditional into ensure_for's table.
    """
    command = getattr(args, "command", None)
    if command == "build":
        target = getattr(args, "target", None)
        if target in ("host", "wasm"):
            return target
        # Fall through: ensure_for will raise a clear ValueError.
        return command
    return command


def _apply_toolchain_gate(gate_command: str, skip: bool) -> None:
    """Invoke tools.toolchain.ensure_for for the given profile-selector key.

    ``gate_command`` must be one of the keys ensure_for() understands
    (``gen``/``host``/``wasm``/``test``/``esp32``/``web``/``doctor``), not the
    raw wink subparser name. Callers driving this from the CLI must first pass
    ``args`` through :func:`_resolve_gate_command` to translate ``build`` into
    the host/wasm split.

    Import is deferred so ``wink --help`` and doctor/setup can still function
    even when the toolchain package fails to import for some odd reason.
    """
    from tools.toolchain.check import ensure_for  # noqa: WPS433
    from tools.toolchain.profiles import WORKSPACE_DEPS  # noqa: WPS433

    # gate_command -> profile name for WORKSPACE_DEPS lookup. Doctor probes
    # every capability so it doesn't map to a single profile; other keys map
    # 1:1 except "gen" which drives the "codegen" profile.
    profile_map = {
        "gen":   "codegen",
        "host":  "host",
        "wasm":  "wasm",
        "test":  "test",
        "esp32": "esp32",
        "web":   "web",
    }
    profile = profile_map.get(gate_command)
    ws_keys = WORKSPACE_DEPS.get(profile, []) if profile else []
    cb = _make_workspace_paths_callback(ws_keys) if ws_keys else None

    ensure_for(
        gate_command,
        workspace_root=_current_workspace_root(),
        resolve_workspace_paths=cb,
        skip=skip,
    )


# ── Command Handlers ──────────────────────────────────────────────────

def handle_gen(args):
    """Run codegen generator for the target app."""
    app_dir = resolve_app_dir(args.app)
    config_json = app_dir / "wink-app.json"

    if not config_json.exists():
        print(f"[wink] Error: 'wink-app.json' not found in App directory: {app_dir}", file=sys.stderr)
        sys.exit(1)

    out_dir = WORKSPACE_ROOT / "build" / "generated"
    codegen_script = Path(__file__).resolve().parent / "codegen" / "app_codegen.py"

    run_cmd([
        sys.executable,
        str(codegen_script),
        "--config", str(config_json),
        "--out-dir", str(out_dir)
    ])
    print("[wink] Success: Codegen and API documentation generation complete!")


def handle_build(args):
    """Compile host or wasm simulator target."""
    app_dir = resolve_app_dir(args.app)

    if args.target == "host":
        ws_root = _APP_WORKSPACE_ROOT or WORKSPACE_ROOT
        build_dir = ws_root / "build" / "host"
        sdk_mode = getattr(args, "sdk_mode", None)
        micro_os_dir = resolve_sdk_dir_for_build(sdk_mode)
        codegen_dir = micro_os_dir / "tools" / "codegen"

        # Validate --sdk-mode against SDK tree early
        if sdk_mode == "binary" and not (micro_os_dir / "libs" / "host").exists():
            print("[wink] Error: --sdk-mode binary but libs/host/ not found in SDK tree.", file=sys.stderr)
            sys.exit(1)

        configure_cmd = [
            "cmake",
            "-S", str(micro_os_dir),
            "-B", str(build_dir),
            "-DTARGET_PLATFORM=host",
            f"-DWINK_APP_DIR={app_dir.as_posix()}",
            f"-DWINK_CODEGEN_ROOT={codegen_dir.as_posix()}"
        ]
        if sdk_mode:
            configure_cmd.append(f"-DWINK_SDK_MODE={sdk_mode}")
        if sys.platform == "win32":
            configure_cmd.extend(["-G", "MinGW Makefiles"])

        if args.clean:
            import shutil
            if build_dir.exists():
                print(f"[wink] Cleaning build directory: {build_dir}")
                shutil.rmtree(build_dir)

        run_cmd(configure_cmd)

        run_cmd(["cmake", "--build", str(build_dir)])
        print(f"[wink] Success: Host simulator build complete. Output is in {build_dir}")

    elif args.target == "wasm":
        ws_root = _APP_WORKSPACE_ROOT or WORKSPACE_ROOT
        # Per-app output: build/wasm/{projectCode}/  (projectCode = wink-micro-app/<name>)
        app_name = app_dir.name
        build_dir = ws_root / "build" / "wasm" / app_name
        sdk_mode = getattr(args, "sdk_mode", None)
        micro_os_dir = resolve_sdk_dir_for_build(sdk_mode)
        codegen_dir = micro_os_dir / "tools" / "codegen"

        if sdk_mode == "binary" and not (micro_os_dir / "libs" / "wasm").exists():
            print("[wink] Error: --sdk-mode binary but libs/wasm/ not found in SDK tree.", file=sys.stderr)
            sys.exit(1)

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
        # unisim_smoke exercises blocking PAL imports (js_* bridge coverage);
        # default wasm STRICT=1 would hide those declarations (ADR-0017).
        if app_name == "unisim_smoke":
            configure_cmd.append("-DWINK_STRICT_NONBLOCKING=0")

        if args.clean:
            import shutil
            if build_dir.exists():
                print(f"[wink] Cleaning build directory: {build_dir}")
                shutil.rmtree(build_dir)

        run_cmd(configure_cmd)
        run_cmd(["cmake", "--build", str(build_dir)])

        print(f"[wink] Success: WASM build complete for '{app_name}'. Output in {build_dir}")


def handle_esp32(args):
    """Build, flash or monitor ESP32 firmware."""
    app_dir = resolve_app_dir(args.app)
    # ESP32 always needs the Source SDK tree (targets/esp32 component). Prefer
    # the in-tree wink-micro-os even when a stale WINK_SDK_PATH still points at
    # a Binary SDK tarball from a prior experiment.
    env_sdk = os.environ.get("WINK_SDK_PATH", "").strip()
    if _is_in_tree_source_sdk(SDK_ROOT):
        sdk_dir = SDK_ROOT
        if env_sdk and Path(env_sdk).resolve() != SDK_ROOT.resolve():
            print(
                f"[wink] Note: ignoring WINK_SDK_PATH={env_sdk!r} for esp32; "
                f"using in-tree Source SDK {sdk_dir}",
                file=sys.stderr,
            )
    else:
        sdk_dir = resolve_sdk_dir()
        if (sdk_dir / "libs").is_dir() and not (sdk_dir / "targets" / "esp32").is_dir():
            print(
                "[wink] Error: esp32 builds require a Source SDK (targets/esp32). "
                f"WINK_SDK_PATH={sdk_dir} looks like a Binary SDK. "
                "Unset WINK_SDK_PATH or point it at a Source SDK tarball.",
                file=sys.stderr,
            )
            sys.exit(1)
    esp32_dir = resolve_esp32_dir(required=True)
    codegen_dir = Path(__file__).resolve().parent / "codegen"

    os.environ["WINK_APP_DIR"] = app_dir.as_posix()
    os.environ["WINK_SDK_PATH"] = sdk_dir.as_posix()

    # Ensure UTF-8 stdout in the child so the ✅ glyph doesn't mojibake on cp936.
    os.environ.setdefault("PYTHONUTF8", "1")
    os.environ.setdefault("PYTHONIOENCODING", "utf-8")

    gen_script = sdk_dir / "tools" / "esp32" / "generate_app_sources.py"
    run_cmd([
        sys.executable,
        str(gen_script),
        "--esp32-firmware-dir", str(esp32_dir),
        "--app-dir", str(app_dir),
    ])

    build_script = sdk_dir / "tools" / "esp32" / "build.py"
    idf_args = args.idf_args if args.idf_args else ["build"]
    # Strip a leading '--' if the user used one to disambiguate idf.py flags
    # that start with '-' (e.g. ``wink esp32 -- -- -p COM3 flash`` produces
    # idf_args=['--', '-p', 'COM3', 'flash']; pop the '--' so build.py sees
    # clean argv and can join its own REMAINDER after '-C <fw>').
    if idf_args and idf_args[0] == "--":
        idf_args = idf_args[1:]

    cmake_app_def = f"-DWINK_APP_DIR={app_dir.as_posix()}"
    cmake_sdk_def = f"-DWINK_SDK_PATH={sdk_dir.as_posix()}"
    cmake_codegen_def = f"-DWINK_CODEGEN_ROOT={codegen_dir.as_posix()}"
    idf_args = [cmake_app_def, cmake_sdk_def, cmake_codegen_def] + idf_args

    run_cmd([
        sys.executable,
        str(build_script),
        "--esp32-firmware-dir", str(esp32_dir),
        "--",
    ] + idf_args)
    print("[wink] Success: ESP32 Firmware build step complete!")


def handle_web(args):
    """Run Vue Vite frontend dev web server."""
    frontend_dir = resolve_frontend_dir(required=True)
    print("[wink] Starting Vue/Vite frontend dev server (press Ctrl+C to stop)...")
    run_cmd([
        "npm",
        "run",
        "dev",
        "--",
        "--port", str(args.port)
    ], cwd=frontend_dir)


def handle_test(args):
    """Run all Python golden and C unit tests."""
    print("[wink] Running Codegen Golden + validation regression tests...")
    sdk_dir = resolve_sdk_dir()
    # SDK root on PYTHONPATH so ``tools.codegen`` resolves under wink-micro-os/
    env = os.environ.copy()
    prev = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = str(sdk_dir) + (os.pathsep + prev if prev else "")
    tests_dir = sdk_dir / "tools" / "codegen" / "tests"
    print(
        f"\n[wink] Running: {sys.executable} -m unittest discover "
        f"-s {tests_dir} -p test_*.py (cwd={sdk_dir / 'tools' / 'codegen'}, "
        f"PYTHONPATH={sdk_dir})"
    )
    try:
        # cwd=tools/codegen so golden banner paths match checked-in fixtures
        # (app_codegen embeds Path.cwd()-relative config source).
        subprocess.run(
            [
                sys.executable, "-m", "unittest", "discover",
                "-s", str(tests_dir),
                "-p", "test_*.py",
            ],
            cwd=str(sdk_dir / "tools" / "codegen"),
            check=True,
            env=env,
        )
    except subprocess.CalledProcessError as e:
        print(f"[wink] Error: Codegen tests failed with exit code {e.returncode}", file=sys.stderr)
        sys.exit(e.returncode)

    print("\n[wink] Configuring and compiling Host unit tests...")
    ws_root = _APP_WORKSPACE_ROOT or WORKSPACE_ROOT
    build_dir = ws_root / "build" / "test"
    micro_os_dir = resolve_sdk_dir()

    if build_dir.exists():
        import shutil
        print(f"[wink] Cleaning test build directory: {build_dir}")
        shutil.rmtree(build_dir)

    codegen_dir = Path(__file__).resolve().parent / "codegen"
    configure_cmd = [
        "cmake",
        "-S", str(micro_os_dir),
        "-B", str(build_dir),
        "-DTARGET_PLATFORM=host",
        f"-DWINK_CODEGEN_ROOT={codegen_dir.as_posix()}"
    ]
    if sys.platform == "win32":
        configure_cmd.extend(["-G", "MinGW Makefiles"])

    run_cmd(configure_cmd)
    run_cmd(["cmake", "--build", str(build_dir)])

    import shutil
    if shutil.which("emcc") and shutil.which("emcmake"):
        print("\n[wink] Building WASM smoke test targets...")
        run_cmd(["cmake", "--build", str(build_dir), "--target", "wasm_unisim_smoke_build"])

    print("\n[wink] Running Host C Unit Tests (ctest)...")
    run_cmd([
        "ctest",
        "--test-dir", str(build_dir),
        "--output-on-failure"
    ])
    print("[wink] Success: All tests passed successfully!")


# ── Doctor / Setup handlers ────────────────────────────────────────────

# Fixed probe order for `wink doctor`. Fast local probes first, slow SDK
# probes last so the user sees progress instead of waiting on the ESP-IDF
# EIM profile subprocess (~12s cold).
_DOCTOR_PROBE_ORDER: list[str] = [
    "python",
    "jinja2",
    "cmake",
    "make",
    "gcc",
    "node",
    "emsdk",
    "idf",
]

# ANSI colour codes. When VT mode is unavailable these are still emitted;
# on a terminal that ignores them they render as short garbage that the user
# can decipher, which is worse than plain marks — so we blank them out when
# _VT_ENABLED is False.
if _VT_ENABLED:
    _ANSI_GREEN = "\033[32m"
    _ANSI_RED = "\033[31m"
    _ANSI_YELLOW = "\033[33m"
    _ANSI_BOLD = "\033[1m"
    _ANSI_DIM = "\033[2m"
    _ANSI_RESET = "\033[0m"
else:
    _ANSI_GREEN = _ANSI_RED = _ANSI_YELLOW = _ANSI_BOLD = _ANSI_DIM = _ANSI_RESET = ""


# Column widths for the doctor checklist. Tuned for ~80-col terminals but
# gracefully accepts overflow (we do not truncate the version suffix).
_COL_ITEM = 14
_COL_LOCATION = 48
_COL_STATUS = 20


def _truncate_middle(text: str, limit: int) -> str:
    """Return ``text`` shortened to ``limit`` chars with ``…`` in the middle."""
    if len(text) <= limit:
        return text
    if limit <= 3:
        return text[:limit]
    keep = limit - 1  # 1 char for the ellipsis
    left = keep // 2
    right = keep - left
    return text[:left] + "…" + text[-right:]


def _doctor_required_and_optional() -> tuple[set[str], set[str]]:
    """Return (required_caps, optional_caps) sets aggregated over profiles.

    A cap is *required overall* if any profile lists it as required. A cap is
    *optional overall* if it appears only in some profile's optional list.
    Matches the semantics used by ``ensure_for("doctor", ...)`` in
    :mod:`tools.toolchain.check`.
    """
    from tools.toolchain.profiles import (  # noqa: WPS433
        OPTIONAL_CAPS,
        PROFILES,
        expand_profile,
    )
    required: set[str] = set()
    optional: set[str] = set()
    for profile in PROFILES:
        full = expand_profile(profile)
        opt = set(OPTIONAL_CAPS.get(profile, []))
        required |= {c for c in full if c not in opt}
        optional |= opt
    optional -= required
    return required, optional


def _format_location(cap_id: str, result) -> str:
    """Return the "Location" cell text for a single cap result.

    Uses the truncate-middle rule so long MinGW / IDF paths still fit. The
    version and any provider-specific extra info (gcc triplet, idf source
    tag) are appended in parentheses.
    """
    from tools.toolchain.types import DetectResult  # noqa: WPS433
    if not isinstance(result, DetectResult):
        return "(unknown)"

    if not result.found:
        # Prefer the short reason; if none, generic "not installed".
        reason = (result.reason or "not installed").strip()
        # Reasons like "gcc not found on PATH" are already short; longer
        # ones (bad triplet, IDF version mismatch) get truncated cleanly.
        return _truncate_middle(reason, _COL_LOCATION)

    # Found: build "<path> (<version>[, <extra>])".
    path_str = str(result.path) if result.path else "(no path)"
    extras: list[str] = []
    if result.version:
        extras.append(result.version)

    if cap_id == "gcc" and result.path is not None:
        triplet = _gcc_dumpmachine(result.path)
        if triplet:
            extras.append(triplet)
    elif cap_id == "idf":
        # Surface where the IDF was located (EIM profile / env / PATH). This
        # is the field the user cares about when debugging IDF activation.
        src = (result.source or "").strip()
        if src == "eim-profile":
            extras.append("via EIM profile")
        elif src == "env:IDF_PATH":
            extras.append("via IDF_PATH")
        elif src == "path":
            extras.append("via PATH")
    elif cap_id == "jinja2":
        # jinja2 has no filesystem path of its own; show which interpreter
        # imported it. The path we set on DetectResult IS that interpreter.
        path_str = f"imported via {path_str}"

    suffix = f" ({', '.join(extras)})" if extras else ""
    combined = path_str + suffix
    return _truncate_middle(combined, _COL_LOCATION)


def _gcc_dumpmachine(gcc_exe: Path) -> str | None:
    """Return the GCC target triplet, or None if the probe fails.

    A small extra ~50 ms subprocess call. Only invoked from doctor when gcc
    is found; other code paths never pay this cost.
    """
    try:
        cp = subprocess.run(
            [str(gcc_exe), "-dumpmachine"],
            capture_output=True, text=True, timeout=5,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if cp.returncode != 0:
        return None
    line = (cp.stdout or "").strip().splitlines()
    return line[0] if line else None


def _doctor_status_cell(
    cap_id: str,
    result,
    required: set[str],
    optional: set[str],
) -> tuple[str, bool]:
    """Return (status_cell_string, is_blocking_failure)."""
    # is_blocking_failure is True iff the cap is required AND result.found is False.
    is_optional = cap_id in optional and cap_id not in required
    if result.found:
        mark = f"{_ANSI_GREEN}✓{_ANSI_RESET}"
        tag = "  (optional)" if is_optional else ""
        return f"{mark}{tag}", False
    if is_optional:
        mark = f"{_ANSI_YELLOW}!{_ANSI_RESET}"
        return f"{mark}  (optional)", False
    mark = f"{_ANSI_RED}✗{_ANSI_RESET}"
    return mark, True


def _pad_visible(text: str, width: int) -> str:
    """Pad ``text`` on the right to ``width`` visible columns.

    Strips ANSI colour escapes when measuring so coloured cells still align.
    """
    import re
    visible = re.sub(r"\x1b\[[0-9;]*m", "", text)
    pad = width - len(visible)
    if pad <= 0:
        return text
    return text + " " * pad


def _print_doctor_row(item: str, location: str, status: str) -> None:
    """Emit one checklist row and flush stdout so the user sees streaming output."""
    line = (
        f"{_pad_visible(item, _COL_ITEM)}"
        f" {_pad_visible(location, _COL_LOCATION)}"
        f" {status}"
    )
    print(line)
    try:
        sys.stdout.flush()
    except Exception:
        pass


def handle_doctor(args):
    """Probe each registered capability, streaming a checklist row per probe.

    Exit code semantics (spec §9.1 / ADR-0029): exit 1 iff any *required*
    capability (aggregated across profiles) is missing; optional caps are
    warnings only. See :func:`_doctor_required_and_optional`.
    """
    if args.skip_toolchain_check:
        print(
            "[wink] Note: --skip-toolchain-check is ignored for `doctor` "
            "(probing is the whole point of doctor).",
            file=sys.stderr,
        )

    from tools.toolchain import providers as providers_mod  # noqa: WPS433
    from tools.toolchain.resolve import ResolveContext  # noqa: WPS433
    from tools.toolchain.types import DetectResult  # noqa: WPS433

    ctx = ResolveContext.snapshot(_current_workspace_root())
    required, optional = _doctor_required_and_optional()

    # Header
    hr = "─" * (_COL_ITEM + _COL_LOCATION + _COL_STATUS + 2)
    print(f"{_ANSI_BOLD}Wink toolchain doctor{_ANSI_RESET}")
    print(hr)
    _print_doctor_row("Item", "Location", "Status")
    print(hr)
    try:
        sys.stdout.flush()
    except Exception:
        pass

    # Probe in the canonical order; if REGISTRY contains extras (e.g. a test
    # fake), append them after the canonical list so nothing is dropped.
    seen: set[str] = set()
    ordered_ids: list[str] = []
    for cap_id in _DOCTOR_PROBE_ORDER:
        if cap_id in providers_mod.REGISTRY and cap_id not in seen:
            ordered_ids.append(cap_id)
            seen.add(cap_id)
    for cap_id in providers_mod.REGISTRY:
        if cap_id not in seen:
            ordered_ids.append(cap_id)
            seen.add(cap_id)

    results: dict[str, DetectResult] = {}
    blocking_missing: list[str] = []
    optional_missing: list[str] = []

    for cap_id in ordered_ids:
        provider = providers_mod.REGISTRY[cap_id]
        try:
            result = provider.detect(ctx)
        except Exception as exc:  # noqa: BLE001 — defensive; report and continue
            result = DetectResult(
                found=False, path=None, version=None,
                reason=f"provider raised: {exc}", source=None,
            )
        results[cap_id] = result
        location = _format_location(cap_id, result)
        status, is_blocking = _doctor_status_cell(cap_id, result, required, optional)
        _print_doctor_row(cap_id, location, status)
        if not result.found:
            if is_blocking:
                blocking_missing.append(cap_id)
            else:
                optional_missing.append(cap_id)

    print(hr)

    total = len(ordered_ids)
    installed = sum(1 for r in results.values() if r.found)
    missing_all = blocking_missing + optional_missing
    missing_count = len(missing_all)

    summary_line = (
        f"{_ANSI_BOLD}Summary:{_ANSI_RESET} "
        f"{total} checked, {installed} installed, {missing_count} missing"
    )
    print(summary_line)

    # Actionable hints for each missing cap (blocking first, then optional).
    if missing_all:
        print()
        idx = 1
        for cap_id in blocking_missing:
            provider = providers_mod.REGISTRY[cap_id]
            try:
                hint_text = provider.hint(ctx)
            except Exception:  # noqa: BLE001
                hint_text = "(no hint available)"
            print(f"  {idx}. {_ANSI_RED}{cap_id}{_ANSI_RESET} — {hint_text}")
            idx += 1
        for cap_id in optional_missing:
            provider = providers_mod.REGISTRY[cap_id]
            try:
                hint_text = provider.hint(ctx)
            except Exception:  # noqa: BLE001
                hint_text = "(no hint available)"
            print(
                f"  {idx}. {_ANSI_YELLOW}{cap_id}{_ANSI_RESET} "
                f"(optional) — {hint_text}"
            )
            idx += 1

        # Spec §8.2: ESP-IDF is never auto-installed by Wink.
        if "idf" in blocking_missing:
            print()
            print(
                "Note: ESP-IDF is never auto-installed by Wink. "
                "Install via Espressif IDF Manager (EIM); see preinstall.md §3."
            )

    try:
        sys.stdout.flush()
    except Exception:
        pass

    if blocking_missing:
        sys.exit(1)


def _probe_all_for_setup():
    """Probe every registered cap and return a list of (id, DetectResult, source_tag).

    Used by ``handle_setup`` (no-args view). Silences per-provider exceptions:
    a broken provider must not prevent showing the rest of the table.
    """
    from tools.toolchain import providers as providers_mod  # noqa: WPS433
    from tools.toolchain.resolve import ResolveContext, candidate_paths  # noqa: WPS433
    from tools.toolchain.types import DetectResult  # noqa: WPS433

    ctx = ResolveContext.snapshot(_current_workspace_root())
    rows: list[tuple[str, "DetectResult", str]] = []
    for cap_id, provider in providers_mod.REGISTRY.items():
        # Determine source tag from candidate_paths; if empty, fall back to
        # "PATH" (found) / "—" (not found) after detect().
        cands = candidate_paths(cap_id, ctx)
        try:
            result = provider.detect(ctx)
        except Exception as exc:  # noqa: BLE001 — defensive; must not break UI
            result = DetectResult(
                found=False, path=None, version=None,
                reason=f"provider raised: {exc}", source=None,
            )
        source_tag = ""
        if result.found:
            if cands:
                source_tag = cands[0][0]  # env:X or config:workspace/user
            elif result.source:
                source_tag = result.source
            else:
                source_tag = "PATH"
        else:
            source_tag = "—"
        rows.append((cap_id, result, source_tag))
    return rows, ctx


def handle_setup(args):
    """Setup subcommand dispatcher.

    - No args              → print resolved-toolchain YAML-like table.
    - --set key=value      → validate then write.
    - --install cap        → phase-B stub, prints the hint.
    """
    # Validate mutually exclusive nature
    if args.set_kv and args.install_cap:
        print("[wink] Error: --set and --install are mutually exclusive.", file=sys.stderr)
        sys.exit(2)

    if args.install_cap:
        _handle_setup_install(args.install_cap)
        return

    if args.set_kv:
        _handle_setup_set(args.set_kv, workspace=args.workspace)
        return

    _handle_setup_noargs()


def _handle_setup_noargs() -> None:
    """Print the fully-resolved toolchain view (spec §10.3)."""
    from tools.toolchain.config import (  # noqa: WPS433
        _user_config_path, _workspace_config_path,
    )

    rows, ctx = _probe_all_for_setup()
    print("Wink toolchain resolution:")
    # Widths tuned to keep the table readable on typical Windows terminals
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
    ws_root = _current_workspace_root()
    ws_p = _workspace_config_path(ws_root) if ws_root else None
    user_state = "present" if user_p.exists() else "not present"
    print(f"  user      : {user_p} ({user_state})")
    if ws_p is not None:
        ws_state = "present" if ws_p.exists() else "not present"
        print(f"  workspace : {ws_p} ({ws_state})")


# Capability IDs we accept for `setup --set`.
_TOOL_CAP_KEYS = {"python", "jinja2", "gcc", "cmake", "make", "emsdk", "idf", "node"}
_TOOLS_HOME_KEY = "tools_home"
_WORKSPACE_PATH_KEYS = set(_WORKSPACE_KEY_RESOLVERS.keys())


def _handle_setup_set(kv: str, *, workspace: bool) -> None:
    """Validate and persist a single ``paths[key] = value`` update.

    Rejects unknown keys, empty values, and (for tool caps) paths that fail
    provider.detect() against a temporary snapshot with the new value.
    """
    from tools.toolchain import providers as providers_mod  # noqa: WPS433
    from tools.toolchain.config import (  # noqa: WPS433
        save_user_path,
        save_workspace_layout_key,
        save_workspace_path,
    )
    from tools.toolchain.resolve import ResolveContext, CAP_ENV_VARS  # noqa: WPS433

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
        print(
            f"[wink] Error: unknown key {key!r}. Allowed: {', '.join(allowed)}.",
            file=sys.stderr,
        )
        sys.exit(2)

    # Tool cap: run provider.detect() against a context that reflects the
    # candidate value in the appropriate env var (fallback: workspace/user
    # override). We do NOT persist first — a bad path is rejected up front.
    if key in _TOOL_CAP_KEYS:
        provider = providers_mod.REGISTRY.get(key)
        if provider is None:
            print(f"[wink] Error: no provider registered for {key!r}.", file=sys.stderr)
            sys.exit(2)

        env_var = CAP_ENV_VARS.get(key)
        # Build a probe context that pretends the new value is set.
        ctx = ResolveContext.snapshot(_current_workspace_root())
        # candidate_paths reads env first; workspace_paths next; user_paths last.
        # Simplest injection: shove the value into the top-priority slot.
        if env_var:
            ctx.environ[env_var] = value
        else:
            ctx.workspace_paths[key] = value
        result = provider.detect(ctx)
        if not result.found:
            reason = result.reason or "detect() reported not-found"
            print(
                f"[wink] Error: validation failed for {key}={value!r}: {reason}. "
                "Config NOT written.",
                file=sys.stderr,
            )
            sys.exit(1)

        if workspace:
            ws_root = _current_workspace_root()
            if ws_root is None:
                print("[wink] Error: --workspace requested but no workspace root resolved.", file=sys.stderr)
                sys.exit(1)
            target = save_workspace_path(ws_root, key, value)
        else:
            target = save_user_path(key, value)
        print(f"[wink] wrote {key}={value} to {target}")
        return

    # Workspace layout keys → wink-workspace.json (NOT tools.json).
    if key in _WORKSPACE_PATH_KEYS:
        p = Path(value)
        if not p.exists() or not p.is_dir():
            print(
                f"[wink] Error: workspace path {key}={value!r} does not exist or is not a directory. "
                "Config NOT written.",
                file=sys.stderr,
            )
            sys.exit(1)
        ws_root = _current_workspace_root()
        if ws_root is None:
            print(
                "[wink] Error: cannot resolve workspace root to write wink-workspace.json. "
                "Run from the monorepo / set WINK_* paths.",
                file=sys.stderr,
            )
            sys.exit(1)
        target = save_workspace_layout_key(ws_root, key, value)
        print(f"[wink] wrote {key}={value} to {target}")
        return

    # tools_home: also require the dir exists (or can be created), but do not
    # try to enforce provider detect — it's a *base* dir, not a specific tool.
    if key == _TOOLS_HOME_KEY:
        p = Path(value)
        if not p.exists():
            print(
                f"[wink] Error: tools_home={value!r} does not exist. Create it first "
                "or point at an existing directory. Config NOT written.",
                file=sys.stderr,
            )
            sys.exit(1)
        if workspace:
            ws_root = _current_workspace_root()
            if ws_root is None:
                print("[wink] Error: --workspace requested but no workspace root resolved.", file=sys.stderr)
                sys.exit(1)
            target = save_workspace_path(ws_root, key, value)
        else:
            target = save_user_path(key, value)
        print(f"[wink] wrote {key}={value} to {target}")
        return

    # Unreachable: key already validated against the three allowed sets.
    print(f"[wink] Error: unhandled key {key!r}.", file=sys.stderr)
    sys.exit(2)


def _handle_setup_install(cap: str) -> None:
    """Phase-B placeholder for automatic installation.

    Never installs anything today; prints the provider's hint and, for
    ``idf``, the ADR-0030 "never auto-install" notice.
    """
    from tools.toolchain import providers as providers_mod  # noqa: WPS433
    from tools.toolchain.resolve import ResolveContext  # noqa: WPS433

    if cap not in providers_mod.REGISTRY:
        print(
            f"[wink] Error: unknown capability {cap!r}. "
            f"Known: {', '.join(sorted(providers_mod.REGISTRY))}.",
            file=sys.stderr,
        )
        sys.exit(2)

    provider = providers_mod.REGISTRY[cap]
    ctx = ResolveContext.snapshot(_current_workspace_root())
    try:
        hint_text = provider.hint(ctx)
    except Exception:  # noqa: BLE001
        hint_text = "(no hint available)"

    if cap == "idf":
        print(
            "[wink] ESP-IDF is never auto-installed by Wink (ADR-0030). "
            "Please install via Espressif IDF Manager (EIM); "
            "see wink-micro-os/tools/preinstall.md §3."
        )
        print(f"[wink] hint: {hint_text}")
        return

    print(
        f"[wink] Automatic install is not yet implemented (phase B). "
        f"Manual step: {hint_text}"
    )


# ── Main Entry ────────────────────────────────────────────────────────

def _build_parser() -> argparse.ArgumentParser:
    """Build the top-level ArgumentParser (extracted for testability)."""
    # Parent parser: global flags visible on the top-level command and on
    # every subcommand via ``parents=[global_parent]``.
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

    p_gen = sub.add_parser("gen", parents=[global_parent],
                           help="Run device tree & config macro codegen")
    p_gen.add_argument("--app", default="oled_dashboard",
                       help="App name in samples/ or path to app directory")
    p_gen.set_defaults(handler=handle_gen)

    p_build = sub.add_parser("build", parents=[global_parent],
                             help="Build Host or WASM simulators")
    p_build.add_argument("target", choices=["host", "wasm"],
                         help="Build target platform")
    p_build.add_argument("--app", default="oled_dashboard",
                         help="App name in samples/ or path to app directory")
    p_build.add_argument("--clean", action="store_true",
                         help="Clean the build directory before building (host only)")
    p_build.add_argument("--sdk-mode", choices=["source", "binary"], default=None,
                         help="SDK mode: 'source' (build from source) or 'binary' (use precompiled .a). "
                              "Default: auto-detect from SDK tree.")
    p_build.set_defaults(handler=handle_build)

    p_esp = sub.add_parser("esp32", parents=[global_parent],
                           help="Build, flash, or monitor ESP32 firmware")
    p_esp.add_argument("--app", default="devkitc_smoke",
                       help="App name in samples/ or path to app directory")
    p_esp.add_argument("idf_args", nargs="*", default=["build"],
                       help="Arguments forwarded to idf.py. Plain subcommands "
                            "(build, flash, monitor, fullclean) can be passed "
                            "directly; for idf.py args that start with '-' "
                            "(like -p, -v, -b, -D), place '--' before them so "
                            "argparse stops parsing wink flags, e.g. "
                            "'wink esp32 --app foo -- -p COM3 flash monitor'. "
                            "Default: 'build'.")
    p_esp.set_defaults(handler=handle_esp32)

    p_web = sub.add_parser("web", parents=[global_parent],
                           help="Start Vue Vite frontend web server")
    p_web.add_argument("--port", type=int, default=5173,
                       help="Vite server port (default: 5173)")
    p_web.set_defaults(handler=handle_web)

    p_test = sub.add_parser("test", parents=[global_parent],
                            help="Run Python and C unit tests")
    p_test.set_defaults(handler=handle_test)

    p_doctor = sub.add_parser("doctor", parents=[global_parent],
                              help="Probe every registered toolchain capability")
    p_doctor.set_defaults(handler=handle_doctor)

    p_setup = sub.add_parser("setup", parents=[global_parent],
                             help="Inspect or edit ~/.wink/tools.json")
    p_setup.add_argument("--set", dest="set_kv", metavar="KEY=VALUE", default=None,
                         help="Validate and write paths[KEY]=VALUE.")
    p_setup.add_argument("--workspace", action="store_true",
                         help="With --set, write to <workspace>/.wink/tools.json instead of user config.")
    p_setup.add_argument("--install", dest="install_cap", metavar="CAP", default=None,
                         help="Attempt automatic install (phase B; currently prints hint only).")
    p_setup.set_defaults(handler=handle_setup)

    # Lazy handler: keep PyYAML (and the whole lint engine) out of the import
    # path for unrelated commands like `build` / `gen`. Only `wink lint`
    # requires it, and we surface a friendly hint if the dep is missing.
    def _lazy_handle_lint(args):
        try:
            from tools.lint.cli import handle_lint  # noqa: WPS433
        except ModuleNotFoundError as exc:
            if exc.name == "yaml":
                print(
                    "[wink lint] PyYAML is required. Install it with:\n"
                    "    python -m pip install -r wink-micro-os/tools/requirements-lint.txt\n"
                    "  (or: python -m pip install \"PyYAML>=6\")",
                    file=sys.stderr,
                )
                sys.exit(2)
            raise
        handle_lint(args)

    p_lint = sub.add_parser("lint", parents=[global_parent],
                            help="Run YAML layer/API/Arduino lints (ADR-0043)")
    p_lint.add_argument("--root", default=None,
                        help="SDK root to scan (default: wink-micro-os/)")
    p_lint.add_argument("--config", action="append", default=[],
                        help="Extra YAML config path (repeatable)")
    p_lint.add_argument("--pack", action="append", default=None,
                        help="Rule pack id to run (repeatable; default: layering+api)")
    p_lint.add_argument("--rule", default=None,
                        help="Only report findings for this rule id")
    p_lint.add_argument("--paths", nargs="*", default=None,
                        help="Incremental scan: only these files")
    p_lint.add_argument("--changed", nargs="?", const="HEAD", default=None,
                        help="Derive --paths from git diff --name-only [REV] (default HEAD)")
    p_lint.add_argument("--format", choices=["text", "json", "sarif"], default="text",
                        help="Output format (default: text)")
    p_lint.add_argument("--output", default=None,
                        help="Write report to FILE instead of stdout")
    p_lint.add_argument("--strict", action="store_true",
                        help="Treat warnings as failures")
    p_lint.add_argument("--explain", metavar="RULE_ID", default=None,
                        help="Print rule explanation and exit 0")
    p_lint.add_argument("--report-allowlist", action="store_true",
                        help="Report allowlisted / expiring allow_paths entries")
    p_lint.add_argument("--baseline", default=None,
                        help="Optional baseline file for fingerprint diff (later)")
    p_lint.add_argument("--today", default=None,
                        help="Override today for until expiry (YYYY-MM-DD); also $WINK_LINT_TODAY")
    p_lint.set_defaults(handler=_lazy_handle_lint)

    return p


def main():
    p = _build_parser()

    # argparse quirk: with parents=[global_parent] on subparsers, the
    # subparser's default (False) for --skip-toolchain-check overwrites the
    # top-level's value when the flag appears BEFORE the subcommand. Preserve
    # OR semantics with a manual pre-pass on argv so both orderings work.
    argv = sys.argv[1:]
    skip_from_prepass = "--skip-toolchain-check" in argv

    args = p.parse_args()
    if skip_from_prepass:
        args.skip_toolchain_check = True

    # doctor / setup / lint are diagnostic commands: doctor calls
    # ensure_for("doctor") from its handler; setup and lint do no gating
    # (must still work when toolchains are missing). Every other command
    # must pass the ensure_for gate before its handler runs.
    if args.command not in ("doctor", "setup", "lint"):
        gate_command = _resolve_gate_command(args)
        _apply_toolchain_gate(gate_command, skip=args.skip_toolchain_check)

    args.handler(args)


if __name__ == "__main__":
    main()
