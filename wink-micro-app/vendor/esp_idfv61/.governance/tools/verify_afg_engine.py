#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
verify_afg_engine.py - Unified Pilot & Legacy Remediation Verification Tool (Compatibility Shim)
================================================================================================
Thin backward-compatibility shim for verify_afg_engine.py.
Canonical implementation has moved to loop.services.afg_verification and cli.verify_afg.
"""
from __future__ import annotations

import sys
from pathlib import Path

_GOV_DIR = Path(__file__).resolve().parent.parent
if str(_GOV_DIR) not in sys.path:
    sys.path.insert(0, str(_GOV_DIR))

from loop.services.afg_verification import (
    AFGVerificationService,
    PilotVerifier,
    find_workspace_root,
)
from cli.verify_afg import main

__all__ = [
    "PilotVerifier",
    "AFGVerificationService",
    "find_workspace_root",
    "main",
]

if __name__ == "__main__":
    sys.exit(main())
