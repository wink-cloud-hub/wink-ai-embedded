# SPDX-License-Identifier: Apache-2.0
"""
run_loop.py - Convenience Launcher for Autonomous ESP-IDF Governance Loop (Compatibility Shim)
================================================================================================
Canonical entrypoint is cli/run_loop.py. This script provides backward compatibility.
"""
from __future__ import annotations

import sys
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
_GOV_DIR = _SCRIPT_DIR.parent
if str(_GOV_DIR) not in sys.path:
    sys.path.insert(0, str(_GOV_DIR))

from loop.runner import LoopRunner, main

if __name__ == "__main__":
    main()
