#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
cli.verify_afg - Anti-False-Green Engine Verification & Pilot CLI
================================================================
Standard CLI entry point to verify AFG v1.1 pilot scenarios and legacy remediation.
"""
from __future__ import annotations

import sys
from pathlib import Path

# Add governance root to path if needed
GOV_DIR = Path(__file__).resolve().parent.parent
TOOLS_DIR = GOV_DIR / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from verify_afg_engine import main

if __name__ == "__main__":
    main()
