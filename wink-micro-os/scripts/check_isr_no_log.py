#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
[DEPRECATED] check_isr_no_log.py - Migrated to `wink lint --pack isr_safety` (ADR-0047, ADR-0051).
"""
import sys
import subprocess
from pathlib import Path

def main():
    print("[DEPRECATED] check_isr_no_log.py is deprecated. Delegating to `run_lint.py --pack isr_safety`...", file=sys.stderr)
    repo_root = Path(__file__).resolve().parent.parent
    run_lint = repo_root / "tools" / "run_lint.py"
    if run_lint.exists():
        return subprocess.run([sys.executable, str(run_lint), "--pack", "isr_safety", "--rule", "ISR-NO-FLASH-CALL"]).returncode
    return 0

if __name__ == "__main__":
    sys.exit(main())
