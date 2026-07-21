#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CLI shim for ADR-0035 Arduino isolation.

Rule SSOT lives in ``tools/lint/packs/legacy_arduino.py`` (also used by
``wink lint --pack arduino``). Prefer the wink CLI; this script remains for
standalone / historical callers.
"""
from __future__ import annotations

import sys
from pathlib import Path

_SDK = Path(__file__).resolve().parents[2]  # wink-micro-os
if str(_SDK) not in sys.path:
    sys.path.insert(0, str(_SDK))

from tools.lint.packs.legacy_arduino import check_arduino_isolation  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    del argv  # unused; kept for call-site compatibility
    root = _SDK
    print(f"Scanning kernel core targets under {root} for Arduino leaks...")

    findings = check_arduino_isolation(root)
    if not findings:
        print(
            "[PASS] Core kernel is strictly isolated from Arduino C++ references."
        )
        return 0

    by_path: dict[str, list] = {}
    for f in findings:
        by_path.setdefault(f.path, []).append(f)

    for rel, items in sorted(by_path.items()):
        print(f"\n[FAIL] Isolation violation in {rel}:")
        for f in items:
            print(f"  Line {f.line}: {f.snippet}  <-- {f.message}")

    print(
        f"\n[ERROR] Found {len(findings)} isolation violations! "
        "ADR-0035 forbids C++ Arduino dependencies in the core kernel."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
