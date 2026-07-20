"""include_graph pack: enforce include allow/deny rules per layer."""
from __future__ import annotations

import re
from pathlib import Path
from typing import Any

from tools.lint.engine.config import LintConfig
from tools.lint.engine.includes import extract_includes
from tools.lint.engine.models import Finding


def check_includes(
    file_path: str,
    text: str,
    layer_id: str,
    cfg: LintConfig,
    *,
    root: Path | None = None,
) -> list[Finding]:
    """Evaluate include_rules for one file classified as ``layer_id``."""
    includes = extract_includes(text)
    findings: list[Finding] = []
    for rule in cfg.include_rules:
        if rule.get("disabled"):
            continue
        layers = rule.get("in") or []
        if layer_id not in layers:
            continue
        for deny in rule.get("deny") or []:
            findings.extend(
                _eval_deny(
                    file_path=file_path,
                    includes=includes,
                    rule=rule,
                    deny=deny,
                    root=root,
                )
            )
    return findings


def _eval_deny(
    *,
    file_path: str,
    includes: list[tuple[int, str, str]],
    rule: dict[str, Any],
    deny: dict[str, Any],
    root: Path | None,
) -> list[Finding]:
    forms = deny.get("include_forms") or ["quote", "angle"]
    if forms == ["both"] or forms == "both":
        forms = ["quote", "angle"]
    match_mode = deny.get("match") or "basename"
    pattern = deny.get("pattern") or ""
    try:
        cre = re.compile(pattern)
    except re.error:
        cre = re.compile(re.escape(pattern))

    except_basename = set(deny.get("except_basename") or [])
    except_literal = set(deny.get("except_literal") or [])

    out: list[Finding] = []
    for line, header, form in includes:
        if form not in forms:
            continue
        if header in except_literal:
            continue
        basename = Path(header).name
        if basename in except_basename:
            continue

        subject = _match_subject(header, match_mode, file_path, root)
        if cre.search(subject) is None:
            continue

        if form == "angle":
            snippet = f"#include <{header}>"
        else:
            snippet = f'#include "{header}"'
        out.append(
            Finding(
                rule_id=rule["id"],
                severity=rule.get("severity") or "error",
                path=file_path.replace("\\", "/"),
                line=line,
                column=None,
                message=rule.get("message") or f"forbidden include: {header}",
                snippet=snippet,
                help=None,
                refs=tuple(rule.get("refs") or ()),
                allowlisted=False,
                rule_source=rule.get("rule_source") or "sdk",
            )
        )
    return out


def _match_subject(
    header: str, match_mode: str, file_path: str, root: Path | None
) -> str:
    if match_mode == "basename":
        return Path(header).name
    if match_mode == "literal":
        return header
    if match_mode == "resolved":
        resolved = _try_resolve(header, file_path, root)
        return resolved if resolved is not None else header
    return Path(header).name


def _try_resolve(header: str, file_path: str, root: Path | None) -> str | None:
    """Best-effort resolve; return posix-relative path or None on failure."""
    candidates: list[Path] = []
    hdr = Path(header)
    if hdr.is_absolute():
        candidates.append(hdr)
    else:
        parent = Path(file_path).parent
        candidates.append(parent / hdr)
        if root is not None:
            candidates.append(root / hdr)
            # Common layout: pal/include/pal_hal.h for #include "pal_hal.h"
            candidates.append(root / "pal" / "include" / hdr.name)
            candidates.append(root / "dal" / "include" / hdr.name)
            candidates.append(root / "bal" / "include" / hdr.name)

    for cand in candidates:
        try:
            if cand.is_file():
                if root is not None:
                    try:
                        return cand.resolve().relative_to(root.resolve()).as_posix()
                    except ValueError:
                        return cand.as_posix()
                return cand.as_posix()
        except OSError:
            continue
    return None
