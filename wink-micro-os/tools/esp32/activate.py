"""Harvest an activated ESP-IDF environment without owning the install.

Python port of the activation logic in ``scripts/build_esp32.ps1``. This
module never installs ESP-IDF (ADR-0030); it discovers an existing
installation via — in order:

1. **Hot start** — the current ``environ`` already has ``idf.py`` on PATH,
   ``IDF_PATH`` pointing at a valid IDF checkout, and ``idf.py --version``
   reports a *real* ESP-IDF v6.x banner (not the bare ``idf-exe`` shim
   which prints ``ESP-IDF Tools Installer v1.0.3``).
2. **EIM PowerShell profile** — on Windows, source the newest
   ``C:\\Espressif\\tools\\Microsoft.v6*.PowerShell_profile.ps1`` in a
   subprocess and harvest the resulting env deltas.
3. **Export script fallback** — if ``IDF_PATH`` is set but no EIM profile
   works, dot-source ``<IDF_PATH>/export.ps1`` (Windows) or ``export.sh``
   (Posix — untested).
4. Otherwise raise :class:`IdfActivationError` with a pointer at
   ``python tools/wink.py doctor`` and ``tools/preinstall.md §3``.

The returned :class:`IdfEnv` carries a **fresh** environ dict — the input
mapping (typically ``os.environ``) is never mutated. Callers pass the
returned ``env`` to ``subprocess.run(env=env)`` when spawning ``idf.py``.

Note: several small helpers below (``_find_eim_profiles``, ``_run``,
``_parse_kv_lines``, ``_validate_idf_path``, and the version regex) mirror
their twins in :mod:`tools.toolchain.providers.idf`. They are duplicated
deliberately rather than cross-imported to keep this module self-contained
for use outside the ``wink doctor / ensure_for`` flows — keep them in sync.
"""
from __future__ import annotations

import glob
import re
import shutil
import subprocess
import sys
from collections.abc import Mapping
from dataclasses import dataclass, field
from pathlib import Path


# ---------------------------------------------------------------------------
# Shared with toolchain.providers.idf — keep in sync.
# ---------------------------------------------------------------------------

_IDF_MAJOR_REQUIRED = 6
_EIM_PROFILE_GLOB = r"C:\Espressif\tools\Microsoft.v*.PowerShell_profile.ps1"

# Real ESP-IDF banner: "ESP-IDF v6.0.1", "ESP-IDF v6.0.1-dirty", etc.
_IDF_BANNER_RE = re.compile(r"ESP-IDF\s+v?(\d+)(?:\.(\d+))?(?:\.(\d+))?")

# The bare idf-exe shim ships with EIM and answers ``idf.py --version``
# even before a profile is sourced. Two banner shapes have been observed
# across EIM versions:
#
# * Older versions print ``ESP-IDF Tools Installer v1.0.3``.
# * The current version prints just ``v1.0.3`` (no "ESP-IDF" prefix) on
#   its own line.
#
# Neither matches the real ``ESP-IDF vX.Y.Z`` pattern. We detect shims
# explicitly so callers get an actionable error ("source an EIM profile
# or export script") instead of the vague "banner did not match".
#
# Do NOT match the bare substring "idf-exe" — that appears inside PATH
# output as ``C:\Espressif\tools\idf-exe\1.0.3\`` and would false-positive
# after a successful EIM source.
_SHIM_BANNER_RE = re.compile(
    r"ESP-IDF\s+Tools\s+Installer"
    r"|(?:^|\n)\s*v1\.\d+\.\d+\s*(?:\n|$)",
    re.IGNORECASE,
)

# Sourcing an EIM profile activates a Python venv (~12s on warm machines);
# the shared 10s probe timeout is not enough.
_EIM_PROBE_TIMEOUT_SEC = 30
_QUICK_PROBE_TIMEOUT_SEC = 10

# The keys we harvest from a sourced profile / export script.
_HARVEST_KEYS = (
    "IDF_PATH",
    "IDF_TOOLS_PATH",
    "IDF_PYTHON_ENV_PATH",
    "ESP_IDF_VERSION",
    "PATH",
    "ESP_ROM_ELF_DIR",
)

_ERROR_MSG_TEMPLATE = (
    "ESP-IDF shell is not ready and could not be activated automatically.\n"
    "  Run 'python tools/wink.py doctor' to diagnose, or see "
    "wink-micro-os/tools/preinstall.md §3 for ESP-IDF install "
    "instructions.\n"
    "  {detail}"
)


