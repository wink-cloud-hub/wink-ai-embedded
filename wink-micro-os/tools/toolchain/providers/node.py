"""Node.js capability provider (cap id: ``node``).

Detection: locate ``node`` on PATH via :func:`shutil.which`, then probe
``node --version`` (output like ``v20.11.0``; the leading ``v`` is stripped).

No version floor is enforced in phase A. ``install()`` inherits the base
``UnsupportedError``; ``hint()`` points at the Node.js LTS installer.
"""
from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

from ..resolve import ResolveContext
from ..types import DetectResult, PROBE_TIMEOUT_SEC
from ._version import parse_version
from .base import Provider


class NodeProvider(Provider):
    id = "node"

    def detect(self, ctx: ResolveContext) -> DetectResult:
        exe_name = "node.exe" if ctx.os_name == "nt" else "node"
        which = shutil.which(exe_name) or shutil.which("node")
        if which is None:
            return DetectResult(
                found=False,
                path=None,
                version=None,
                reason="node not found on PATH",
                source=None,
            )
        try:
            cp = subprocess.run(
                [which, "--version"],
                capture_output=True,
                text=True,
                timeout=PROBE_TIMEOUT_SEC,
            )
        except (OSError, subprocess.SubprocessError):
            return DetectResult(
                found=False,
                path=Path(which),
                version=None,
                reason="node --version failed",
                source="path",
            )
        if cp.returncode != 0:
            return DetectResult(
                found=False,
                path=Path(which),
                version=None,
                reason="node --version returned non-zero",
                source="path",
            )
        out = (cp.stdout or "") + (cp.stderr or "")
        v = parse_version(out)
        if v is None:
            return DetectResult(
                found=False,
                path=Path(which),
                version=None,
                reason="could not parse node --version output",
                source="path",
            )
        v_str = ".".join(str(x) for x in v)
        return DetectResult(
            found=True,
            path=Path(which),
            version=v_str,
            reason=None,
            source="path",
        )

    def hint(self, ctx: ResolveContext) -> str:
        if ctx.os_name == "nt":
            return (
                "Install Node.js LTS from https://nodejs.org/ or "
                "`winget install OpenJS.NodeJS.LTS`"
            )
        return "Install Node.js LTS from https://nodejs.org/ or your package manager"
