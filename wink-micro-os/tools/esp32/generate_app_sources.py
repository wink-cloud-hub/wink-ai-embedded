"""Auto-generate ``esp32_firmware/main/app_sources.cmake``.

Python port of ``esp32_firmware/generate_app_sources.ps1``. Scans the
selected app dir for ``*.c`` files (excluding host end-to-end ``test_*.c``),
optionally augments with ``wink-micro-os/samples/common/src/*.c`` minus the
helpers that migrated to BAL (ADR-0023 Stage 2), and writes a CMake fragment
that the ESP-IDF ``main`` component includes.

Behaviour matches the PowerShell original byte-for-byte modulo path
separators (``.as_posix()`` throughout). Output encoding is ``utf-8-sig``
with CRLF newlines to match PowerShell's ``Out-File -Encoding utf8`` on
Windows.

Callable in two ways:

* As a module::

      python -m tools.esp32.generate_app_sources --esp32-firmware-dir PATH ...

* As a script (no ``tools`` package required on ``sys.path``)::

      python wink-micro-os/tools/esp32/generate_app_sources.py --esp32-firmware-dir PATH ...
"""
from __future__ import annotations

import argparse
import fnmatch
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


# Ensure UTF-8 output on Windows so the ``✅`` success glyph doesn't mojibake
# through cp936. wink.py applies the same fix at its entry point; we repeat
# it here for direct-import callers (tests, ad-hoc ``python -m …`` runs)
# that bypass the CLI wrapper.
if sys.platform == "win32":
    for _stream in (sys.stdout, sys.stderr):
        if hasattr(_stream, "reconfigure"):
            try:
                _stream.reconfigure(encoding="utf-8", errors="replace")
            except (AttributeError, OSError):
                pass


# Helpers that migrated to BAL (ADR-0023 Stage 2). If any of these ever get
# accidentally copied back into ``samples/common/src`` they would collide
# with the BAL sources compiled via ``WINK_BAL_SOURCES`` and cause duplicate
# symbol link errors on ESP32 — so filter them out here as a safety net.
BAL_MIGRATED_NAMES: frozenset[str] = frozenset({
    "wink_blink_helper.c",
    "wink_button_helper.c",
    "wink_default_telemetry.c",  # legacy name before Stage 2.3 rename
    "wink_telemetry_helper.c",
    "wink_sim_ultrasonic_echo.c",  # moved to runtime/selftest/src/ in Stage 2.4
})


def _is_test_c(name: str) -> bool:
    """Return True for host end-to-end test files (``test_*.c``, any case)."""
    return fnmatch.fnmatch(name.lower(), "test_*.c")


def _is_bal_migrated(name: str) -> bool:
    return name.lower() in {n.lower() for n in BAL_MIGRATED_NAMES}


def _format_repo_relative(path: Path, repo_root: Path) -> str:
    """Format ``path`` for inclusion in the generated cmake fragment.

    Files under ``repo_root`` are emitted as
    ``${CMAKE_CURRENT_LIST_DIR}/../../<posix-rel-path>`` so the fragment is
    portable across machines. Files outside ``repo_root`` are emitted as
    absolute POSIX paths.
    """
    try:
        rel = path.resolve().relative_to(repo_root.resolve())
    except ValueError:
        return path.resolve().as_posix()
    return "${CMAKE_CURRENT_LIST_DIR}/../../" + rel.as_posix()


def _resolve_common_include_dir(app_dir: Path, repo_root: Path) -> Path:
    """Locate the common/include dir.

    Preference order (matches PS1):

    1. ``<parent-of-app>/common/include`` — sibling to the app dir. This is
       how ``wink-micro-app/<name>`` discovers ``wink-micro-app/common/include``.
    2. Fallback: ``<repo_root>/wink-micro-os/samples/common/include``.

    The fallback is returned even if it does not exist, mirroring the PS1
    which unconditionally emits the fallback path in that case.
    """
    sibling = app_dir.parent / "common" / "include"
    if sibling.exists():
        return sibling
    return repo_root / "wink-micro-os" / "samples" / "common" / "include"


def _collect_app_sources(app_dir: Path) -> list[Path]:
    """Return sorted-by-relative-path ``*.c`` files under ``app_dir`` minus tests."""
    files: list[Path] = []
    for p in app_dir.rglob("*.c"):
        if p.is_file() and not _is_test_c(p.name):
            files.append(p)
    # PowerShell's ``Get-ChildItem -Recurse`` walks depth-first with parent
    # entries preceding children; ``rglob`` on CPython yields in filesystem
    # order which on NTFS is typically directory-then-children. Sort by
    # posix-path for stable output across platforms.
    files.sort(key=lambda p: p.as_posix().lower())
    return files


def _collect_common_sources(repo_root: Path) -> list[Path]:
    """Return sorted ``wink-micro-os/samples/common/src/*.c`` minus BAL-migrated names.

    Returns ``[]`` silently if the dir doesn't exist (post-BAL-migration
    tree). Non-recursive: matches PS1's ``Get-ChildItem`` without ``-Recurse``.
    """
    common_dir = repo_root / "wink-micro-os" / "samples" / "common" / "src"
    if not common_dir.exists():
        return []
    files: list[Path] = []
    for p in common_dir.glob("*.c"):
        if p.is_file() and not _is_bal_migrated(p.name):
            files.append(p)
    files.sort(key=lambda p: p.as_posix().lower())
    return files