# ---------------------------------------------------------------------------
# Public types
# ---------------------------------------------------------------------------


class IdfActivationError(RuntimeError):
    """Raised when no ESP-IDF shell can be activated."""


@dataclass(frozen=True)
class IdfEnv:
    """Result of activating an IDF environment.

    Attributes:
        idf_path: Path to the ESP-IDF checkout (contains ``tools/idf.py``).
        environ: Full env dict suitable for ``subprocess.run(env=...)``.
        version: Detected ESP-IDF version string (e.g. ``"6.0.1"``) or None.
        source: Where the activation came from — one of ``"path"``,
            ``"eim-profile"``, ``"export-script"``, or None.
    """

    idf_path: Path
    environ: dict[str, str] = field(default_factory=dict)
    version: str | None = None
    source: str | None = None


# ---------------------------------------------------------------------------
# Small helpers (twins in tools.toolchain.providers.idf — keep in sync)
# ---------------------------------------------------------------------------


def _find_eim_profiles() -> list[Path]:
    """Return EIM v6.x PowerShell profile paths, most recent first.

    Exposed at module scope so tests can monkeypatch it directly.
    """
    matches = glob.glob(_EIM_PROFILE_GLOB)
    v6 = [Path(m) for m in matches if "Microsoft.v6" in Path(m).name]
    v6.sort(reverse=True)
    return v6


def _run(cmd: list[str], timeout: int, env: Mapping[str, str] | None = None):
    """Run ``cmd`` with a bounded timeout; return ``(rc, stdout+stderr)`` or None."""
    try:
        cp = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            env=dict(env) if env is not None else None,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    return cp.returncode, (cp.stdout or "") + (cp.stderr or "")


def _parse_kv_lines(text: str) -> dict[str, str]:
    """Extract ``KEY=VALUE`` lines from ``text`` into a dict."""
    out: dict[str, str] = {}
    for line in text.splitlines():
        s = line.strip()
        if "=" in s and not s.startswith("#"):
            k, _, v = s.partition("=")
            k = k.strip()
            if k.isidentifier() or k.replace("_", "").isalnum():
                out[k] = v.strip()
    return out


def _validate_idf_path(idf_path: Path) -> bool:
    """A valid IDF root is a directory containing ``tools/idf.py``."""
    try:
        if not idf_path.is_dir():
            return False
        return (idf_path / "tools" / "idf.py").is_file()
    except OSError:
        return False


def _parse_banner(text: str) -> tuple[int, ...] | None:
    """Return the (major, minor, patch) tuple from a real ESP-IDF banner.

    Rejects the ``idf-exe`` shim explicitly by returning ``None`` when the
    output matches the shim pattern.
    """
    if _SHIM_BANNER_RE.search(text):
        return None
    m = _IDF_BANNER_RE.search(text)
    if not m:
        return None
    parts: list[int] = [int(m.group(1))]
    if m.group(2) is not None:
        parts.append(int(m.group(2)))
    if m.group(3) is not None:
        parts.append(int(m.group(3)))
    return tuple(parts)


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------


def is_shell_ready(
    environ: Mapping[str, str] | None = None,
) -> tuple[bool, str | None]:
    """Probe the current environment for an already-activated IDF shell.

    Returns ``(True, version_str)`` iff:
      * ``idf.py`` resolves on the PATH implied by ``environ``,
      * ``idf.py --version`` reports a real ESP-IDF banner (not the shim),
      * the major version is 6, and
      * ``IDF_PATH`` points at a valid IDF checkout (``tools/idf.py`` exists).

    Otherwise returns ``(False, reason)`` explaining why.
    """
    if environ is None:
        environ = dict(_os_environ_snapshot())
    else:
        environ = dict(environ)

    # Defense-in-depth: strip MSYS/EMSDK vars that would make ``idf.py
    # --version`` misreport. An empty-but-present ``MSYSTEM`` is enough to
    # trigger IDF v6's MSYS detector across subprocess boundaries (see note
    # in activate() ``_k`` strip block).
    for _k in (
        "MSYSTEM", "MSYS", "MINGW_PREFIX", "MSYSTEM_PREFIX",
        "EMSDK", "EMSDK_NODE", "EMSDK_PYTHON",
    ):
        environ.pop(_k, None)

    path_value = environ.get("PATH") or environ.get("Path") or ""
    idf_py = shutil.which("idf.py", path=path_value)
    if not idf_py:
        return False, "idf.py not found on PATH"

    rv = _run(
        [idf_py, "--version"],
        timeout=_QUICK_PROBE_TIMEOUT_SEC,
        env=environ,
    )
    if rv is None:
        return False, "idf.py --version failed to execute"
    rc, out = rv
    if rc != 0:
        return False, f"idf.py --version exited with code {rc}"

    if _SHIM_BANNER_RE.search(out):
        return False, (
            "idf.py found but reports the installer shim, not a real "
            "ESP-IDF shell — activate an EIM profile or source "
            "export.ps1/export.sh"
        )

    parsed = _parse_banner(out)
    if parsed is None:
        return False, "idf.py --version output did not match ESP-IDF banner"

    version_str = ".".join(str(x) for x in parsed)
    if parsed[0] != _IDF_MAJOR_REQUIRED:
        return False, (
            f"ESP-IDF {version_str} is not v{_IDF_MAJOR_REQUIRED}.x "
            "(required)"
        )

    idf_path_str = (environ.get("IDF_PATH") or "").strip()
    if not idf_path_str:
        return False, "IDF_PATH is not set in the environment"
    if not _validate_idf_path(Path(idf_path_str)):
        return False, (
            f"IDF_PATH='{idf_path_str}' is not a valid ESP-IDF checkout "
            "(missing tools/idf.py)"
        )

    return True, version_str


