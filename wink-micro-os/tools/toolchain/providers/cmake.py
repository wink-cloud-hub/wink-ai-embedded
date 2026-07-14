"""CMake capability provider (cap id: ``cmake``).

Detection strategy
------------------
1. Explicit candidates from :func:`candidate_paths` (workspace/user config).
2. Fall back to ``shutil.which("cmake")`` on PATH.
3. Probe with ``cmake --version``; require >= 3.15.

CMake is not auto-installable by Wink; ``install()`` raises
:class:`UnsupportedError` (inherited from the base). ``hint()`` returns
a platform-appropriate install command via :mod:`tools.toolchain.platform`.
"""
from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

from ..platform import get_hints
from ..resolve import ResolveContext, candidate_paths
from ..types import DetectResult, PROBE_TIMEOUT_SEC
from ._version import floor_str, parse_version, version_ge
from .base import Provider

_CMAKE_FLOOR: tuple[int, ...] = (3, 15)


def _probe(exe: Path) -> str | None:
    """Return the raw stdout of ``exe --version`` or None on failure."""
    try:
        cp = subprocess.run(
            [str(exe), "--version"],
            capture_output=True,
            text=True,
            timeout=PROBE_TIMEOUT_SEC,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if cp.returncode != 0:
        return None
    return (cp.stdout or "") + (cp.stderr or "")


class CMakeProvider(Provider):
    id = "cmake"

    def detect(self, ctx: ResolveContext) -> DetectResult:
        # 1. Explicit candidates (env is not populated for cmake, but config is).
        for source, cand in candidate_paths(self.id, ctx):
            if not cand.exists():
                continue
            out = _probe(cand)
            if out is None:
                continue
            v = parse_version(out)
            if v is None:
                continue
            v_str = ".".join(str(x) for x in v)
            if not version_ge(v_str, _CMAKE_FLOOR):
                return DetectResult(
                    found=False,
                    path=cand,
                    version=v_str,
                    reason=(
                        f"cmake {v_str} is below required floor "
                        f"{floor_str(_CMAKE_FLOOR)}"
                    ),
                    source=source,
                )
            return DetectResult(
                found=True,
                path=cand,
                version=v_str,
                reason=None,
                source=source,
            )

        # 2. PATH fallback.
        which = shutil.which("cmake")
        if which is None:
            return DetectResult(
                found=False,
                path=None,
                version=None,
                reason="cmake not found on PATH",
                source=None,
            )
        cand = Path(which)
        out = _probe(cand)
        if out is None:
            return DetectResult(
                found=False,
                path=cand,
                version=None,
                reason="cmake --version failed",
                source="path",
            )
        v = parse_version(out)
        if v is None:
            return DetectResult(
                found=False,
                path=cand,
                version=None,
                reason="could not parse cmake version",
                source="path",
            )
        v_str = ".".join(str(x) for x in v)
        if not version_ge(v_str, _CMAKE_FLOOR):
            return DetectResult(
                found=False,
                path=cand,
                version=v_str,
                reason=(
                    f"cmake {v_str} is below required floor "
                    f"{floor_str(_CMAKE_FLOOR)}"
                ),
                source="path",
            )
        return DetectResult(
            found=True,
            path=cand,
            version=v_str,
            reason=None,
            source="path",
        )

    def hint(self, ctx: ResolveContext) -> str:
        return get_hints(ctx.os_name).install_hint(self.id)
