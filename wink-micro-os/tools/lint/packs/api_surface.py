"""api_surface pack: ops/vtable, malloc hotpath, bool DAL public APIs."""
from __future__ import annotations

import re
from typing import Any

from tools.lint.engine.config import LintConfig
from tools.lint.engine.lexer import strip_comments_and_strings, strip_strings
from tools.lint.engine.models import Finding


def check_api_surface(
    file_path: str,
    text: str,
    layer_id: str,
    kind: str,
    cfg: LintConfig,
) -> list[Finding]:
    findings: list[Finding] = []
    for rule in cfg.api_rules:
        if rule.get("disabled"):
            continue
        layers = rule.get("in") or []
        if layers and layer_id not in layers:
            continue
        findings.extend(_eval_api_rule(file_path, text, kind, rule))
    return findings


def _eval_api_rule(
    file_path: str, text: str, kind: str, rule: dict[str, Any]
) -> list[Finding]:
    ctx = rule.get("context") or {}
    scanned = text
    if ctx.get("strip_comments", True):
        scanned = strip_comments_and_strings(scanned)
    if ctx.get("strip_strings", True):
        scanned = strip_strings(scanned)

    scope_map = (ctx.get("scope_by_kind") or {})
    scope = scope_map.get(kind, "full")
    if scope == "declarations_only":
        scanned = _declarations_only_view(scanned)

    except_res = [_compile(p) for p in (rule.get("except_regex") or [])]
    out: list[Finding] = []
    for item in rule.get("deny_regex") or []:
        pattern = item.get("pattern") if isinstance(item, dict) else str(item)
        cre = _compile(pattern)
        for line_no, line in enumerate(scanned.splitlines(), start=1):
            if not cre.search(line):
                continue
            if any(ex.search(line) for ex in except_res):
                continue
            out.append(
                Finding(
                    rule_id=rule["id"],
                    severity=rule.get("severity") or "error",
                    path=file_path.replace("\\", "/"),
                    line=line_no,
                    column=None,
                    message=rule.get("message") or f"matched /{pattern}/",
                    snippet=line.strip(),
                    help=rule.get("help"),
                    refs=tuple(rule.get("refs") or ()),
                    allowlisted=False,
                    rule_source=rule.get("rule_source") or "sdk",
                )
            )
    return out


def _declarations_only_view(text: str) -> str:
    """Keep lines that look like declarations / struct fields; blank others.

    Function bodies in public headers are suppressed so local ``_ops`` names
    do not fire NO-OPS-VTABLE; struct / prototype lines remain.
    """
    out: list[str] = []
    brace_depth = 0
    for line in text.splitlines():
        stripped = line.strip()
        opens = line.count("{")
        closes = line.count("}")
        keep = False
        if brace_depth == 0:
            # Prototypes, typedef/struct starts, macros, global decls.
            if (
                stripped.endswith(";")
                or stripped.startswith("#")
                or "struct" in stripped
                or "typedef" in stripped
                or stripped.endswith("{")
                or re.search(r"\b\w+\s*\([^;]*\)\s*;?\s*$", stripped)
            ):
                keep = True
        # Inside braces of a function body (depth after processing prior lines
        # was >0 and this isn't a top-level struct field line): drop.
        if brace_depth > 0 and not stripped.endswith(";") and "{" not in stripped:
            keep = False
        if brace_depth == 0 and "_ops" in stripped and "{" in stripped:
            keep = True
        out.append(line if keep else " " * len(line))
        brace_depth += opens - closes
        if brace_depth < 0:
            brace_depth = 0
    return "\n".join(out)


def _compile(pattern: str) -> re.Pattern[str]:
    try:
        return re.compile(pattern)
    except re.error:
        return re.compile(re.escape(pattern))
