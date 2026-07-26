"""tools.cli.commands.test — TestCommand for unit tests and sanitizer pass matrix."""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Optional

from tools.cli._shared import run_adr0017_l1_strict_lint, run_cmd, run_esp32_guard_density_lint
from tools.cli.base import CommandBase
from tools.cli.context import AppContext


class TestCommand(CommandBase):
    name = "test"
    help = "Run Python, C unit tests, sanitizer pass matrix, and lints"

    def register_args(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--clean", action="store_true", help="Clean test build directories before running tests")
        parser.add_argument("--detailed", action="store_true", help="Print verbose ctest output (-V)")
        parser.add_argument("--sanitize", action="store_true", help="Enable UBSan sanitize matrix pass")
        parser.add_argument("--asan", action="store_true", help="Enable ASan matrix pass")
        parser.add_argument("--full", action="store_true", help="Run full test matrix")
        parser.add_argument("--with-wasm", action="store_true", help="Run optional WASM compilation check")

    def run(self, ctx: AppContext, args: argparse.Namespace) -> Optional[int]:
        is_full = getattr(args, "full", False)
        do_sanitize = is_full or getattr(args, "sanitize", False)
        do_asan = getattr(args, "asan", False) or (is_full and os.environ.get("WINK_ENABLE_ASAN_TESTS") == "1")
        do_wasm = is_full or getattr(args, "with_wasm", False)
        do_clean = getattr(args, "clean", False)
        detailed = getattr(args, "detailed", False)

        sdk_dir = ctx.sdk_root

        # 0. Protothread check
        pt_check_script = sdk_dir / "tools" / "lint" / "check_pt_variables.py"
        if pt_check_script.is_file():
            print("[wink] Scanning for protothread auto variable footguns...")
            try:
                subprocess.run([sys.executable, str(pt_check_script)], check=True)
            except subprocess.CalledProcessError as e:
                print(f"[wink] Error: Protothread variable check failed with code {e.returncode}", file=sys.stderr)
                return e.returncode

        # 1. Codegen Golden regression tests
        print("\n[wink] Running Codegen Golden + validation regression tests...")
        ws_root = ctx.workspace_root
        env = os.environ.copy()
        prev = env.get("PYTHONPATH", "")
        env["PYTHONPATH"] = str(sdk_dir) + (os.pathsep + prev if prev else "")
        tests_dir = sdk_dir / "tools" / "codegen" / "tests"
        try:
            subprocess.run(
                [sys.executable, "-m", "unittest", "discover", "-s", str(tests_dir), "-p", "test_*.py"],
                cwd=str(ws_root),
                check=True,
                env=env,
            )
        except subprocess.CalledProcessError as e:
            print(f"[wink] Error: Codegen tests failed with exit code {e.returncode}", file=sys.stderr)
            return e.returncode

        # 2. Host C Test Pass Matrix Execution
        codegen_dir = sdk_dir / "tools" / "codegen"

        passes = [
            {"label": "default", "dir": ws_root / "build" / "test", "flags": "", "enabled": True},
            {
                "label": "sanitize",
                "dir": ws_root / "build" / "test-san",
                "flags": "-fsanitize=undefined -fsanitize-undefined-trap-on-error -Wcast-function-type -Werror=cast-function-type",
                "enabled": do_sanitize,
            },
            {
                "label": "asan",
                "dir": ws_root / "build" / "test-asan",
                "flags": "-fsanitize=address -fno-omit-frame-pointer",
                "enabled": do_asan,
            },
        ]

        overall_rc = 0

        for p in passes:
            if not p["enabled"]:
                continue
            label = p["label"]
            bdir = p["dir"]
            cflags = p["flags"]

            print(f"\n===== [{label}] cmake configure ({bdir}) =====")
            if do_clean and bdir.exists():
                print(f"-> Cleaning {bdir}...")
                shutil.rmtree(bdir)

            configure_cmd = [
                "cmake",
                "-S", str(sdk_dir),
                "-B", str(bdir),
                "-DTARGET_PLATFORM=host",
                f"-DWINK_CODEGEN_ROOT={codegen_dir.as_posix()}",
            ]
            if cflags:
                configure_cmd.append(f"-DCMAKE_C_FLAGS={cflags}")
            if sys.platform == "win32":
                configure_cmd.extend(["-G", "MinGW Makefiles"])

            try:
                run_cmd(configure_cmd)
            except Exception:
                print(f"[FAIL] [{label}] configure failed", file=sys.stderr)
                overall_rc = 1
                continue

            print(f"-> [{label}] Building...")
            try:
                run_cmd(["cmake", "--build", str(bdir)])
                if shutil.which("emcc") and shutil.which("emcmake"):
                    print(f"-> [{label}] Building WASM smoke target...")
                    run_cmd(["cmake", "--build", str(bdir), "--target", "wasm_unisim_smoke_build"])
            except Exception:
                print(f"[FAIL] [{label}] build failed", file=sys.stderr)
                overall_rc = 1
                continue

            print(f"-> [{label}] Running CTest...")
            ctest_cmd = ["ctest", "--test-dir", str(bdir), "--output-on-failure"]
            if detailed:
                ctest_cmd.append("-V")
            try:
                run_cmd(ctest_cmd)
                print(f"[PASS] [{label}] all tests passed")
            except Exception:
                print(f"[FAIL] [{label}] some tests failed", file=sys.stderr)
                overall_rc = 1

        # 3. Optional WASM build check
        if do_wasm:
            print("\n===== [wasm build check] =====")
            emcc_path = shutil.which("emcc")
            emcmake_path = shutil.which("emcmake")
            if not emcc_path or not emcmake_path:
                print("[FAIL] emcc or emcmake not found on PATH for WASM build check", file=sys.stderr)
                overall_rc = 1
            else:
                avoidance_app = (sdk_dir.parent / "wink-micro-app" / "avoidance_car").resolve()
                if not avoidance_app.is_dir():
                    avoidance_app = (sdk_dir / "samples" / "avoidance_car").resolve()
                wasm_bdir = ws_root / "build" / "wasm" / "avoidance_car"
                if do_clean and wasm_bdir.exists():
                    shutil.rmtree(wasm_bdir)
                try:
                    run_cmd([
                        emcmake_path, "cmake",
                        "-B", str(wasm_bdir),
                        "-DTARGET_PLATFORM=wasm",
                        f"-DWINK_APP_DIR={avoidance_app.as_posix()}",
                    ])
                    run_cmd(["cmake", "--build", str(wasm_bdir)])
                    print("[PASS] WASM build check succeeded")
                except Exception:
                    print("[FAIL] WASM build check failed", file=sys.stderr)
                    overall_rc = 1

        # 4. Static Lints execution
        if not run_esp32_guard_density_lint(sdk_dir):
            overall_rc = 1

        if not run_adr0017_l1_strict_lint(sdk_dir):
            overall_rc = 1

        header_self_check = sdk_dir / "tools" / "lint" / "check_headers_self_contained.py"
        if header_self_check.is_file():
            print("\n[lint] Header self-containment (P1-B2)...")
            try:
                subprocess.run([sys.executable, str(header_self_check)], check=True)
            except subprocess.CalledProcessError as e:
                print(f"[wink] Error: Header self-containment check failed: {e}", file=sys.stderr)
                overall_rc = 1

        try:
            from tools.lint.cli import handle_lint

            class LintArgs:
                root = str(sdk_dir)
                config = []
                pack = ["layering", "api", "arduino"]
                rule = None
                paths = None
                changed = None
                format = "text"
                output = None
                strict = False
                explain = None
                report_allowlist = False
                baseline = None
                today = None

            print("\n[lint] Architecture lints (layering + api + arduino)...")
            handle_lint(LintArgs())
        except ModuleNotFoundError as exc:
            if exc.name == "yaml":
                print("[wink lint] PyYAML not installed — skipping YAML lints pass.", file=sys.stderr)
            else:
                raise
        except SystemExit as e:
            if e.code != 0:
                overall_rc = e.code

        if overall_rc != 0:
            print("\n[wink] Test suite finished with FAILURES.", file=sys.stderr)
            return overall_rc

        print("\n[wink] Test suite finished SUCCESS.")
        return 0