def activate(environ: Mapping[str, str] | None = None) -> IdfEnv:
    """Ensure an activated ESP-IDF environment is available; return it.

    The input ``environ`` mapping (or ``os.environ`` when ``None``) is
    treated as read-only. A **new** dict is built and returned inside the
    :class:`IdfEnv` result. Callers pass ``env.environ`` to
    ``subprocess.run(env=...)`` — no process-wide state is changed.
    """
    base_env: dict[str, str] = dict(environ) if environ is not None else dict(
        _os_environ_snapshot()
    )

    # Defense-in-depth: strip MSYS/EMSDK contamination before probing the
    # shell. An empty-but-present MSYSTEM triggers IDF v6's "MSys/Mingw is no
    # longer supported" warning across subprocess boundaries (observed: when
    # MSYSTEM="" is inherited, ``idf.py --version`` prints the warning on
    # stderr and SUPPRESSES the ESP-IDF vX.Y banner on stdout, so our parser
    # can't confirm a real shell). Delete these keys outright — build.py also
    # strips before calling us, but direct callers (e.g. ad-hoc scripts)
    # might not.
    for _k in (
        "MSYSTEM", "MSYS", "MINGW_PREFIX", "MSYSTEM_PREFIX",
        "EMSDK", "EMSDK_NODE", "EMSDK_PYTHON",
    ):
        base_env.pop(_k, None)
    # Force UTF-8 for child processes too, so Chinese/Unicode output from
    # EIM profiles and idf.py doesn't mojibake on cp936.
    base_env["PYTHONUTF8"] = "1"
    base_env["PYTHONIOENCODING"] = "utf-8"

    # (1) Hot start — already ready.
    ok, info = is_shell_ready(base_env)
    if ok:
        idf_path = Path(base_env["IDF_PATH"].strip())
        return IdfEnv(
            idf_path=idf_path,
            environ=dict(base_env),
            version=info,
            source="path",
        )

    # (2) EIM PowerShell profile (Windows only).
    if sys.platform == "win32":
        for profile in _find_eim_profiles():
            merged = _try_eim_profile(base_env, profile)
            if merged is not None:
                idf_path, version, env_dict = merged
                return IdfEnv(
                    idf_path=idf_path,
                    environ=env_dict,
                    version=version,
                    source="eim-profile",
                )

    # (3) IDF_PATH export-script fallback.
    idf_path_str = (base_env.get("IDF_PATH") or "").strip()
    if idf_path_str:
        idf_path = Path(idf_path_str)
        if sys.platform == "win32":
            export = idf_path / "export.ps1"
            if export.is_file():
                merged = _try_export_ps1(base_env, export)
                if merged is not None:
                    real_root, version, env_dict = merged
                    return IdfEnv(
                        idf_path=real_root,
                        environ=env_dict,
                        version=version,
                        source="export-script",
                    )
        else:
            # Posix branch — untested on Windows CI but structured as a
            # clear elif for future porting. EIM is Windows-only, so we
            # skip it above and land here whenever IDF_PATH is set.
            export = idf_path / "export.sh"
            if export.is_file():
                merged = _try_export_sh(base_env, export)
                if merged is not None:
                    real_root, version, env_dict = merged
                    return IdfEnv(
                        idf_path=real_root,
                        environ=env_dict,
                        version=version,
                        source="export-script",
                    )

    # (4) Nothing worked.
    raise IdfActivationError(
        _ERROR_MSG_TEMPLATE.format(
            detail=(
                "Neither an activated shell nor an EIM v6 profile nor "
                "an IDF_PATH export script could be found. Last "
                f"readiness check: {info}."
            )
        )
    )


