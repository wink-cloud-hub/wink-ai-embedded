#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
triage_soc_support.py — 81 项未列 ESP32 支持表冲突分流与清单缺口修复工具 (兼容代理)
=================================================================================
Thin backward-compatibility shim for triage_soc_support.py.
Canonical implementation has moved to loop.services.soc_triage and cli.triage_soc.
"""
from __future__ import annotations

import sys
from pathlib import Path

_GOV_DIR = Path(__file__).resolve().parent.parent
if str(_GOV_DIR) not in sys.path:
    sys.path.insert(0, str(_GOV_DIR))

from cli.triage_soc import main
from loop.services.soc_triage import (
    DATA_PATH,
    HARDWARE_EXCLUSIVE_CAPS,
    HARDWARE_EXCLUSIVE_PATH_PATTERNS,
    apply_gap_remediation_and_triage,
    check_upstream_soc_support,
    extract_headers_and_configs,
)

__all__ = [
    "DATA_PATH",
    "HARDWARE_EXCLUSIVE_CAPS",
    "HARDWARE_EXCLUSIVE_PATH_PATTERNS",
    "extract_headers_and_configs",
    "check_upstream_soc_support",
    "apply_gap_remediation_and_triage",
    "main",
]

if __name__ == "__main__":
    sys.exit(main())
