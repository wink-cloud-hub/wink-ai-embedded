#!/usr/bin/env python3
"""wink.py — Unified build orchestrator and CLI gateway for Wink Micro OS.

Supports Host, WASM, and ESP32 platforms with unified custom application path support.
Supports workspace config 'wink-workspace.json' for full directory relocatability.
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# Set up toolchain path on Windows (WinLibs MinGW)
if sys.platform == "win32":
    mingw_bin = r"C:\Users\77174\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
    if os.path.exists(mingw_bin):
        os.environ["PATH"] = mingw_bin + os.pathsep + os.environ.get("PATH", "")


def load_workspace_config() -> dict:
    """Loads workspace config from 'wink-workspace.json' if present in os.getcwd() or REPO_ROOT."""
    candidates = [
        Path(os.getcwd()) / "wink-workspace.json",
        REPO_ROOT / "wink-workspace.json"
    ]
    for c in candidates:
        if c.exists():
            try:
                with c.open("r", encoding="utf-8") as fp:
                    return json.load(fp)
            except Exception as e:
                print(f"[wink] Warning: failed to parse '{c}': {e}", file=sys.stderr)
    return {}


CONFIG = load_workspace_config()


def resolve_sdk_dir() -> Path:
    """Resolve the SDK (wink-micro-os) directory."""
    # 1. Environment variable
    env_val = os.environ.get("WINK_SDK_PATH")
    if env_val:
        return Path(env_val).resolve()
    # 2. Config file
    config_val = CONFIG.get("sdk_dir")
    if config_val:
        return Path(config_val).resolve()
    # 3. Default monorepo path
    default_path = REPO_ROOT / "wink-micro-os"
    if default_path.exists():
        return default_path
    
    print("[wink] Error: Cannot resolve Wink Micro OS SDK directory. Set WINK_SDK_PATH environment variable, sdk_dir in wink-workspace.json, or run in the monorepo.", file=sys.stderr)
    sys.exit(1)


def resolve_frontend_dir() -> Path:
    """Resolve the embedded-frontend directory."""
    env_val = os.environ.get("WINK_FRONTEND_PATH")
    if env_val:
        return Path(env_val).resolve()
    config_val = CONFIG.get("frontend_dir")
    if config_val:
        return Path(config_val).resolve()
    default_path = REPO_ROOT / "embedded-frontend"
    if default_path.exists():
        return default_path
        
    print("[wink] Error: Cannot resolve embedded-frontend directory. Set WINK_FRONTEND_PATH environment variable, frontend_dir in wink-workspace.json, or run in the monorepo.", file=sys.stderr)
    sys.exit(1)


def resolve_esp32_dir() -> Path:
    """Resolve the esp32_firmware directory."""
    env_val = os.environ.get("WINK_ESP32_PATH")
    if env_val:
        return Path(env_val).resolve()
    config_val = CONFIG.get("esp32_dir")
    if config_val:
        return Path(config_val).resolve()
    default_path = REPO_ROOT / "esp32_firmware"
    if default_path.exists():
        return default_path
        
    print("[wink] Error: Cannot resolve esp32_firmware directory. Set WINK_ESP32_PATH environment variable, esp32_dir in wink-workspace.json, or run in the monorepo.", file=sys.stderr)
    sys.exit(1)


def resolve_scripts_dir() -> Path:
    """Resolve the build scripts directory."""
    env_val = os.environ.get("WINK_SCRIPTS_PATH")
    if env_val:
        return Path(env_val).resolve()
    config_val = CONFIG.get("scripts_dir")
    if config_val:
        return Path(config_val).resolve()
    default_path = REPO_ROOT / "scripts"
    if default_path.exists():
        return default_path
    
    fallback_path = REPO_ROOT / "scripts"
    if fallback_path.exists():
        return fallback_path
        
    print("[wink] Error: Cannot resolve scripts directory. Set WINK_SCRIPTS_PATH environment variable, scripts_dir in wink-workspace.json, or run in the monorepo.", file=sys.stderr)
    sys.exit(1)


# Export global path environment variables to propagate them to all child subprocesses
os.environ["WINK_SDK_PATH"] = str(resolve_sdk_dir().as_posix())
os.environ["WINK_FRONTEND_PATH"] = str(resolve_frontend_dir().as_posix())
os.environ["WINK_ESP32_PATH"] = str(resolve_esp32_dir().as_posix())
os.environ["WINK_SCRIPTS_PATH"] = str(resolve_scripts_dir().as_posix())
os.environ["WINK_CODEGEN_ROOT"] = str((Path(__file__).resolve().parent / "codegen").as_posix())


def run_cmd(cmd: list[str] | str, cwd: Path = REPO_ROOT, shell: bool = False, check: bool = True) -> subprocess.CompletedProcess:
    """Helper to run a system command and print output."""
    cmd_str = " ".join(cmd) if isinstance(cmd, list) else cmd
    print(f"\n[wink] Running: {cmd_str} (in {cwd.relative_to(REPO_ROOT) if cwd != REPO_ROOT and REPO_ROOT in cwd.parents else cwd})")
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
    Otherwise, defaults to SDK_DIR/samples/<app>.
    """
    path_opt = Path(app)
    if path_opt.exists() and path_opt.is_dir():
        resolved = path_opt.resolve()
        if (resolved / "wink-app.json").exists():
            return resolved
    
    # Check default wink-micro-app path relative to REPO_ROOT
    micro_app_dir = REPO_ROOT / "wink-micro-app" / app
    if micro_app_dir.exists() and micro_app_dir.is_dir():
        resolved = micro_app_dir.resolve()
        if (resolved / "wink-app.json").exists():
            return resolved

    # Check default samples path relative to resolved SDK
    sdk_dir = resolve_sdk_dir()
    samples_dir = sdk_dir / "samples" / app
    if samples_dir.exists() and samples_dir.is_dir():
        resolved = samples_dir.resolve()
        if (resolved / "wink-app.json").exists():
            return resolved
            
    # If the specified path is a directory but missing wink-app.json
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
        
    out_dir = REPO_ROOT / "build" / "generated"
    # app_codegen.py stays side-by-side with wink.py under tools/
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
        build_dir = REPO_ROOT / "build-host"
        micro_os_dir = resolve_sdk_dir()
        codegen_dir = Path(__file__).resolve().parent / "codegen"
        
        # 1. Configure CMake
        configure_cmd = [
            "cmake",
            "-S", str(micro_os_dir),
            "-B", str(build_dir),
            "-DTARGET_PLATFORM=host",
            f"-DWINK_APP_DIR={app_dir.as_posix()}",
            f"-DWINK_CODEGEN_ROOT={codegen_dir.as_posix()}"
        ]
        if sys.platform == "win32":
            configure_cmd.extend(["-G", "MinGW Makefiles"])
            
        if args.clean:
            # Remove build directory first
            import shutil
            if build_dir.exists():
                print(f"[wink] Cleaning build directory: {build_dir}")
                shutil.rmtree(build_dir)
                
        run_cmd(configure_cmd)
        
        # 2. Build target
        run_cmd(["cmake", "--build", str(build_dir)])
        print(f"[wink] Success: Host simulator build complete. Output is in {build_dir}")
        
    elif args.target == "wasm":
        frontend_dir = resolve_frontend_dir()
        wasm_script = frontend_dir / "scripts" / "build-wasm.mjs"
        # Run node build-wasm.mjs <app_dir>
        run_cmd([
            "node",
            str(wasm_script),
            str(app_dir)
        ], cwd=frontend_dir)
        print("[wink] Success: WASM simulator compilation complete!")


