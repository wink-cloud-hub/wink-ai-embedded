"""Lint runner orchestration: discover, classify, run packs, apply allowlist."""
from __future__ import annotations

from datetime import date
from pathlib import Path

from tools.lint.engine.allowlist import apply_allowlist, resolve_today
from tools.lint.engine.classify import classify_file
from tools.lint.engine.config import LintConfig
from tools.lint.engine.models import Finding
from tools.lint.packs.include_graph import check_includes
from tools.lint.packs.path_name import check_path_names
from tools.lint.packs.regex_ban import check_regex_bans
from tools.lint.packs.api_surface import check_api_surface
from tools.lint.packs.legacy_arduino import check_arduino_isolation

_SOURCE_SUFFIXES = {".c", ".h", ".cc", ".cpp", ".hpp", ".cxx", ".hxx"}


def run_lint(
    root: Path,
    cfg: LintConfig,
    packs: list[str] | None = None,
    paths: list[Path] | None = None,
    today: date | None = None,
) -> list[Finding]:
    """Run selected packs over ``root`` (or ``paths``) and return Findings."""
    root = root.resolve()
    effective_today = resolve_today(today)
    pack_set = set(packs) if packs else {"layering", "api"}
    files = _discover_files(root, paths)

    findings: list[Finding] = []
    for rel, abs_path in files:
        classified = classify_file(rel, cfg.layers, cfg.ignore)
        if classified is None:
            continue
        layer_id, kind = classified
        text = abs_path.read_text(encoding="utf-8", errors="replace")

        if pack_set & {"layering", "include_graph", "all"}:
            findings.extend(
                check_includes(rel, text, layer_id, cfg, root=root)
            )
            findings.extend(check_path_names(rel, layer_id, cfg))
            findings.extend(check_regex_bans(rel, text, layer_id, cfg))

        if pack_set & {"api", "api_surface", "all"}:
            findings.extend(check_api_surface(rel, text, layer_id, kind, cfg))

    if pack_set & {"arduino", "legacy_arduino", "all"}:
        # Arduino pack is path-scoped to kernel dirs; ignore --paths filter.
        findings.extend(check_arduino_isolation(root))

    return apply_allowlist(findings, cfg, effective_today)


def _discover_files(
    root: Path, paths: list[Path] | None
) -> list[tuple[str, Path]]:
    """Return (posix-rel-path, absolute-path) pairs under root."""
    if paths:
        out: list[tuple[str, Path]] = []
        for p in paths:
            abs_p = p if p.is_absolute() else (root / p)
            abs_p = abs_p.resolve()
            if not abs_p.is_file():
                continue
            try:
                rel = abs_p.relative_to(root).as_posix()
            except ValueError:
                rel = p.as_posix().replace("\\", "/")
            if abs_p.suffix.lower() in _SOURCE_SUFFIXES:
                out.append((rel, abs_p))
        return out

    out = []
    for abs_p in root.rglob("*"):
        if not abs_p.is_file():
            continue
        if abs_p.suffix.lower() not in _SOURCE_SUFFIXES:
            continue
        # Skip common junk / build trees
        parts = set(abs_p.relative_to(root).parts)
        if parts & {"build", ".git", "node_modules", "__pycache__"}:
            continue
        rel = abs_p.relative_to(root).as_posix()
        out.append((rel, abs_p))
    return out
