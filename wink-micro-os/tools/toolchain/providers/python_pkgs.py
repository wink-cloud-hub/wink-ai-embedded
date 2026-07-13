"""Python-package capabilities (currently: jinja2, cap id ``jinja2``).

Detection runs the resolved python interpreter with a short probe script
that imports the package and prints its ``__version__``. In phase A we use
:data:`sys.executable` as the interpreter (or ``paths["python"]`` /
``WINK_PYTHON`` if the user has overridden it) rather than coupling to the
:class:`PythonProvider` result — this keeps the provider order flexible.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from ..platform import get_hints
from ..resolve import ResolveContext
from ..types import DetectResult, PROBE_TIMEOUT_SEC
from .base import Provider

_PROBE_SCRIPT = "import jinja2, sys; sys.stdout.write(jinja2.__version__)"


def _resolve_python(ctx: ResolveContext) -> Path:
    """Return the python interpreter to probe.

    Preference order matches :mod:`tools.toolchain.resolve.candidate_paths`
    for cap ``python``: WINK_PYTHON env, workspace config, user config,
    else :data:`sys.executable`.
    """
    env_val = ctx.environ.get("WINK_PYTHON", "").strip()
    if env_val:
        return Path(env_val)
    ws_val = ctx.workspace_paths.get("python")
    if ws_val:
        return Path(ws_val)
    user_val = ctx.user_paths.get("python")
    if user_val:
        return Path(user_val)
    return Path(sys.executable)


class Jinja2Provider(Provider):
    id = "jinja2"

    def detect(self, ctx: ResolveContext) -> DetectResult:
        python = _resolve_python(ctx)
        try:
            cp = subprocess.run(
                [str(python), "-c", _PROBE_SCRIPT],
                capture_output=True,
                text=True,
                timeout=PROBE_TIMEOUT_SEC,
            )
        except (OSError, subprocess.SubprocessError) as exc:
            return DetectResult(
                found=False,
                path=None,
                version=None,
                reason=f"failed to run python probe: {exc}",
                source=None,
            )
        if cp.returncode != 0:
            # ModuleNotFoundError or other import-time failure.
            err = (cp.stderr or "").strip().splitlines()
            tail = err[-1] if err else "import jinja2 failed"
            return DetectResult(
                found=False,
                path=None,
                version=None,
                reason=f"jinja2 not importable: {tail}",
                source=None,
            )
        version = (cp.stdout or "").strip() or None
        return DetectResult(
            found=True,
            path=python,
            version=version,
            reason=None,
            source=f"python:{python}",
        )

    def hint(self, ctx: ResolveContext) -> str:
        return get_hints(ctx.os_name).install_hint(self.id)
