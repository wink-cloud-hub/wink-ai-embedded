#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
cli.triage_soc - SoC Target Compatibility & Triage CLI
=====================================================
Standard CLI entry point to analyze target SoC support across examples.
"""
from __future__ import annotations

import sys
from pathlib import Path

GOV_DIR = Path(__file__).resolve().parent.parent
TOOLS_DIR = GOV_DIR / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from triage_soc_support import main

if __name__ == "__main__":
    main()
