"""regex_ban pack: first_content_hit content regex path_rules."""
from __future__ import annotations

import re
from typing import Any

from tools.lint.engine.config import LintConfig
from tools.lint.engine.models import Finding


def check_regex_bans(
    file_path: str,
    text: str,
    layer_id: str,
    cfg: LintConfig,
) -> list[Finding]:
    """Evaluate path_rules with deny_content_regex / deny_regex (first hit)."""
    findings: list[Finding] = []
    for rule in cfg.path_rules:
        if rule.get("disabled"):
            continue
        patterns = rule.get("deny_content_regex") or rule.get("deny_regex") or []
        if not patterns:
            continue
        layers = rule.get("in") or []
        if layers and layer_id not in layers:
            continue
        path_globs = rule.get("paths") or []
        if path_globs and not _path_in_globs(file_path, path_globs):
            continue

        for pat in patterns:
            hit = _first_content_hit(text, pat)
            if hit is None:
                continue
            line_no, snippet = hit
            findings.append(
                Finding(
                    rule_id=rule["id"],
                    severity=rule.get("severity") or "error",
                    path=file_path.replace("\\", "/"),
                    line=line_no,
                    column=None,
                    message=rule.get("message")
                    or f"forbidden content matching /{pat}/",
                    snippet=snippet.rstrip("\n"),
                    help=rule.get("help"),
                    refs=tuple(rule.get("refs") or ()),
                    allowlisted=False,
                    rule_source=rule.get("rule_source") or "sdk",
                )
            )
            # One finding per rule (first pattern hit) is enough for parity.
            break
    return findings


def _first_content_hit(text: str, pattern: str) -> tuple[int, str] | None:
    try:
        cre = re.compile(pattern)
    except re.error:
        cre = re.compile(re.escape(pattern))
    for i, line in enumerate(text.splitlines(), start=1):
        if cre.search(line):
            return i, line
    # Also allow multiline match for completeness — report first line of file.
    m = cre.search(text)
    if m:
        line_no = text.count("\n", 0, m.start()) + 1
        snippet = text.splitlines()[line_no - 1] if text.splitlines() else ""
        return line_no, snippet
    return None


def _path_in_globs(rel_path: str, globs: list[str]) -> bool:
    from tools.lint.engine.allowlist import path_matches_allow

    return any(path_matches_allow(rel_path, g) for g in globs)
