"""Path resolution priority for toolchain caps.

Given a capability id (e.g. ``"emsdk"``), :func:`candidate_paths` returns the
explicit candidate paths to try, in priority order:

    1. Environment variable override (e.g. ``EMSDK``, ``WINK_GCC_PREFIX``).
    2. Workspace config: ``<workspace>/.wink/tools.json`` -> ``paths[cap]``.
    3. User config:      ``~/.wink/tools.json``          -> ``paths[cap]``.

Providers consume this list and additionally fall back to a ``PATH`` lookup
(via :func:`shutil.which`) themselves — that fallback is *not* returned by
this module because the resolved location is only knowable after the lookup.

The :class:`ResolveContext` snapshot is intentionally captured once per CLI
invocation so that a single ``wink doctor`` run sees a consistent view even
if the caller mutates ``os.environ`` between provider calls.
"""
from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path

from .config import (
    _load_config_file,
    _user_config_path,
    _workspace_config_path,
    ToolsConfig,
)

# Capability -> environment variable that, when set to a non-empty value,
# takes precedence over any tools.json entry for that cap.
#
# Caps not present here have no dedicated env override:
#   - cmake, make, node    -> located via PATH by their provider.
#   - jinja2               -> resolved as an importable Python package.
CAP_ENV_VARS: dict[str, str] = {
    "gcc":    "WINK_GCC_PREFIX",
    "python": "WINK_PYTHON",
    "emsdk":  "EMSDK",
    "idf":    "IDF_PATH",
}

# Environment variable overriding the managed tools_home directory.
_TOOLS_HOME_ENV = "WINK_TOOLS_HOME"


@dataclass
class ResolveContext:
    """Snapshot of the environment used by all providers in one CLI run.

    User and workspace ``paths`` are kept *separately* (not merged) so that
    :func:`candidate_paths` can surface both candidates in priority order.
    """

    environ: dict[str, str] = field(default_factory=dict)
    user_paths: dict[str, str] = field(default_factory=dict)
    workspace_paths: dict[str, str] = field(default_factory=dict)
    tools_home: Path | None = None
    workspace_root: Path | None = None
    os_name: str = ""

    @classmethod
    def snapshot(cls, workspace_root: Path | None = None) -> "ResolveContext":
        """Build a context from live ``os.environ`` and the on-disk configs."""
        # Load user + workspace tools.json separately so we retain priority
        # information (the merged form in ToolsConfig loses provenance).
        user_data = _load_config_file(_user_config_path())
        user_cfg = ToolsConfig.from_dict(user_data) if user_data is not None else ToolsConfig()

        ws_cfg: ToolsConfig | None = None
        if workspace_root is not None:
            ws_data = _load_config_file(_workspace_config_path(workspace_root))
            if ws_data is not None:
                ws_cfg = ToolsConfig.from_dict(ws_data)

        # tools_home: workspace overrides user.
        tools_home: Path | None
        if ws_cfg is not None and ws_cfg.tools_home is not None:
            tools_home = ws_cfg.tools_home
        else:
            tools_home = user_cfg.tools_home

        user_paths = {k: v for k, v in user_cfg.paths.items() if v}
        workspace_paths = (
            {k: v for k, v in ws_cfg.paths.items() if v} if ws_cfg is not None else {}
        )

        return cls(
            environ=dict(os.environ),
            user_paths=user_paths,
            workspace_paths=workspace_paths,
            tools_home=tools_home,
            workspace_root=workspace_root,
            os_name=os.name,
        )


def candidate_paths(cap_id: str, ctx: ResolveContext) -> list[tuple[str, Path]]:
    """Return ``(source_tag, path)`` candidates for ``cap_id`` in priority order.

    Source tags are:
        - ``"env:<VARNAME>"``   for an environment variable override.
        - ``"config:workspace"`` for ``<workspace>/.wink/tools.json``.
        - ``"config:user"``      for ``~/.wink/tools.json``.

    An empty list is returned when neither env nor either config knows about
    the cap; the calling provider should then fall back to PATH lookup itself
    (and may label that result with the sentinel tag ``"path"``).
    """
    candidates: list[tuple[str, Path]] = []

    env_var = CAP_ENV_VARS.get(cap_id)
    if env_var:
        env_val = ctx.environ.get(env_var, "").strip()
        if env_val:
            candidates.append((f"env:{env_var}", Path(env_val)))

    ws_val = ctx.workspace_paths.get(cap_id)
    if ws_val:
        candidates.append(("config:workspace", Path(ws_val)))

    user_val = ctx.user_paths.get(cap_id)
    if user_val:
        candidates.append(("config:user", Path(user_val)))

    return candidates


def resolve_tools_home(ctx: ResolveContext) -> Path | None:
    """Return the effective ``tools_home`` directory, or ``None`` if unset.

    Priority: ``WINK_TOOLS_HOME`` env > merged config (workspace wins over
    user) > ``None``.
    """
    env_val = ctx.environ.get(_TOOLS_HOME_ENV, "").strip()
    if env_val:
        return Path(env_val)
    return ctx.tools_home
