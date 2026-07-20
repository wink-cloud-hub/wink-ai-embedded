"""Replay bal/CMakeLists.txt layering REGEX gates in pure Python.

Used by Task 7.5 parity probes so wink lint can be proven equivalent to the
configure-time CMake checks before those checks are deleted (Task 8).

DAL-HDR-NO-HAL is not a CMake gate; it is included here as the same probe
criterion wink lint uses, so the bad/good fixture sets stay comparable.
"""
from __future__ import annotations

import re
from collections import defaultdict
from pathlib import Path

# Constants extracted from bal/CMakeLists.txt (ADR-0023 / ADR-0038).
_BAL_FORBIDDEN_INCLUDE_REGEX = re.compile(
    r'^\s*#\s*include\s*[<"]pal_.*\.h[>"]', re.MULTILINE
)
_BAL_MATH_FORBIDDEN_REGEXES = [
    re.compile(r"dal_"),
    re.compile(r"wink_runtime"),
    re.compile(r"wink_periodic"),
    re.compile(r"pal_"),
    re.compile(r'^\s*#\s*include\s*[<"]pal_', re.MULTILINE),
]
_SONAR_WORD = re.compile(r"\bsonar\b")
_PAL_HAL_INCLUDE = re.compile(
    r'^\s*#\s*include\s*[<"][^>"]*pal_hal\.h[>"]', re.MULTILINE
)


def run_cmake_replay(root: Path) -> dict[str, set[str]]:
    """Return {rel_posix_path: {rule_id, ...}} for violations under ``root``."""
    root = root.resolve()
    fails: dict[str, set[str]] = defaultdict(set)

    bal_include = root / "bal" / "include"
    if bal_include.is_dir():
        public_headers = sorted(bal_include.rglob("*.h"))
        for hdr in public_headers:
            rel = hdr.relative_to(root).as_posix()
            text = hdr.read_text(encoding="utf-8", errors="replace")

            for m in _BAL_FORBIDDEN_INCLUDE_REGEX.finditer(text):
                line = m.group(0)
                if "pal_log.h" in line:
                    continue
                fails[rel].add("BAL-HDR-NO-PAL")
                break

            name = hdr.name
            if name.endswith("_helper.h") or name.endswith("_controller.h"):
                fails[rel].add("BAL-NAME-1")

            if _SONAR_WORD.search(text):
                fails[rel].add("BAL-NAME-2")

        math_dir = bal_include / "math"
        if math_dir.is_dir():
            for hdr in sorted(math_dir.glob("*.h")):
                rel = hdr.relative_to(root).as_posix()
                text = hdr.read_text(encoding="utf-8", errors="replace")
                for cre in _BAL_MATH_FORBIDDEN_REGEXES:
                    if cre.search(text):
                        fails[rel].add("BAL-MATH-1")
                        break

    dal_include = root / "dal" / "include"
    if dal_include.is_dir():
        for hdr in sorted(dal_include.rglob("*.h")):
            rel = hdr.relative_to(root).as_posix()
            text = hdr.read_text(encoding="utf-8", errors="replace")
            if _PAL_HAL_INCLUDE.search(text):
                fails[rel].add("DAL-HDR-NO-HAL")

    return {k: v for k, v in fails.items()}
