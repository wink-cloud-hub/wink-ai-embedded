#!/usr/bin/env python3
"""Compatibility shim: wink-micro-os/tools/ moved to wink-tools/.

Prefer: python wink-tools/wink.py …
"""
from __future__ import annotations

import runpy
import sys
from pathlib import Path

_NEW = Path(__file__).resolve().parents[2] / "wink-tools" / "wink.py"
if not _NEW.is_file():
    sys.stderr.write(
        f"[wink] shim error: expected {_NEW}\n"
        "  → wink-tools/ must sit next to wink-micro-os/\n"
    )
    sys.exit(2)
sys.stderr.write(
    "[wink] deprecation: wink-micro-os/tools/wink.py is a shim; "
    "use python wink-tools/wink.py instead\n"
)
runpy.run_path(str(_NEW), run_name="__main__")
