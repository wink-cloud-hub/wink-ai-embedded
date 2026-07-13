"""GCC capability provider (cap id: ``gcc``).

Detection strategy
------------------
1. Explicit candidates via :func:`candidate_paths` (``WINK_GCC_PREFIX`` env,
   workspace config, user config). ``WINK_GCC_PREFIX`` typically points at a
   directory containing ``bin/gcc(.exe)``.
2. Fall back to ``shutil.which("gcc")``.
3. Probe ``{gcc} --version`` for the version (floor: >= 14).
4. **On Windows**, additionally probe ``{gcc} -dumpmachine`` and require
   the triplet to contain ``w64-mingw32``. This rejects MSYS/Cygwin gcc
   variants that produce incompatible objects for our workflow (see the
   ``host-c-toolchain`` memory note: WinLibs MinGW is the only supported
   host compiler on Windows).
5. On POSIX, the native system gcc is fine; no triplet check.
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

_GCC_FLOOR: tuple[int, ...] = (14,)
_REQUIRED_TRIPLET_TAG = "w64-mingw32"


def _run(cmd: list[str]) -> tuple[int, str] | None:
    try:
        cp = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=PROBE_TIMEOUT_SEC,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    return cp.returncode, (cp.stdout or "") + (cp.stderr or "")


def _resolve_gcc_exe(path: Path, os_name: str) -> Path | None:
    """Turn a candidate path (file or prefix dir) into a real gcc executable.

    - If ``path`` is a file that exists, return it as-is.
    - If ``path`` is a directory, try ``<path>/bin/gcc[.exe]`` then
      ``<path>/gcc[.exe]``.
    - Otherwise return None.
    """
    if path.is_file():
        return path
    if path.is_dir():
        exe = "gcc.exe" if os_name == "nt" else "gcc"
        for sub in (path / "bin" / exe, path / exe):
            if sub.is_file():
                return sub
    return None


class GccProvider(Provider):
    id = "gcc"

    def detect(self, ctx: ResolveContext) -> DetectResult:
        # 1. Explicit candidates (env WINK_GCC_PREFIX, config).
        for source, cand in candidate_paths(self.id, ctx):
            resolved = _resolve_gcc_exe(cand, ctx.os_name)
            if resolved is None:
                continue
            r = self._validate(resolved, source, ctx.os_name)
            if r is not None:
                return r

        # 2. PATH fallback.
        exe_name = "gcc.exe" if ctx.os_name == "nt" else "gcc"
        which = shutil.which(exe_name) or shutil.which("gcc")
        if which is None:
            return DetectResult(
                found=False,
                path=None,
                version=None,
                reason="gcc not found on PATH",
                source=None,
            )
        r = self._validate(Path(which), "path", ctx.os_name)
        if r is not None:
            return r
        return DetectResult(
            found=False,
            path=Path(which),
            version=None,
            reason="gcc probe failed",
            source="path",
        )

    def _validate(
        self, exe: Path, source: str, os_name: str
    ) -> DetectResult | None:
        """Run the full validation pipeline on one candidate.

        Returns a DetectResult (found True or a specific failure), or None
        if this candidate is unusable in a way we should skip (allowing the
        caller to try the next candidate). We only return None for cases
        that don't produce useful diagnostic output; concrete failures
        (below-floor version, wrong triplet) short-circuit with found=False
        so the user sees the reason.
        """
        rv = _run([str(exe), "--version"])
        if rv is None:
            return None
        rc, out = rv
        if rc != 0:
            return None
        v = parse_version(out)
        if v is None:
            return None
        v_str = ".".join(str(x) for x in v)

        if not version_ge(v_str, _GCC_FLOOR):
            return DetectResult(
                found=False,
                path=exe,
                version=v_str,
                reason=(
                    f"gcc {v_str} is below required floor "
                    f"{floor_str(_GCC_FLOOR)}"
                ),
                source=source,
            )

        # Windows: enforce w64-mingw32 triplet.
        if os_name == "nt":
            rv2 = _run([str(exe), "-dumpmachine"])
            if rv2 is None:
                return DetectResult(
                    found=False,
                    path=exe,
                    version=v_str,
                    reason="gcc -dumpmachine failed",
                    source=source,
                )
            rc2, triplet_out = rv2
            if rc2 != 0:
                return DetectResult(
                    found=False,
                    path=exe,
                    version=v_str,
                    reason="gcc -dumpmachine failed",
                    source=source,
                )
            triplet = triplet_out.strip().splitlines()[0] if triplet_out.strip() else ""
            if _REQUIRED_TRIPLET_TAG not in triplet:
                return DetectResult(
                    found=False,
                    path=exe,
                    version=v_str,
                    reason=(
                        f"gcc target triplet {triplet!r} is not "
                        f"{_REQUIRED_TRIPLET_TAG}; install WinLibs MinGW"
                    ),
                    source=source,
                )

        return DetectResult(
            found=True,
            path=exe,
            version=v_str,
            reason=None,
            source=source,
        )

    def hint(self, ctx: ResolveContext) -> str:
        return get_hints(ctx.os_name).install_hint(self.id)
