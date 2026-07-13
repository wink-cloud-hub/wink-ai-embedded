"""ensure_for: profile-driven toolchain gate + env injection.

This module is the top-level orchestrator consumed by ``wink.py`` (and, later,
by ``build_esp32.ps1`` / ``run-tests.ps1`` via ``python -m tools.wink doctor``
style invocations). It ties together the pieces built in tasks 2–7:

    profiles.py   →  which capability ids does this command need?
    providers/    →  how do I probe each capability?
    resolve.py    →  environment snapshot (env + tools.json)
    report.py     →  render-and-exit for missing deps

Design intent (spec §9.1 / ADR-0029):

- Fail *early* and *once*. ``ensure_for`` is called at the very top of each
  ``wink.py`` subcommand; if any required capability for the profile is
  missing, we render a single collect-all report to stderr and exit 1.
- Inject the *minimum* env needed for the resolved toolchain. Do not pollute
  environments for unrelated profiles (esp32 must not gain WinLibs bin on
  PATH; wasm must not gain IDF env vars).
- Cache detect() results within one ``ensure_for`` call. A single ``wink
  doctor`` may transitively reference the same capability (python, jinja2)
  via several profiles; we never re-probe.
- ``--skip-toolchain-check`` is a documented emergency escape hatch: it must
  print a prominent stderr warning and return without probing anything, so
  the user cannot accidentally build with a broken toolchain silently.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Callable

from . import providers as providers_mod
from .profiles import OPTIONAL_CAPS, WORKSPACE_DEPS, expand_profile
from .report import ReportItem, exit_for_report, render_report
from .resolve import ResolveContext
from .types import DetectResult

# ``wink.py`` subcommand → profile name in profiles.PROFILES.
# "doctor" is special-cased (probes everything).
_COMMAND_TO_PROFILE: dict[str, str] = {
    "gen":    "codegen",
    "host":   "host",
    "wasm":   "wasm",
    "test":   "test",
    "esp32":  "esp32",
    "web":    "web",
}

# Which subprocess-derived env var to publish per capability id, when the
# provider's DetectResult tells us where the SDK lives. Only used for SDKs
# whose downstream scripts require the env var to locate the toolchain.
#
# For idf: DetectResult.path is IDF_PATH; nothing else is authoritative.
_IDF_TOOLS_PATH_DEFAULT: str | None = None  # populated from env passthrough only.


def _skip_warning() -> None:
    """Emit the loud stderr warning for ``--skip-toolchain-check``."""
    sys.stderr.write(
        "[wink] WARNING: --skip-toolchain-check is set; skipping toolchain "
        "probes. Build failures beyond this point are on you.\n"
    )


def _probe_cap(
    cap_id: str,
    ctx: ResolveContext,
    cache: dict[str, DetectResult],
) -> DetectResult:
    """Detect ``cap_id`` at most once per ``ensure_for`` call.

    The provider is fetched from :data:`providers.REGISTRY` at call time so
    tests can monkey-patch the registry with fakes.
    """
    if cap_id in cache:
        return cache[cap_id]
    provider = providers_mod.REGISTRY[cap_id]
    result = provider.detect(ctx)
    cache[cap_id] = result
    return result


def _build_report_item(
    cap_id: str,
    result: DetectResult,
    *,
    kind: str,
    ctx: ResolveContext,
) -> ReportItem:
    """Create a :class:`ReportItem` for a failed probe."""
    provider = providers_mod.REGISTRY[cap_id]
    try:
        hint_text = provider.hint(ctx)
    except Exception:  # noqa: BLE001 — providers should not raise, but be defensive
        hint_text = None
    return ReportItem(
        kind=kind,  # type: ignore[arg-type]
        id=cap_id,
        message=result.reason or f"{cap_id} not found",
        hint=hint_text,
        found_path=result.path,
        found_version=result.version,
    )


def _prepend_path(directory: Path) -> None:
    """Prepend ``directory`` to ``PATH`` if not already at head."""
    d = str(directory)
    cur = os.environ.get("PATH", "")
    parts = cur.split(os.pathsep) if cur else []
    if parts and parts[0] == d:
        return
    if d in parts:
        parts.remove(d)
    parts.insert(0, d)
    os.environ["PATH"] = os.pathsep.join(parts)


def _inject_env_for_profile(
    profile: str,
    cache: dict[str, DetectResult],
    ctx: ResolveContext,
) -> None:
    """Apply the spec §9.1 env matrix.

    Only reads capabilities that were actually probed and marked ``found``.
    Silently no-ops for anything not in the cache — safer than reprobing.
    """
    def _bin_dir_of(cap_id: str) -> Path | None:
        r = cache.get(cap_id)
        if r is None or not r.found or r.path is None:
            return None
        # gcc: r.path is the .exe → parent is bin
        # cmake/make: r.path is the .exe → parent is bin
        p = r.path
        if p.is_dir():
            # Some providers may return a bin dir already; keep as-is.
            return p
        return p.parent

    if profile in ("codegen", "host", "test", "wasm"):
        for cap_id in ("gcc", "cmake", "make"):
            d = _bin_dir_of(cap_id)
            if d is not None:
                _prepend_path(d)
        # emsdk: intentionally NOT prepended (must be activated in parent shell).

    if profile == "esp32":
        # UTF-8 forcing is unconditional even if IDF was found via a source
        # that already sets these — cheap and defensive.
        os.environ["PYTHONUTF8"] = "1"
        os.environ["PYTHONIOENCODING"] = "utf-8"
        idf_result = cache.get("idf")
        if idf_result is not None and idf_result.found and idf_result.path is not None:
            os.environ["IDF_PATH"] = str(idf_result.path)
            # If parent shell already exports IDF_TOOLS_PATH, pass it through;
            # our EIM subprocess probe surfaces it in DetectResult.source only.
            existing_tools_path = ctx.environ.get("IDF_TOOLS_PATH", "").strip()
            if existing_tools_path:
                os.environ["IDF_TOOLS_PATH"] = existing_tools_path

    if profile == "web":
        d = _bin_dir_of("node")
        if d is not None:
            # Only prepend if not already discoverable on PATH (heuristic:
            # check whether the executable is already on PATH). For simplicity
            # and idempotence we always prepend; _prepend_path dedupes.
            _prepend_path(d)


def _all_required_caps(profile: str) -> list[str]:
    """Return required (non-optional) capability ids for ``profile``."""
    full = expand_profile(profile)
    optional = set(OPTIONAL_CAPS.get(profile, []))
    return [c for c in full if c not in optional]


def _optional_caps(profile: str) -> list[str]:
    """Return the optional (report-as-warning) capabilities for ``profile``."""
    # Optional caps as declared for the profile — do not expand transitively;
    # OPTIONAL_CAPS is authoritative for what's non-blocking.
    return list(OPTIONAL_CAPS.get(profile, []))


def ensure_for(
    command: str,
    *,
    workspace_root: Path | None,
    resolve_workspace_paths: Callable[[], dict[str, Path | None]] | None = None,
    skip: bool = False,
) -> None:
    """Probe capabilities for ``command`` and inject env, or exit(1).

    Parameters
    ----------
    command:
        One of ``"gen"``, ``"host"``, ``"wasm"``, ``"test"``, ``"esp32"``,
        ``"web"``, or ``"doctor"``. Values map through
        :data:`_COMMAND_TO_PROFILE`; ``"doctor"`` is a special case that
        probes every capability in :data:`providers.REGISTRY`.
    workspace_root:
        Repo root used by :meth:`ResolveContext.snapshot` to find
        ``<root>/.wink/tools.json``. May be ``None``.
    resolve_workspace_paths:
        Optional zero-arg callable returning ``{workspace_key: Path | None}``.
        Any value that is ``None`` is reported as a ``required_workspace``
        failure. Keys are informal (e.g. ``esp32_dir``, ``scripts_dir``).
    skip:
        When ``True``, print a warning to stderr and return immediately
        without probing anything (spec §9.2 escape hatch).
    """
    if skip:
        _skip_warning()
        return

    ctx = ResolveContext.snapshot(workspace_root)
    cache: dict[str, DetectResult] = {}
    items: list[ReportItem] = []

    if command == "doctor":
        # Doctor probes every registered capability; required-vs-optional is
        # decided by whether the cap appears in ANY profile's optional list
        # and NOT as required by any other profile.
        all_caps = list(providers_mod.REGISTRY.keys())

        # A cap is "required overall" if any profile lists it as required
        # (i.e. it is in expand_profile(p) minus OPTIONAL_CAPS[p]).
        required_overall: set[str] = set()
        optional_overall: set[str] = set()
        from .profiles import PROFILES
        for profile in PROFILES:
            req = set(_all_required_caps(profile))
            opt = set(_optional_caps(profile))
            required_overall |= req
            optional_overall |= opt
        # If a cap is required by some profile, it is required.
        optional_overall -= required_overall

        for cap_id in all_caps:
            result = _probe_cap(cap_id, ctx, cache)
            if result.found:
                continue
            if cap_id in optional_overall:
                items.append(_build_report_item(cap_id, result, kind="optional", ctx=ctx))
            else:
                items.append(_build_report_item(cap_id, result, kind="required_tool", ctx=ctx))

        # Workspace checks (aggregate across all profiles).
        if resolve_workspace_paths is not None:
            ws_map = resolve_workspace_paths()
            for ws_key, ws_path in ws_map.items():
                if ws_path is None:
                    items.append(ReportItem(
                        kind="required_workspace",
                        id=ws_key,
                        message=f"workspace path {ws_key!r} not found",
                        hint="check the workspace layout / clone the missing repo",
                    ))

        # For doctor, we ALWAYS render, and exit_for_report exits 1 iff any
        # required item is present. When all-green (items empty), it exits 0.
        # We want ensure_for("doctor") to return normally on all-green so the
        # CLI can print an all-clear message and continue. Emulate manually:
        has_required = any(
            i.kind in ("required_tool", "required_workspace") for i in items
        )
        if items or has_required:
            render_report(items, file=sys.stderr)
            if has_required:
                sys.exit(1)
        else:
            render_report(items, file=sys.stderr)  # all-clear line
        return

    # Regular subcommand path.
    if command not in _COMMAND_TO_PROFILE:
        raise ValueError(
            f"ensure_for: unknown command {command!r}; expected one of "
            f"{sorted(_COMMAND_TO_PROFILE) + ['doctor']}"
        )
    profile = _COMMAND_TO_PROFILE[command]

    required = _all_required_caps(profile)
    optional = _optional_caps(profile)

    # Probe required caps.
    for cap_id in required:
        result = _probe_cap(cap_id, ctx, cache)
        if not result.found:
            items.append(_build_report_item(cap_id, result, kind="required_tool", ctx=ctx))

    # Probe optional caps (warnings only).
    for cap_id in optional:
        result = _probe_cap(cap_id, ctx, cache)
        if not result.found:
            items.append(_build_report_item(cap_id, result, kind="optional", ctx=ctx))

    # Workspace path checks for this profile.
    ws_keys = WORKSPACE_DEPS.get(profile, [])
    if ws_keys and resolve_workspace_paths is not None:
        ws_map = resolve_workspace_paths()
        for ws_key in ws_keys:
            ws_path = ws_map.get(ws_key)
            if ws_path is None:
                items.append(ReportItem(
                    kind="required_workspace",
                    id=ws_key,
                    message=f"workspace path {ws_key!r} not found",
                    hint="check the workspace layout / clone the missing repo",
                ))

    # Blocking failure? Render report + exit 1.
    has_blocking = any(
        i.kind in ("required_tool", "required_workspace") for i in items
    )
    if has_blocking:
        # exit_for_report exits 1 iff there's a required item, else 0. Since
        # has_blocking is True we know it'll exit 1.
        exit_for_report(items)

    # Success path — inject env matrix per §9.1.
    _inject_env_for_profile(profile, cache, ctx)
