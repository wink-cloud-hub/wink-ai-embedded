#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
run_lint.py - Dual-mode adapter for wink-tools linting.
Priority: 1. Local source (WINK_TOOLS_ROOT / sibling) -> 2. Global `winkcli` / `wink` CLI
"""
import os
import sys
import shutil
import subprocess
from pathlib import Path

def main():
    args = sys.argv[1:]
    repo_root = Path(__file__).resolve().parent.parent

    # 1. Probe local source directories
    candidate_roots = [
        os.environ.get("WINK_TOOLS_ROOT"),
        str(repo_root.parent / "wink-tools"),
        str(repo_root.parent / "packages" / "wink-tools"),
        str(repo_root.parent.parent / "wink-ai" / "packages" / "wink-tools"),
    ]
    
    source_wink_py = None
    for cand in candidate_roots:
        if cand and (Path(cand) / "wink.py").exists():
            source_wink_py = Path(cand) / "wink.py"
            break

    # Mode A: Source mode (Development)
    if source_wink_py:
        cmd = [sys.executable, str(source_wink_py), "lint"] + args
        return subprocess.run(cmd).returncode

    # Mode B: Global CLI mode (CI / User mode)
    global_cli = shutil.which("winkcli") or shutil.which("wink")
    if global_cli:
        cmd = [global_cli, "lint"] + args
        return subprocess.run(cmd).returncode

    print("Error: wink-tools not found!", file=sys.stderr)
    print("  - For CI/Users: Run 'pip install winkcli' to install the global toolchain.", file=sys.stderr)
    print("  - For Developers: Set WINK_TOOLS_ROOT to the wink-tools source directory.", file=sys.stderr)
    return 1

if __name__ == "__main__":
    sys.exit(main())
