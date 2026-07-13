"""Windows PowerShell 5.1 capability provider (cap id: ``powershell``).

Phase A stance
--------------
esp32 builds are driven by ``scripts/build_esp32.ps1``, which requires
Windows PowerShell 5.1 (the ``powershell.exe`` shipped at
``C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\``, distinct from
cross-platform ``pwsh``). Non-Windows hosts fail detection: esp32 support
is Windows-only in phase A.

Detection is a fixed-path existence check — no ``--version`` probe is
needed since Windows always ships PowerShell 5.1 at this exact location.

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
