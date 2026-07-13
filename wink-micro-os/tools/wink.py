#!/usr/bin/env python3
"""wink.py — Unified build orchestrator and CLI gateway for Wink Micro OS.

Supports Host, WASM, and ESP32 platforms with unified custom application path support.
Supports workspace config 'wink-workspace.json' for full directory relocatability.

This CLI lives inside the SDK tree (wink-micro-os/tools/). Sibling monorepo
dirs (frontend, esp32_firmware, apps) are resolved via env / workspace config /
SDK parent defaults.
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

# SDK root = wink-micro-os/ (parent of tools/)
SDK_ROOT = Path(__file__).resolve().parent.parent
# Default monorepo / workspace root = parent of the SDK package
WORKSPACE_ROOT = SDK_ROOT.parent

# Set up toolchain path on Windows (WinLibs MinGW)
if sys.platform == "win32":
    mingw_bin = r"C:\Users\77174\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
    if os.path.exists(mingw_bin):
        os.environ["PATH"] = mingw_bin + os.pathsep + os.environ.get("PATH", "")


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
    """
    path_opt = Path(app)
    if path_opt.exists() and path_opt.is_dir():
        resolved = path_opt.resolve()
        if (resolved / "wink-app.json").exists():
            return resolved

    micro_app_dir = WORKSPACE_ROOT / "wink-micro-app" / app
    if micro_app_dir.exists() and micro_app_dir.is_dir():
        resolved = micro_app_dir.resolve()
        if (resolved / "wink-app.json").exists():
            return resolved

    sdk_dir = resolve_sdk_dir()
    samples_dir = sdk_dir / "samples" / app
    if samples_dir.exists() and samples_dir.is_dir():
        resolved = samples_dir.resolve()
        if (resolved / "wink-app.json").exists():
            return resolved

    if path_opt.exists() and path_opt.is_dir():
        print(f"[wink] Error: Directory '{app}' exists but does not contain 'wink-app.json'.", file=sys.stderr)
        sys.exit(1)

    print(f"[wink] Error: Cannot resolve App '{app}' as a path containing 'wink-app.json' or as a sample in '{sdk_dir}/samples/'", file=sys.stderr)
    sys.exit(1)


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
        build_dir = ws_root / "build" / "wasm"
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

        if args.clean:
            import shutil
            if build_dir.exists():
                print(f"[wink] Cleaning build directory: {build_dir}")
                shutil.rmtree(build_dir)

        run_cmd(configure_cmd)
        run_cmd(["cmake", "--build", str(build_dir)])

        app_name = app_dir.name
        frontend_dir = resolve_frontend_dir(required=False)
        if frontend_dir and frontend_dir.exists():
            meta_dir = frontend_dir / "public" / "wasm" / app_name
            meta_dir.mkdir(parents=True, exist_ok=True)
            (meta_dir / "wasm-app-id.txt").write_text(f"{app_name}\n", encoding="utf-8")
            print(f"[wink] Wrote {meta_dir / 'wasm-app-id.txt'}")

        print(f"[wink] Success: WASM build complete for '{app_name}'. Output in {build_dir}")


def handle_esp32(args):
    """Build, flash or monitor ESP32 firmware."""
    app_dir = resolve_app_dir(args.app)
    sdk_dir = resolve_sdk_dir()
    esp32_dir = resolve_esp32_dir(required=True)
    scripts_dir = resolve_scripts_dir(required=True)
    codegen_dir = Path(__file__).resolve().parent / "codegen"

    os.environ["WINK_APP_DIR"] = app_dir.as_posix()
    os.environ["WINK_SDK_PATH"] = sdk_dir.as_posix()

    gen_script = esp32_dir / "generate_app_sources.ps1"
    run_cmd([
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", str(gen_script),
        "-AppDir", str(app_dir)
    ])

    build_script = scripts_dir / "build_esp32.ps1"
    idf_args = args.idf_args if args.idf_args else ["build"]

    cmake_app_def = f"-DWINK_APP_DIR={app_dir.as_posix()}"
    cmake_sdk_def = f"-DWINK_SDK_PATH={sdk_dir.as_posix()}"
    cmake_codegen_def = f"-DWINK_CODEGEN_ROOT={codegen_dir.as_posix()}"
    idf_args = [cmake_app_def, cmake_sdk_def, cmake_codegen_def] + idf_args

    run_cmd([
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", str(build_script)
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
    print("[wink] Running Codegen Golden regression tests...")
    sdk_dir = resolve_sdk_dir()
    # SDK root on PYTHONPATH so ``tools.codegen`` resolves under wink-micro-os/
    env = os.environ.copy()
    prev = env.get("PYTHONPATH", "")
    env["PYTHONPATH"] = str(sdk_dir) + (os.pathsep + prev if prev else "")
    golden = sdk_dir / "tools" / "codegen" / "tests" / "test_golden.py"
    print(f"\n[wink] Running: {sys.executable} {golden} (PYTHONPATH={sdk_dir})")
    try:
        subprocess.run(
            [sys.executable, str(golden)],
            cwd=WORKSPACE_ROOT,
            check=True,
            env=env,
        )
    except subprocess.CalledProcessError as e:
        print(f"[wink] Error: Golden tests failed with exit code {e.returncode}", file=sys.stderr)
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


# ── Main Entry ────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(
        prog="wink",
        description="Unified build orchestrator CLI for Wink Micro OS."
    )
    sub = p.add_subparsers(dest="command", required=True, help="Subcommand to execute")

    p_gen = sub.add_parser("gen", help="Run device tree & config macro codegen")
    p_gen.add_argument("--app", default="oled_dashboard",
                       help="App name in samples/ or path to app directory")
    p_gen.set_defaults(handler=handle_gen)

    p_build = sub.add_parser("build", help="Build Host or WASM simulators")
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

    p_esp = sub.add_parser("esp32", help="Build, flash, or monitor ESP32 firmware")
    p_esp.add_argument("--app", default="devkitc_smoke",
                       help="App name in samples/ or path to app directory")
    p_esp.add_argument("idf_args", nargs="*", default=["build"],
                       help="Arguments forwarded to idf.py (e.g. build, flash, monitor)")
    p_esp.set_defaults(handler=handle_esp32)

    p_web = sub.add_parser("web", help="Start Vue Vite frontend web server")
    p_web.add_argument("--port", type=int, default=5173,
                       help="Vite server port (default: 5173)")
    p_web.set_defaults(handler=handle_web)

    p_test = sub.add_parser("test", help="Run Python and C unit tests")
    p_test.set_defaults(handler=handle_test)

    args = p.parse_args()
    args.handler(args)


if __name__ == "__main__":
    main()