def handle_esp32(args):
    """Build, flash or monitor ESP32 firmware."""
    app_dir = resolve_app_dir(args.app)
    sdk_dir = resolve_sdk_dir()
    esp32_dir = resolve_esp32_dir()
    scripts_dir = resolve_scripts_dir()
    codegen_dir = Path(__file__).resolve().parent / "codegen"
    
    # Set environment variables to make them available to ESP-IDF CMake script-mode
    os.environ["WINK_APP_DIR"] = app_dir.as_posix()
    os.environ["WINK_SDK_PATH"] = sdk_dir.as_posix()
    
    # 1. Run PowerShell generate_app_sources.ps1 with the App Dir
    gen_script = esp32_dir / "generate_app_sources.ps1"
    run_cmd([
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", str(gen_script),
        "-AppDir", str(app_dir)
    ])
    
    # 2. Forward other idf.py commands to build_esp32.ps1
    build_script = scripts_dir / "build_esp32.ps1"
    idf_args = args.idf_args if args.idf_args else ["build"]
    
    # Inject CMake variables to ensure CMake configure resolves directories dynamically
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
    frontend_dir = resolve_frontend_dir()
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
    run_cmd([
        sys.executable,
        "-m", "tools.codegen.tests.test_golden"
    ])
    
    print("\n[wink] Configuring and compiling Host unit tests...")
    build_dir = REPO_ROOT / "build-host-test"
    micro_os_dir = resolve_sdk_dir()
    
    # Clean the build directory first to prevent source tree mismatch errors
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
    
    # Compile the wasm smoke target explicitly if Emscripten is detected on PATH
    # to satisfy the ctest dependency and avoid test failures
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
    
    # 'gen' command
    p_gen = sub.add_parser("gen", help="Run device tree & config macro codegen")
    p_gen.add_argument("--app", default="oled_dashboard",
                       help="App name in samples/ or path to app directory")
    p_gen.set_defaults(handler=handle_gen)
    
    # 'build' command
    p_build = sub.add_parser("build", help="Build Host or WASM simulators")
    p_build.add_argument("target", choices=["host", "wasm"],
                         help="Build target platform")
    p_build.add_argument("--app", default="oled_dashboard",
                         help="App name in samples/ or path to app directory")
    p_build.add_argument("--clean", action="store_true",
                         help="Clean the build directory before building (host only)")
    p_build.set_defaults(handler=handle_build)
    
    # 'esp32' command
    p_esp = sub.add_parser("esp32", help="Build, flash, or monitor ESP32 firmware")
    p_esp.add_argument("--app", default="devkitc_smoke",
                       help="App name in samples/ or path to app directory")
    p_esp.add_argument("idf_args", nargs="*", default=["build"],
                       help="Arguments forwarded to idf.py (e.g. build, flash, monitor)")
    p_esp.set_defaults(handler=handle_esp32)
    
    # 'web' command
    p_web = sub.add_parser("web", help="Start Vue Vite frontend web server")
    p_web.add_argument("--port", type=int, default=5173,
                       help="Vite server port (default: 5173)")
    p_web.set_defaults(handler=handle_web)
    
    # 'test' command
    p_test = sub.add_parser("test", help="Run Python and C unit tests")
    p_test.set_defaults(handler=handle_test)
    
    args = p.parse_args()
    args.handler(args)


if __name__ == "__main__":
    main()
