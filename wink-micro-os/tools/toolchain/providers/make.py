"""GNU Make capability provider (cap id: ``make``).

Detection strategy
------------------
- Windows: prefer ``mingw32-make.exe`` (the WinLibs MinGW variant); if
  candidate paths from config exist, use them; else ``shutil.which``.
- POSIX: prefer ``make`` on PATH; fall back to ``gmake`` (some BSDs and
  older setups ship a BSD make as ``make`` and GNU make as ``gmake``).
- No version floor in phase A; any GNU Make works.
"""
from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

from ..platform import get_hints
from ..resolve import ResolveContext, candidate_paths
from ..types import DetectResult, PROBE_TIMEOUT_SEC
from ._version import parse_version
from .base import Provider


def _probe(exe: Path) -> str | None:
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


def _finalize(cand: Path, source: str) -> DetectResult:
    out = _probe(cand)
    if out is None:
        return DetectResult(
            found=False,
            path=cand,
            version=None,
            reason="make --version failed",
            source=source,
        )
    v = parse_version(out)
    v_str = ".".join(str(x) for x in v) if v is not None else None
    return DetectResult(
        found=True,
        path=cand,
        version=v_str,
        reason=None,
        source=source,
    )


class MakeProvider(Provider):
    id = "make"

    def detect(self, ctx: ResolveContext) -> DetectResult:
        # 1. Explicit candidates from config (make has no dedicated env var).
        for source, cand in candidate_paths(self.id, ctx):
            if cand.exists():
                return _finalize(cand, source)

        # 2. Platform-specific PATH fallback.
        if ctx.os_name == "nt":
            search = ["mingw32-make.exe", "mingw32-make", "make.exe", "make"]
        else:
            search = ["make", "gmake"]

        for name in search:
            found = shutil.which(name)
            if found is not None:
                return _finalize(Path(found), "path")

        return DetectResult(
            found=False,
            path=None,
            version=None,
            reason="make not found on PATH (looked for "
            + ", ".join(search) + ")",
            source=None,
        )

    def hint(self, ctx: ResolveContext) -> str:
        return get_hints(ctx.os_name).install_hint(self.id)