# ---------------------------------------------------------------------------
# Internal — activation subprocess wrappers
# ---------------------------------------------------------------------------


def _os_environ_snapshot() -> Mapping[str, str]:
    """Return a read-only snapshot of ``os.environ``.

    Isolated so tests that want to verify activate() does not mutate
    ``os.environ`` can rely on a stable capture point.
    """
    import os

    return dict(os.environ)


def _powershell_harvest_script(source_stmt: str) -> str:
    """Return a PowerShell one-liner that sources a profile and dumps env.

    Uses ``('KEY=' + $env:KEY)`` string-concatenation — embedding literal
    double-quotes inside a PS ``-Command`` argument is mangled by Windows
    argv quoting (IdfProvider hit this and left a comment noting the fix).
    """
    parts = [source_stmt, "idf.py --version"]
    for key in _HARVEST_KEYS:
        parts.append(f"Write-Output ('{key}=' + $env:{key})")
    return "; ".join(parts)


def _try_eim_profile(
    base_env: Mapping[str, str], profile: Path
) -> tuple[Path, str, dict[str, str]] | None:
    """Source ``profile`` in powershell.exe; return (idf_path, version, env) or None."""
    ps_script = _powershell_harvest_script(f". '{profile}'")
    cmd = ["powershell.exe", "-NoProfile", "-Command", ps_script]
    rv = _run(cmd, timeout=_EIM_PROBE_TIMEOUT_SEC, env=base_env)
    if rv is None or rv[0] != 0:
        return None
    return _finalize_harvest(base_env, rv[1])


def _try_export_ps1(
    base_env: Mapping[str, str], export: Path
) -> tuple[Path, str, dict[str, str]] | None:
    """Source ``export.ps1`` in powershell.exe; return (idf_path, version, env) or None."""
    ps_script = _powershell_harvest_script(f". '{export}'")
    cmd = ["powershell.exe", "-NoProfile", "-Command", ps_script]
    rv = _run(cmd, timeout=_EIM_PROBE_TIMEOUT_SEC, env=base_env)
    if rv is None or rv[0] != 0:
        return None
    return _finalize_harvest(base_env, rv[1])


def _try_export_sh(
    base_env: Mapping[str, str], export: Path
) -> tuple[Path, str, dict[str, str]] | None:
    """Source ``export.sh`` under bash -lc; return (idf_path, version, env) or None.

    Posix path — untested on this Windows machine, but kept as a clear
    branch for future porting rather than a TODO.
    """
    parts = [f". '{export}' >/dev/null 2>&1", "idf.py --version"]
    for key in _HARVEST_KEYS:
        parts.append(f'echo "{key}=${key}"')
    sh_script = " && ".join(parts[:2]) + "; " + "; ".join(parts[2:])
    cmd = ["bash", "-lc", sh_script]
    rv = _run(cmd, timeout=_EIM_PROBE_TIMEOUT_SEC, env=base_env)
    if rv is None or rv[0] != 0:
        return None
    return _finalize_harvest(base_env, rv[1])


def _finalize_harvest(
    base_env: Mapping[str, str], output: str
) -> tuple[Path, str, dict[str, str]] | None:
    """Parse harvest output, validate, and produce merged env.

    Rejects shim banners and non-v6 majors.
    """
    parsed = _parse_banner(output)
    if parsed is None:
        return None
    if parsed[0] != _IDF_MAJOR_REQUIRED:
        return None
    version_str = ".".join(str(x) for x in parsed)

    kv = _parse_kv_lines(output)
    idf_path_str = kv.get("IDF_PATH", "").strip()
    if not idf_path_str:
        return None
    idf_path = Path(idf_path_str)
    if not _validate_idf_path(idf_path):
        return None

    # Merge: start from base_env, overlay harvested keys (PATH wholesale).
    merged: dict[str, str] = dict(base_env)
    for key in _HARVEST_KEYS:
        val = kv.get(key)
        if val:
            merged[key] = val
    return idf_path, version_str, merged
