"""Run ``idf.py`` with a stripped, UTF-8-armed, activated environment.

Python port of the "prepare env → run idf.py" tail of
``scripts/build_esp32.ps1``. The activation half (probe env, source EIM
profile, source ``export.ps1``) lives in :mod:`tools.esp32.activate`;
this module handles:

1. **Strip MSYS/EMSDK contamination** — ESP-IDF v6 refuses to build under
   MSYS/MINGW ("MSys/Mingw is no longer supported"); Emscripten env vars
   have been observed to leak IDF's Python venv discovery.
2. **Set UTF-8** — both before activation (so activation-time messages
   render on cp936 Chinese Windows) and after (because the EIM profile
   overwrites ``PYTHONUTF8``).
3. **Activate** via :func:`tools.esp32.activate.activate`.
4. **Run** ``idf.py -C <esp32_firmware_dir> <idf_args...>`` with the
   activated env, inheriting stdout/stderr so build output streams live.

We never use ``shell=True`` here — the activated env has ``idf.py`` on
PATH (via the IDF venv ``Scripts/`` dir on Windows), and letting cmd.exe
re-parse the argv would re-import the surrounding shell's environment
and reintroduce the MSYS/EMSDK vars we just stripped.

Callable in two ways:

* As a module::

      python -m tools.esp32.build --esp32-firmware-dir PATH -- <idf args...>

* As a script (no ``tools`` package required on ``sys.path``)::

      python wink-micro-os/tools/esp32/build.py --esp32-firmware-dir PATH -- <idf args...>
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

# Prefer package import when available; fall back to path-manipulation when
# invoked directly as a script (mirrors generate_app_sources.py).
try:
    from tools.esp32.activate import (
        IdfActivationError,
        activate as activate_idf,
    )
except ImportError:  # pragma: no cover - script-mode fallback
    _SDK = Path(__file__).resolve().parents[2]
    if str(_SDK) not in sys.path:
        sys.path.insert(0, str(_SDK))
    from tools.esp32.activate import (  # type: ignore  # noqa: E402
        IdfActivationError,
        activate as activate_idf,
    )


# Ensure UTF-8 output on Windows so any Chinese/Unicode strings the parent
# emits (activation banners, error text) don't mojibake through cp936.
# The subprocess also gets PYTHONUTF8=1 in its env below.
if sys.platform == "win32":
    for _stream in (sys.stdout, sys.stderr):
        if hasattr(_stream, "reconfigure"):
            try:
                _stream.reconfigure(encoding="utf-8", errors="replace")
            except (AttributeError, OSError):
                pass


# The exact set of MSYS/EMSDK env vars scripts/build_esp32.ps1 strips.
# Kept as constants so tests can reason about them.
#
# Note on MSYSTEM: the PowerShell original did ``$env:MSYSTEM = ""`` in its own
# process, which worked because the EIM profile dot-sources in-process and
# idf.py's MSYS detector appears to treat an in-process empty string
# differently than an empty string inherited across a subprocess boundary.
# When we launch a child ``powershell.exe`` to harvest env (activate.py) or
# a child ``idf.py`` directly, an empty-but-present MSYSTEM still triggers
# IDF v6's "MSys/Mingw is no longer supported" check (observed: idf.py
# --version prints the warning and SUPPRESSES the ESP-IDF vX.Y version line
# when MSYSTEM is "" vs. absent). Delete it outright to match a clean shell.
_MSYS_EMSDK_POP_KEYS: tuple[str, ...] = (
    "MSYSTEM",
    "MSYS",
    "MINGW_PREFIX",
    "MSYSTEM_PREFIX",
    "EMSDK",
    "EMSDK_NODE",
    "EMSDK_PYTHON",
)


def _strip_msys_emsdk(env: dict[str, str]) -> None:
    """Mutate ``env`` in place to strip MSYS/EMSDK contamination.

    All listed keys are deleted outright (not set to empty string) — an
    empty-but-present ``MSYSTEM`` still triggers IDF v6's MSYS detector
    across subprocess boundaries.
    """
    for key in _MSYS_EMSDK_POP_KEYS:
        env.pop(key, None)


def _assert_utf8(env: dict[str, str]) -> None:
    """Set ``PYTHONUTF8=1`` and ``PYTHONIOENCODING=utf-8`` on ``env``.

    Called twice: once before activation, once after (because EIM
    profiles overwrite these). Cheap enough that idempotence is fine.
    """
    env["PYTHONUTF8"] = "1"
    env["PYTHONIOENCODING"] = "utf-8"


def run_idf(
    *,
    esp32_firmware_dir: Path,
    idf_args: list[str],
    cwd: Path | None = None,
    environ: dict[str, str] | None = None,
) -> int:
    """Activate IDF, strip contamination, assert UTF-8, then run ``idf.py``.

    Args:
        esp32_firmware_dir: Path to ``esp32_firmware/`` (passed as
            ``idf.py -C``).
        idf_args: Extra args forwarded verbatim to ``idf.py`` (typically
            ``["build"]`` plus any ``-DXXX=...`` CMake defines).
        cwd: Working directory for the subprocess. Defaults to
            ``esp32_firmware_dir.parent`` (the repo root — matches PS1
            behavior of ``Set-Location (Split-Path -Parent $PSScriptRoot)``).
        environ: Base environment to derive from. Defaults to a copy of
            ``os.environ``. Never mutated — a local copy is made.

    Returns:
        The exit code of ``idf.py``.

    Raises:
        IdfActivationError: If no ESP-IDF shell can be prepared.
    """
    # 1. Start from a private copy so we never mutate the caller's dict.
    base: dict[str, str] = dict(environ) if environ is not None else dict(os.environ)

    # 2. Strip contamination and set UTF-8 BEFORE activation. This mirrors
    # the PS1 which does the same thing before sourcing the EIM profile.
    _strip_msys_emsdk(base)
    _assert_utf8(base)

    # 3. Activate. May raise IdfActivationError — let it propagate to the
    # caller with the preinstall.md §3 pointer built in.
    idf_env = activate_idf(base)

    # 4. The activated env dict was built by activate() from `base`, so it
    # inherits our contamination strip. But the EIM profile or export script
    # can reset PYTHONUTF8 (observed on Chinese Windows) or re-introduce
    # MSYS/EMSDK vars, so re-assert UTF-8 and re-strip before spawning idf.py.
    sub_env = dict(idf_env.environ)
    _assert_utf8(sub_env)
    _strip_msys_emsdk(sub_env)

    # 5. Resolve the idf.py entry point. PowerShell's `& idf.py` invokes the
    # .py via Windows file association, but Python's subprocess.run with
    # shell=False does NOT do that reliably (it looks for an exact
    # ``idf.py``/``idf.exe`` on PATH; EIM ships an ``idf.exe`` shim in
    # ``IDF_PYTHON_ENV_PATH/Scripts`` which usually works, but using the
    # venv's python + the idf.py script path inside IDF_PATH is the most
    # robust cross-platform form and matches what idf.py itself recommends
    # for CI).
    idf_py_path, idf_python = _resolve_idf_entry(idf_env.idf_path, sub_env)
    cmd = [idf_python, str(idf_py_path), "-C", str(esp32_firmware_dir), *list(idf_args)]

    # 6. Default cwd is the repo root (parent of esp32_firmware/).
    if cwd is None:
        cwd = esp32_firmware_dir.parent

    # 7. Run without capture_output so build output streams to console.
    cp = subprocess.run(cmd, cwd=str(cwd), env=sub_env)
    return cp.returncode


def _resolve_idf_entry(idf_path: Path, env: dict[str, str]) -> tuple[Path, str]:
    """Return ``(idf_py_script, python_executable)`` for invoking idf.py.

    Preference order (cross-platform):
    1. If ``IDF_PYTHON_ENV_PATH`` is set, use its Python (the venv the EIM
       profile activated) and ``<idf_path>/tools/idf.py``.
    2. Else fall back to ``sys.executable`` (whatever Python is running us).
    3. Last resort: look up ``idf.py`` on PATH — only used if neither of the
       above resolved a venv. On Windows this can hit the ``idf.exe`` shim
       launcher that EIM drops on PATH, so we prefer option 1.
    """
    import shutil

    idf_py_script = idf_path / "tools" / "idf.py"
    venv = (env.get("IDF_PYTHON_ENV_PATH") or "").strip()
    if venv:
        if sys.platform == "win32":
            candidate = Path(venv) / "Scripts" / "python.exe"
        else:
            candidate = Path(venv) / "bin" / "python"
        if candidate.is_file():
            return idf_py_script, str(candidate)
    # Fall back to the Python running build.py.
    if idf_py_script.is_file():
        return idf_py_script, sys.executable
    # Absolute last resort: bare "idf.py" via PATH lookup (shell-style).
    found = shutil.which("idf.py", path=env.get("PATH") or env.get("Path") or "")
    if found:
        return Path(found), sys.executable
    # Let subprocess.run fail with FileNotFoundError with a clear argv.
    return idf_py_script, "idf.py"


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="build",
        description=(
            "Activate ESP-IDF, strip MSYS/EMSDK contamination, and run "
            "idf.py -C <esp32_firmware_dir> <idf-args...>."
        ),
    )
    p.add_argument("--esp32-firmware-dir", type=Path, required=True,
                   help="Path to esp32_firmware/ (passed to idf.py -C).")
    p.add_argument("--cwd", type=Path, default=None,
                   help="Working directory for idf.py. Defaults to the parent of "
                        "--esp32-firmware-dir (repo root).")
    p.add_argument("idf_args", nargs=argparse.REMAINDER,
                   help="Everything after `--` is forwarded verbatim to idf.py.")
    return p


def _strip_leading_separator(idf_args: list[str]) -> list[str]:
    """Drop a leading ``--`` if argparse REMAINDER kept it.

    argparse's REMAINDER *does* include the leading ``--`` when the user
    passes one explicitly; strip it so ``idf.py`` doesn't see it.
    """
    if idf_args and idf_args[0] == "--":
        return idf_args[1:]
    return idf_args


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    idf_args = _strip_leading_separator(list(args.idf_args))
    if not idf_args:
        idf_args = ["build"]  # default matches PS1's `$IdfArgs = @("build")`.

    try:
        return run_idf(
            esp32_firmware_dir=args.esp32_firmware_dir,
            idf_args=idf_args,
            cwd=args.cwd,
        )
    except IdfActivationError as exc:
        print(f"[wink] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
