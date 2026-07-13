"""Emsdk (Emscripten SDK) capability provider (cap id: ``emsdk``).

Phase A stance
--------------
The provider **only detects a pre-activated emsdk shell**. It never sources
``emsdk_env.bat`` on the user's behalf, since doing so requires touching the
parent shell environment (which we cannot do from Python). Detection PASSes
only when:

1. ``EMSDK`` env var is set (indicating the user ran ``emsdk activate``), and
2. ``emcc --version`` succeeds in the current environment, and
3. ``emcmake --version`` succeeds.

The version floor is ``>= 3.1.45``. Note that the emcc bundled with recent
ESP-IDF releases can report ``6.0.1`` (per the emsdk memory note); that
still satisfies the floor by tuple comparison and is accepted verbatim.

``install()`` inherits the base ``UnsupportedError`` — Wink does not install
emsdk automatically in phase A (see ADR-0030 and ``preinstall.md`` §2).
"""
from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

from ..resolve import ResolveContext
from ..types import DetectResult, PROBE_TIMEOUT_SEC
from ._version import floor_str, parse_version, version_ge
from .base import Provider

_EMSDK_FLOOR: tuple[int, ...] = (3, 1, 45)


def _run_version(exe_name: str) -> tuple[int, str] | None:
    """Run ``<exe_name> --version`` and return ``(rc, combined output)``.

    Returns ``None`` if the process could not be started (missing binary,
    broken shim, timeout).
    """
    try:
        cp = subprocess.run(
            [exe_name, "--version"],
            capture_output=True,
            text=True,
            timeout=PROBE_TIMEOUT_SEC,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    return cp.returncode, (cp.stdout or "") + (cp.stderr or "")


class EmsdkProvider(Provider):
    id = "emsdk"

    def detect(self, ctx: ResolveContext) -> DetectResult:
        emsdk_root = ctx.environ.get("EMSDK", "").strip()
        if not emsdk_root:
            return DetectResult(
                found=False,
                path=None,
                version=None,
                reason=(
                    "emsdk not activated — set EMSDK and run the activation "
                    "script (see preinstall.md §2)"
                ),
                source=None,
            )

        # Verify both emcc and emcmake are reachable and runnable.
        emcc_which = shutil.which("emcc") or shutil.which("emcc.bat")
        emcmake_which = shutil.which("emcmake") or shutil.which("emcmake.bat")
        if emcc_which is None or emcmake_which is None:
            return DetectResult(
                found=False,
                path=Path(emsdk_root),
                version=None,
                reason=(
                    "EMSDK is set but emcc is not on PATH — run the emsdk "
                    "activation script for your shell (see preinstall.md §2)"
                ),
                source="env:EMSDK",
            )

        emcc_result = _run_version("emcc")
        if emcc_result is None or emcc_result[0] != 0:
            return DetectResult(
                found=False,
                path=Path(emsdk_root),
                version=None,
                reason=(
                    "EMSDK is set but emcc is not on PATH — run the emsdk "
                    "activation script for your shell (see preinstall.md §2)"
                ),
                source="env:EMSDK",
            )
        emcmake_result = _run_version("emcmake")
        if emcmake_result is None or emcmake_result[0] != 0:
            return DetectResult(
                found=False,
                path=Path(emsdk_root),
                version=None,
                reason=(
                    "EMSDK is set but emcmake is not on PATH — run the emsdk "
                    "activation script for your shell (see preinstall.md §2)"
                ),
                source="env:EMSDK",
            )

        v = parse_version(emcc_result[1])
        if v is None:
            return DetectResult(
                found=False,
                path=Path(emsdk_root),
                version=None,
                reason="could not parse emcc --version output",
                source="env:EMSDK",
            )
        v_str = ".".join(str(x) for x in v)
        if not version_ge(v_str, _EMSDK_FLOOR):
            return DetectResult(
                found=False,
                path=Path(emsdk_root),
                version=v_str,
                reason=(
                    f"emcc {v_str} is below required floor "
                    f"{floor_str(_EMSDK_FLOOR)}"
                ),
                source="env:EMSDK",
            )

        return DetectResult(
            found=True,
            path=Path(emsdk_root),
            version=v_str,
            reason=None,
            source="env:EMSDK",
        )

    def hint(self, ctx: ResolveContext) -> str:
        return (
            "Activate emsdk in your shell "
            "(`emsdk activate latest && emsdk_env.bat` on Windows) or install "
            "from https://emscripten.org/; see preinstall.md §2."
        )
