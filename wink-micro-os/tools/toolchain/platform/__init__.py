"""Platform-specific install hint text used by provider ``hint()`` methods.

Providers stay platform-agnostic; the actionable install advice
(``winget install ...``, WinLibs download link, ...) lives here so it can be
adjusted without touching the detection logic.
"""
from __future__ import annotations

from .base import PlatformHints
from .win import WindowsHints

_WINDOWS = WindowsHints()
_POSIX = PlatformHints()


def get_hints(os_name: str) -> PlatformHints:
    """Return the :class:`PlatformHints` instance for ``os_name``.

    ``os_name`` matches :data:`os.name` values: ``"nt"`` for Windows,
    ``"posix"`` for everything else. Unknown values fall back to the POSIX
    hints so we never crash for an unrecognized platform.
    """
    if os_name == "nt":
        return _WINDOWS
    return _POSIX


__all__ = ["PlatformHints", "WindowsHints", "get_hints"]
