# SPDX-License-Identifier: Apache-2.0
"""
run_loop.py - Convenience Launcher for Autonomous ESP-IDF Governance Loop
========================================================================
Usage:
    python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/tools/run_loop.py --app <name>
    python -X utf8 -B wink-micro-app/vendor/esp_idfv61/.governance/tools/run_loop.py --lane 2 --limit 3
"""
import sys
from pathlib import Path

# Add loop micro-package to sys.path
SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from loop.runner import main

if __name__ == "__main__":
    main()
