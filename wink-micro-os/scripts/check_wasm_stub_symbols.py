#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
[DEPRECATED] check_wasm_stub_symbols.py - Migrated to `wink lint --pack wasm_parity` (ADR-0003, ADR-0051).
"""
import sys
import subprocess
from pathlib import Path

def main():
    print("[DEPRECATED] check_wasm_stub_symbols.py is deprecated. Delegating to `run_lint.py --pack wasm_parity`...", file=sys.stderr)
    repo_root = Path(__file__).resolve().parent.parent
    run_lint = repo_root / "tools" / "run_lint.py"
    if run_lint.exists():
        return subprocess.run([sys.executable, str(run_lint), "--pack", "wasm_parity", "--rule", "WASM-STUB-COVERAGE"]).returncode
    return 0

if __name__ == "__main__":
    sys.exit(main())
