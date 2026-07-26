#!/usr/bin/env python3
"""Backwards-compatibility shim for tools.pack.binary."""
from __future__ import annotations

import sys
from pathlib import Path

_HERE = Path(__file__).resolve().parent
if str(_HERE.parent) not in sys.path:
    sys.path.insert(0, str(_HERE.parent))

from tools.pack.binary import main

if __name__ == "__main__":
    raise SystemExit(main())
