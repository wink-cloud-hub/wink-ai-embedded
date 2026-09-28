#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Give an NVS CTest a clean private sandbox and retain it on failure."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def remove_sandbox(path: Path) -> None:
    if path.is_dir() and not path.is_symlink():
        shutil.rmtree(path)
    elif path.exists() or path.is_symlink():
        path.unlink()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sandbox", required=True)
    parser.add_argument("--build-root", required=True)
    parser.add_argument("--expect-not-directory", action="store_true")
    parser.add_argument("--expect-io-error", action="store_true")
    parser.add_argument("--too-long-path", action="store_true")
    parser.add_argument("program")
    args = parser.parse_args()

    root = Path(args.build_root).resolve()
    sandbox = Path(args.sandbox).resolve()
    if sandbox == root or root not in sandbox.parents:
        print("sandbox must be a child of the configured build root", file=sys.stderr)
        return 2
    remove_sandbox(sandbox)
    if args.expect_not_directory:
        sandbox.parent.mkdir(parents=True, exist_ok=True)
        sandbox.write_text("deliberately not a directory\n", encoding="utf-8")
    else:
        sandbox.mkdir(parents=True, exist_ok=True)
    sandbox_env_path = str(sandbox)
    if args.expect_io_error:
        (sandbox / "nvs_storage.bin.tmp").mkdir()
    if args.too_long_path:
        sandbox_env_path += "x" * 320

    env = os.environ.copy()
    env["WINK_SIM_SANDBOX_DIR"] = sandbox_env_path
    result = subprocess.run([args.program], env=env, text=True, capture_output=True)
    if result.stdout:
        print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    if result.returncode != 0:
        print(f"NVS test failed ({result.returncode}); sandbox retained at {sandbox}", file=sys.stderr)
        return result.returncode or 1
    remove_sandbox(sandbox)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
