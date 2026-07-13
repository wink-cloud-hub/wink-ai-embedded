"""Python interpreter capability provider (cap id: ``python``).

Detection strategy
------------------
1. If ``WINK_PYTHON`` env or a configured ``paths["python"]`` is set, probe
   that interpreter with ``{python} --version``.
2. Otherwise default to :data:`sys.executable` (the interpreter running
   ``wink.py``); for that case we use :data:`sys.version_info` directly to
   avoid an unnecessary subprocess.
3. Floor: >= 3.10.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from ..platform import get_hints
from ..resolve import ResolveContext, candidate_paths
from ..types import DetectResult, PROBE_TIMEOUT_SEC
from ._version import floor_str, parse_version, version_ge
from .base import Provider

_PYTHON_FLOOR: tuple[int, ...] = (3, 10)


def _probe(exe: Path) -> str | None:
    """Return combined stdout+stderr from ``exe --version``.

    Note: python 2 (and some early 3.x) prints ``--version`` to stderr.
    We capture both streams and merge, so the parser sees a consistent
    ``Python X.Y.Z`` string regardless of interpreter age.
    """
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


class PythonProvider(Provider):
    id = "python"

    def detect(self, ctx: ResolveContext) -> DetectResult:
        # 1. Explicit candidates: WINK_PYTHON env, then workspace/user config.
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
            if not version_ge(v_str, _PYTHON_FLOOR):
                return DetectResult(
                    found=False,
                    path=cand,
                    version=v_str,
                    reason=(
                        f"python {v_str} is below required floor "
                        f"{floor_str(_PYTHON_FLOOR)}"
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

        # 2. Default: use sys.executable and sys.version_info (no subprocess).
        info = sys.version_info
        v_str = f"{info.major}.{info.minor}.{info.micro}"
        exe = Path(sys.executable)
        if not version_ge(v_str, _PYTHON_FLOOR):
            return DetectResult(
                found=False,
                path=exe,
                version=v_str,
                reason=(
                    f"python {v_str} is below required floor "
                    f"{floor_str(_PYTHON_FLOOR)}"
                ),
                source="sys.executable",
            )
        return DetectResult(
            found=True,
            path=exe,
            version=v_str,
            reason=None,
            source="sys.executable",
        )

    def hint(self, ctx: ResolveContext) -> str:
        return get_hints(ctx.os_name).install_hint(self.id)
