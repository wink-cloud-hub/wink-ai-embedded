"""ESP-IDF capability provider (cap id: ``idf``).

Phase A stance (see ADR-0030)
-----------------------------
- Windows-only. Non-Windows hosts fail detection with a "requires Windows"
  reason (esp32 builds go through PowerShell + EIM in phase A).
- Wink **never** auto-installs ESP-IDF. ``install()`` always raises
  :class:`UnsupportedError` with the canonical message pointing at
  Espressif's IDF Manager (EIM).

Detection strategy (§7.2)
-------------------------
1. If ``idf.py`` is already on PATH: probe ``idf.py --version``, require
   major version == 6, and require ``IDF_PATH`` to point at a directory
   containing ``tools/idf.py``.
2. Else scan for an EIM PowerShell profile matching v6:
   ``C:\\Espressif\\tools\\Microsoft.v6*.PowerShell_profile.ps1``. Run
   PowerShell in a subprocess, source the profile, run ``idf.py --version``,
   and capture ``IDF_PATH`` / ``IDF_TOOLS_PATH``.
3. Else if ``IDF_PATH`` is set but ``idf.py`` is not on PATH → fail with a
   hint to source the export script / activate an EIM profile.
4. Else → fail with the never-auto-install ADR-0030 message.

The provider does **not** hardcode versioned cmake / ninja / xtensa paths.
Downstream scripts (``build_esp32.ps1``) are responsible for using the IDF-
activated environment to reach those.
"""
from __future__ import annotations

import glob
import shutil
import subprocess
from pathlib import Path

from ..resolve import ResolveContext
from ..types import DetectResult, PROBE_TIMEOUT_SEC, UnsupportedError
from ._version import parse_version
from .base import Provider

_IDF_MAJOR_REQUIRED = 6
_EIM_PROFILE_GLOB = r"C:\Espressif\tools\Microsoft.v*.PowerShell_profile.ps1"

_INSTALL_MSG = (
    "ESP-IDF is never auto-installed by Wink. Please install via Espressif "
    "IDF Manager (EIM) or see wink-micro-os/tools/preinstall.md §3."
)


def _find_eim_profiles() -> list[Path]:
    """Return EIM v6.x PowerShell profile paths, most recent first.

    Exposed at module scope so tests can monkeypatch it without having to
    mock ``glob.glob`` globally.
    """
    matches = glob.glob(_EIM_PROFILE_GLOB)
    v6 = [Path(m) for m in matches if "Microsoft.v6" in Path(m).name]
    # Most-recent-first for determinism if multiple v6.x profiles exist.
    v6.sort(reverse=True)
    return v6


def _run(cmd: list[str], **kw) -> tuple[int, str] | None:
    """Run ``cmd`` with the shared probe timeout; return ``(rc, output)``."""
    try:
        cp = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=PROBE_TIMEOUT_SEC,
            **kw,
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


class IdfProvider(Provider):
    id = "idf"

    def detect(self, ctx: ResolveContext) -> DetectResult:
        if ctx.os_name != "nt":
            return DetectResult(
                found=False,
                path=None,
                version=None,
                reason=(
                    "esp32 builds require Windows PowerShell 5.1 + EIM in "
                    "phase A; see preinstall.md §3"
                ),
                source=None,
            )

        # 1. idf.py already on PATH?
        idf_on_path = shutil.which("idf.py")
        if idf_on_path is not None:
            rv = _run(["idf.py", "--version"])
            if rv is not None and rv[0] == 0:
                v = parse_version(rv[1])
                if v is not None:
                    v_str = ".".join(str(x) for x in v)
                    if v[0] != _IDF_MAJOR_REQUIRED:
                        return DetectResult(
                            found=False,
                            path=None,
                            version=v_str,
                            reason=(
                                f"ESP-IDF {v_str} is not in required range "
                                f">=6.0,<7.0"
                            ),
                            source="path",
                        )
                    idf_path_env = ctx.environ.get("IDF_PATH", "").strip()
                    if idf_path_env and _validate_idf_path(Path(idf_path_env)):
                        return DetectResult(
                            found=True,
                            path=Path(idf_path_env),
                            version=v_str,
                            reason=None,
                            source="path",
                        )
                    return DetectResult(
                        found=False,
                        path=Path(idf_path_env) if idf_path_env else None,
                        version=v_str,
                        reason=(
                            "idf.py is on PATH but IDF_PATH is unset or does "
                            "not point to a valid IDF checkout (missing "
                            "tools/idf.py)"
                        ),
                        source="path",
                    )

        # 2. Try EIM PowerShell profile discovery.
        profiles = _find_eim_profiles()
        for profile in profiles:
            r = self._probe_eim_profile(profile)
            if r is not None:
                return r

        # 3. IDF_PATH set but idf.py unreachable.
        idf_path_env = ctx.environ.get("IDF_PATH", "").strip()
        if idf_path_env:
            return DetectResult(
                found=False,
                path=Path(idf_path_env),
                version=None,
                reason=(
                    "IDF_PATH is set but idf.py is not on PATH — source the "
                    "IDF export script or activate via EIM profile; see "
                    "preinstall.md §3"
                ),
                source="env:IDF_PATH",
            )

        # 4. Not installed at all.
        return DetectResult(
            found=False,
            path=None,
            version=None,
            reason=(
                "ESP-IDF v6.x not detected — install via Espressif-IDE "
                "Manager (EIM) or manual install; Wink will never auto-"
                "install IDF (see preinstall.md §3)"
            ),
            source=None,
        )

    @staticmethod
    def _probe_eim_profile(profile: Path) -> DetectResult | None:
        """Source ``profile`` in a PowerShell subprocess and probe idf.py.

        Returns a PASS/FAIL :class:`DetectResult` on success/version-mismatch,
        or ``None`` when this profile could not be probed (subprocess error).
        The caller can then try the next candidate profile.
        """
        cmd = [
            "powershell.exe",
            "-NoProfile",
            "-Command",
            (
                f". '{profile}'; "
                "idf.py --version; "
                "Write-Output \"IDF_PATH=$env:IDF_PATH\"; "
                "Write-Output \"IDF_TOOLS_PATH=$env:IDF_TOOLS_PATH\""
            ),
        ]
        rv = _run(cmd)
        if rv is None or rv[0] != 0:
            return None
        v = parse_version(rv[1])
        if v is None:
            return None
        v_str = ".".join(str(x) for x in v)
        kv = _parse_kv_lines(rv[1])
        idf_path_str = kv.get("IDF_PATH", "").strip()
        if v[0] != _IDF_MAJOR_REQUIRED:
            return DetectResult(
                found=False,
                path=Path(idf_path_str) if idf_path_str else None,
                version=v_str,
                reason=(
                    f"ESP-IDF {v_str} (from EIM profile {profile.name}) is "
                    f"not in required range >=6.0,<7.0"
                ),
                source="eim-profile",
            )
        if not idf_path_str:
            return None
        return DetectResult(
            found=True,
            path=Path(idf_path_str),
            version=v_str,
            reason=None,
            source="eim-profile",
        )

    def hint(self, ctx: ResolveContext) -> str:
        return (
            f"{_INSTALL_MSG} Install ESP-IDF v6.x via EIM and either add "
            "idf.py to PATH or let Wink discover the EIM PowerShell profile."
        )

    def install(self, ctx: ResolveContext) -> None:
        # ADR-0030: never auto-installed. Verbatim message.
        raise UnsupportedError(_INSTALL_MSG)
