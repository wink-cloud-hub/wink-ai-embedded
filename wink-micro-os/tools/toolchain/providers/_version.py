"""Small helpers for parsing and comparing tool version strings.

Kept intentionally minimal: version strings from `--version` output are
noisy (\"cmake version 3.28.1\", \"gcc (Ubuntu ...) 14.1.0\", ...); we only
need to extract the first ``X.Y[.Z]`` numeric tuple and compare it against
a required floor.
"""
from __future__ import annotations

import re

_VERSION_RE = re.compile(r"(\d+)\.(\d+)(?:\.(\d+))?")


def parse_version(text: str) -> tuple[int, ...] | None:
    """Return the first ``(major, minor[, patch])`` tuple found in ``text``.

    Returns ``None`` if no numeric version is found.
    """
    m = _VERSION_RE.search(text)
    if not m:
        return None
    parts = [m.group(1), m.group(2)]
    if m.group(3) is not None:
        parts.append(m.group(3))
    return tuple(int(p) for p in parts)


def version_ge(version_str: str, floor: tuple[int, ...]) -> bool:
    """Return True if the version parsed from ``version_str`` is >= ``floor``."""
    parsed = parse_version(version_str)
    if parsed is None:
        return False
    # Pad the shorter tuple with zeros so comparison is well-defined.
    n = max(len(parsed), len(floor))
    p = tuple(list(parsed) + [0] * (n - len(parsed)))
    f = tuple(list(floor) + [0] * (n - len(floor)))
    return p >= f


def floor_str(floor: tuple[int, ...]) -> str:
    """Render a floor tuple as a dotted string, e.g. ``(3, 15)`` -> ``\"3.15\"``."""
    return ".".join(str(x) for x in floor)
