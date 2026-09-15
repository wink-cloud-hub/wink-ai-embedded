#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify zh/en pairing and i18n metadata for the public wizard docs (wink-tools/docs).

Checks:
1. `wink-tools/docs/zh/**` and `wink-tools/docs/en/**` contain exactly the same
   relative file paths (pairing).
2. Every `en` document carries an `i18n-meta` block with `source:`, `translated:`
   and `sync-status:` keys; `source:` must point at the matching zh document.

Exit code 0 when all checks pass, 1 otherwise.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DOCS_ROOT = REPO_ROOT / "wink-tools" / "docs"

_I18N_META = re.compile(r"i18n-meta(.*?)-->", re.DOTALL)
_REQUIRED_KEYS = ("source:", "translated:", "sync-status:")


def _relative_markdown(root: Path) -> set[str]:
    if not root.is_dir():
        return set()
    return {p.relative_to(root).as_posix() for p in root.rglob("*.md")}


def main() -> int:
    zh_root = DOCS_ROOT / "zh"
    en_root = DOCS_ROOT / "en"

    findings: list[str] = []

    zh = _relative_markdown(zh_root)
    en = _relative_markdown(en_root)

    for missing in sorted(zh - en):
        findings.append(f"wink-tools/docs/en is missing: {missing}")
    for extra in sorted(en - zh):
        findings.append(f"wink-tools/docs/zh is missing: {extra}")

    for rel in sorted(en & zh):
        text = (en_root / rel).read_text(encoding="utf-8")
        match = _I18N_META.search(text)
        if not match:
            findings.append(f"{rel}: missing i18n-meta block")
            continue
        block = match.group(1)
        for key in _REQUIRED_KEYS:
            if key not in block:
                findings.append(f"{rel}: i18n-meta missing '{key.rstrip(':')}'")
        source_match = re.search(r"source:\s*(\S+)", block)
        if source_match:
            expected = f"docs/zh/{rel}"
            if not source_match.group(1).endswith(expected):
                findings.append(
                    f"{rel}: i18n-meta source points to '{source_match.group(1)}', expected '...{expected}'"
                )

    if findings:
        print(f"[FAIL] wink-tools/docs i18n checks: {len(findings)} finding(s)")
        for item in findings:
            print(f"  - {item}")
        return 1

    print(f"[PASS] wink-tools/docs i18n checks ({len(en)} paired documents)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
