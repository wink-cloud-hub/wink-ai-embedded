"""Windows-specific install hint strings.

Prefers ``winget`` where available and points at WinLibs for the MinGW
toolchain (matching the host_c_toolchain memory note).
"""
from __future__ import annotations

from .base import PlatformHints


class WindowsHints(PlatformHints):
    def install_hint(self, cap_id: str) -> str:
        table = {
            "python": (
                "install Python 3.10+ from python.org, or "
                "`winget install Python.Python.3.12`"
            ),
            "jinja2": "pip install jinja2 (or `python -m pip install jinja2`)",
            "cmake": (
                "`winget install Kitware.CMake` "
                "(or download from https://cmake.org/download/)"
            ),
            "make": (
                "install WinLibs MinGW (includes mingw32-make.exe); "
                "see https://winlibs.com/"
            ),
            "gcc": (
                "install WinLibs MinGW UCRT POSIX >= 14 from "
                "https://winlibs.com/ and add its bin/ to PATH "
                "(or set WINK_GCC_PREFIX)"
            ),
        }
        return table.get(
            cap_id,
            f"install '{cap_id}' (see project docs)",
        )
