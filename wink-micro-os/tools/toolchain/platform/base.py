"""Base platform-hint class and cross-platform fallback text.

Providers call :func:`get_hints(os_name)` (from
:mod:`tools.toolchain.platform`) to obtain a :class:`PlatformHints` instance
whose ``install_hint(cap_id)`` returns a one-line, actionable install string
for that capability.
"""
from __future__ import annotations


class PlatformHints:
    """Default (POSIX-ish) install hints; Windows subclass overrides.

    Instances are stateless; a single instance per platform is created and
    reused. Unknown cap ids fall through to a generic ``package manager`` hint.
    """

    def install_hint(self, cap_id: str) -> str:
        # Generic fallback used for POSIX hosts in phase A.
        table = {
            "python": "install Python 3.10+ via your package manager (apt, brew, dnf, ...)",
            "jinja2": "pip install jinja2 (or `python -m pip install jinja2`)",
            "cmake": "install CMake 3.15+ via your package manager (apt install cmake, brew install cmake, ...)",
            "make": "install GNU Make via your package manager (build-essential on Debian/Ubuntu, Xcode CLT on macOS)",
            "gcc": "install gcc 14+ via your package manager (build-essential, brew install gcc@14, ...)",
        }
        return table.get(
            cap_id,
            f"install '{cap_id}' via your platform package manager",
        )
