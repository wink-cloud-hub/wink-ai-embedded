#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify esp_restart cannot fall through outside a scheduler fiber."""

from __future__ import annotations

import subprocess
import sys


def main() -> int:
    result = subprocess.run([sys.argv[1]], text=True, capture_output=True, timeout=10)
    if result.returncode == 0:
        print("esp_restart returned outside a scheduler fiber", file=sys.stderr)
        return 1
    print(f"esp_restart terminated the out-of-fiber process (exit={result.returncode})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
