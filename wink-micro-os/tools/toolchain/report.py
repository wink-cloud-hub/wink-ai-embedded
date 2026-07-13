"""Collect-all missing-dependency report renderer.

Aggregates per-provider detection results into a single human-readable report
grouped by severity, and provides the doctor-style exit hook that returns
non-zero only when a *required* dependency (tool or workspace) is missing.

Design references:
- Spec §8.2 (ESP-IDF is never auto-installed by Wink): when the report contains
  a required-tool item whose ``id`` is ``"idf"``, we surface the standard
  guidance sentence so scripted callers see it once per run.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Literal, NoReturn, TextIO

ReportItemKind = Literal["required_tool", "required_workspace", "optional"]

IDF_NOTICE = (
    "Note: ESP-IDF is never auto-installed by Wink. "
    "Install via Espressif IDF Manager (EIM); see preinstall.md §3."
)


@dataclass
class ReportItem:
    """A single line-item in the collect-all missing-dep report."""

    kind: ReportItemKind
    id: str
    message: str
    hint: str | None = None
    found_path: Path | None = None
    found_version: str | None = None
    min_version: str | None = None


def _pluralize(n: int, singular: str) -> str:
    return f"{n} {singular}" if n == 1 else f"{n} {singular}s"


def _format_version_suffix(item: ReportItem) -> str:
    """Return " (need X, found Y)" / " (need X)" / " (found Y)" or ""."""
    parts: list[str] = []
    if item.min_version:
        parts.append(f"need {item.min_version}")
    if item.found_version:
        parts.append(f"found {item.found_version}")
    if not parts:
        return ""
    return " (" + ", ".join(parts) + ")"


def _format_blocking(item: ReportItem) -> list[str]:
    lines = [f"  ✗ {item.id}: {item.message}{_format_version_suffix(item)}"]
    if item.found_path is not None:
        lines.append(f"      at {item.found_path}")
    if item.hint:
        lines.append(f"      -> {item.hint}")
    return lines


def _format_optional(item: ReportItem) -> list[str]:
    lines = [f"  ! {item.id}: {item.message}{_format_version_suffix(item)}"]
    if item.found_path is not None:
        lines.append(f"      at {item.found_path}")
    if item.hint:
        lines.append(f"      -> {item.hint}")
    return lines


def render_report(items: list[ReportItem], file: TextIO = sys.stderr) -> str:
    """Render the collect-all missing-dep report.

    The returned string is also written to ``file`` (default: stderr).
    Items are grouped by kind: required tools, required workspaces, then
    optional warnings. Blocking rows use ``✗``; optional rows use ``!``.
    Ends with a summary line ``N error(s), M warning(s)``. When empty, prints
    an all-clear line.
    """
    required_tools = [i for i in items if i.kind == "required_tool"]
    required_ws = [i for i in items if i.kind == "required_workspace"]
    optional = [i for i in items if i.kind == "optional"]

    lines: list[str] = ["Toolchain status:"]

    if not items:
        lines.append("  All toolchain dependencies are satisfied.")
        text = "\n".join(lines) + "\n"
        file.write(text)
        return text

    if required_tools:
        lines.append("Missing required tools:")
        for item in required_tools:
            lines.extend(_format_blocking(item))

    if required_ws:
        lines.append("Missing required workspaces:")
        for item in required_ws:
            lines.extend(_format_blocking(item))

    if optional:
        lines.append("Optional (non-blocking):")
        for item in optional:
            lines.extend(_format_optional(item))

    # ESP-IDF notice (spec §8.2)
    if any(i.id == "idf" and i.kind == "required_tool" for i in required_tools):
        lines.append("")
        lines.append(IDF_NOTICE)

    n_err = len(required_tools) + len(required_ws)
    n_warn = len(optional)
    lines.append("")
    lines.append(f"{_pluralize(n_err, 'error')}, {_pluralize(n_warn, 'warning')}")

    text = "\n".join(lines) + "\n"
    file.write(text)
    return text


def exit_for_report(items: list[ReportItem]) -> NoReturn:
    """Render the report to stderr and exit.

    Exits 1 iff at least one ``required_tool`` or ``required_workspace`` item
    is present; otherwise exits 0 (doctor all-green, warnings alone don't
    block).
    """
    render_report(items, file=sys.stderr)
    has_required = any(
        i.kind in ("required_tool", "required_workspace") for i in items
    )
    sys.exit(1 if has_required else 0)
