"""Windows PowerShell 5.1 capability provider (cap id: ``powershell``).

Phase B stance (post Python migration)
--------------------------------------
The ESP32 build pipeline (``tools/esp32/{activate,build,generate_app_sources}.py``)
no longer declares ``powershell`` as a required capability. It is used
**only** on Windows by ``activate.py`` as an internal fallback to harvest
env vars from an EIM PowerShell profile when the current shell is not
already activated. On non-Windows hosts, the ``idf`` provider itself
returns a clear "Windows-only in phase A" detection failure.

PowerShell 5.1 is the inbox edition shipped at
``C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe``
(distinct from cross-platform ``pwsh``). Detection is a fixed-path
existence check — no ``--version`` probe is needed since Windows always
ships PowerShell 5.1 at this exact location.

``install()`` inherits the base ``UnsupportedError``.
"""
from __future__ import annotations

from pathlib import Path

from ..resolve import ResolveContext
from ..types import DetectResult
from .base import Provider

_WIN_POWERSHELL_PATH = Path(
    r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe"
)


class PowerShellProvider(Provider):
    id = "powershell"

    def detect(self, ctx: ResolveContext) -> DetectResult:
        if ctx.os_name != "nt":
            return DetectResult(
                found=False,
                path=None,
                version=None,
                reason=(
                    "esp32 builds require Windows PowerShell 5.1 in phase A"
                ),
                source=None,
            )
        if _WIN_POWERSHELL_PATH.exists():
            return DetectResult(
                found=True,
                path=_WIN_POWERSHELL_PATH,
                version=None,
                reason=None,
                source="system32",
            )
        return DetectResult(
            found=False,
            path=None,
            version=None,
            reason=f"{_WIN_POWERSHELL_PATH} not found",
            source=None,
        )

    def hint(self, ctx: ResolveContext) -> str:
        if ctx.os_name == "nt":
            return (
                "Windows PowerShell 5.1 ships with Windows at "
                f"{_WIN_POWERSHELL_PATH}; reinstall Windows features if it "
                "is missing."
            )
        return (
            "esp32 builds require Windows in phase A — run wink on a "
            "Windows host with PowerShell 5.1 installed."
        )