def _render_cmake(
    *,
    app_name: str,
    app_dir_rel: str,
    common_include_rel: str,
    app_sources_rel: list[str],
    common_sources_rel: list[str],
) -> str:
    """Render the cmake fragment. Uses LF newlines; caller writes with CRLF."""
    total = len(app_sources_rel) + len(common_sources_rel)
    all_sources = app_sources_rel + common_sources_rel
    # Match PS1: source lines start after a newline and are indented 4 spaces.
    if all_sources:
        joined = "\n    ".join(all_sources)
    else:
        # PowerShell's ``-join "`n    "`` on an empty array yields "", which
        # produces ``set(WINK_APP_SOURCES\n    \n    CACHE INTERNAL …)``.
        # Keep the same shape so a legitimate zero-source case still parses.
        joined = ""
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    return (
        "# ============================================================\n"
        "# AUTO-GENERATED by generate_app_sources.ps1 - DO NOT EDIT MANUALLY\n"
        "# ============================================================\n"
        f"# App Name: {app_name}\n"
        f"# Generated at: {now}\n"
        "# ============================================================\n"
        "\n"
        f'set(WINK_APP_NAME "{app_name}" CACHE STRING "Current samples app name" FORCE)\n'
        f'set(WINK_APP_DIR "{app_dir_rel}" CACHE PATH "Current samples app directory" FORCE)\n'
        f'set(WINK_APP_COMMON_INCLUDE_DIR "{common_include_rel}" CACHE PATH "samples/common include dir" FORCE)\n'
        "\n"
        "set(WINK_APP_SOURCES\n"
        f"    {joined}\n"
        f'    CACHE INTERNAL "Auto-scanned source files from samples/{app_name} + samples/common"\n'
        ")\n"
        "\n"
        f'message(STATUS "[WINK App Auto-Scan] App = {app_name}, source files = {total} (app={len(app_sources_rel)}, common={len(common_sources_rel)})")\n'
    )


@dataclass(frozen=True)
class GenerateResult:
    """Result of :func:`generate`."""
    out_path: Path
    app_name: str
    app_dir: Path
    app_count: int
    common_count: int

    @property
    def total_count(self) -> int:
        return self.app_count + self.common_count


def generate(
    *,
    app_dir: Path | None = None,
    app_name: str | None = None,
    esp32_firmware_dir: Path,
    repo_root: Path | None = None,
) -> GenerateResult:
    """Write ``esp32_firmware_dir/main/app_sources.cmake``; return a :class:`GenerateResult`.

    The returned object carries ``out_path``, ``app_name``, resolved ``app_dir``,
    and per-group counts (``app_count``, ``common_count``, ``total_count``).

    Exactly one of ``app_dir`` / ``app_name`` should typically be given. If
    both are absent, ``app_name`` defaults to ``devkitc_smoke`` (matching the
    PS1 default). ``repo_root`` defaults to the parent of ``esp32_firmware_dir``.

    Raises :class:`FileNotFoundError` if the resolved app dir doesn't exist.
    """
    esp32_firmware_dir = Path(esp32_firmware_dir).resolve()
    if repo_root is None:
        repo_root = esp32_firmware_dir.parent
    repo_root = Path(repo_root).resolve()

    if app_dir is not None:
        app_dir_resolved = Path(app_dir).resolve()
        resolved_name = app_dir_resolved.name
    else:
        resolved_name = app_name if app_name else "devkitc_smoke"
        # Legacy path preserved verbatim — do NOT switch to wink-micro-app here.
        app_dir_resolved = (
            repo_root / "wink-micro-os" / "samples" / resolved_name
        ).resolve()

    if not app_dir_resolved.exists():
        raise FileNotFoundError(f"App directory not found: {app_dir_resolved}")

    app_sources = _collect_app_sources(app_dir_resolved)
    common_sources = _collect_common_sources(repo_root)

    app_sources_rel = [_format_repo_relative(p, repo_root) for p in app_sources]
    common_sources_rel = [_format_repo_relative(p, repo_root) for p in common_sources]

    app_dir_rel = _format_repo_relative(app_dir_resolved, repo_root)
    common_include_rel = _format_repo_relative(
        _resolve_common_include_dir(app_dir_resolved, repo_root), repo_root
    )

    content = _render_cmake(
        app_name=resolved_name,
        app_dir_rel=app_dir_rel,
        common_include_rel=common_include_rel,
        app_sources_rel=app_sources_rel,
        common_sources_rel=common_sources_rel,
    )

    out_dir = esp32_firmware_dir / "main"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "app_sources.cmake"
    # utf-8-sig writes the BOM; newline="\r\n" enforces CRLF regardless of
    # the platform default. Matches PS1's ``Out-File -Encoding utf8``.
    out_path.write_text(content, encoding="utf-8-sig", newline="\r\n")
    return GenerateResult(
        out_path=out_path,
        app_name=resolved_name,
        app_dir=app_dir_resolved,
        app_count=len(app_sources),
        common_count=len(common_sources),
    )


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="generate_app_sources",
        description="Generate esp32_firmware/main/app_sources.cmake from an app dir.",
    )
    p.add_argument("--app-dir", type=Path, default=None,
                   help="Absolute or relative path to the app dir. Takes precedence over --app-name.")
    p.add_argument("--app-name", default="devkitc_smoke",
                   help="App name under wink-micro-os/samples/<name> (legacy path).")
    p.add_argument("--esp32-firmware-dir", type=Path, required=True,
                   help="Path to esp32_firmware/ (output is written to <dir>/main/app_sources.cmake).")
    p.add_argument("--repo-root", type=Path, default=None,
                   help="Repo root. Defaults to parent of --esp32-firmware-dir.")
    return p


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)
    try:
        result = generate(
            app_dir=args.app_dir,
            app_name=args.app_name,
            esp32_firmware_dir=args.esp32_firmware_dir,
            repo_root=args.repo_root,
        )
    except FileNotFoundError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    print(
        f"✅ Generated: {result.total_count} source(s) for app '{result.app_name}' "
        f"(app={result.app_count}, common={result.common_count}) -> {result.out_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
