"""allow_paths matching and graded until expiry for wink lint."""
from __future__ import annotations

import os
import re
from datetime import date, datetime
from typing import Any, Literal

from tools.lint.engine.config import LintConfig
from tools.lint.engine.models import Finding

UntilStatus = Literal["active", "expiring_soon", "expiring_notice", "expired"]


def resolve_today(today: date | None) -> date:
    """Resolve effective 'today' from arg, $WINK_LINT_TODAY, or local date."""
    if today is not None:
        return today
    env = os.environ.get("WINK_LINT_TODAY")
    if env:
        return date.fromisoformat(env.strip())
    return date.today()


def evaluate_until(entry: dict[str, Any], today: date) -> UntilStatus:
    """Classify allow_paths entry relative to ``today``.

    - active: no until, or until > today+30
    - expiring_notice: until within 30 days (and > 7)
    - expiring_soon: until within 7 days (and >= today)
    - expired: until < today
    """
    raw = entry.get("until")
    if not raw:
        return "active"
    if isinstance(raw, date):
        until = raw
    else:
        until = date.fromisoformat(str(raw))
    days = (until - today).days
    if days < 0:
        return "expired"
    if days <= 7:
        return "expiring_soon"
    if days <= 30:
        return "expiring_notice"
    return "active"


def path_matches_allow(rel_path: str, pattern: str) -> bool:
    """Match relative path against allow_paths glob (posix, supports **)."""
    normalized = rel_path.replace("\\", "/")
    pat = pattern.replace("\\", "/")
    # Prefer PurePosixPath.match for simple patterns; fall back to ** regex.
    try:
        from pathlib import PurePosixPath

        if PurePosixPath(normalized).match(pat):
            return True
    except (ValueError, re.error):
        pass
    return _glob_match(normalized, pat)


def _glob_match(path: str, pattern: str) -> bool:
    parts: list[str] = []
    i = 0
    while i < len(pattern):
        if pattern[i : i + 2] == "**":
            parts.append(".*")
            i += 2
            if i < len(pattern) and pattern[i] == "/":
                i += 1
        elif pattern[i] == "*":
            parts.append("[^/]*")
            i += 1
        elif pattern[i] == "?":
            parts.append("[^/]")
            i += 1
        else:
            parts.append(re.escape(pattern[i]))
            i += 1
    return re.compile("^" + "".join(parts) + "$").match(path) is not None


def apply_allowlist(
    findings: list[Finding], cfg: LintConfig, today: date
) -> list[Finding]:
    """Annotate findings with allowlisted flags and companion expiry findings."""
    rules_by_id = _index_rules(cfg)
    out: list[Finding] = []
    for finding in findings:
        rule = rules_by_id.get(finding.rule_id)
        if rule is None:
            out.append(finding)
            continue
        entry = _matching_allow_entry(finding.path, rule.get("allow_paths") or [])
        if entry is None:
            out.append(finding)
            continue
        status = evaluate_until(entry, today)
        if status == "expired":
            out.append(
                Finding(
                    rule_id=finding.rule_id,
                    severity=finding.severity,
                    path=finding.path,
                    line=finding.line,
                    column=finding.column,
                    message=f"[allowlist expired] {finding.message}",
                    snippet=finding.snippet,
                    help=finding.help,
                    refs=finding.refs,
                    allowlisted=False,
                    rule_source=finding.rule_source,
                )
            )
            continue

        out.append(
            Finding(
                rule_id=finding.rule_id,
                severity=finding.severity,
                path=finding.path,
                line=finding.line,
                column=finding.column,
                message=finding.message,
                snippet=finding.snippet,
                help=finding.help,
                refs=finding.refs,
                allowlisted=True,
                rule_source=finding.rule_source,
            )
        )
        if status == "expiring_notice":
            out.append(
                _companion(
                    finding,
                    severity="info",
                    until=entry.get("until"),
                    reason=entry.get("reason"),
                )
            )
        elif status == "expiring_soon":
            out.append(
                _companion(
                    finding,
                    severity="warning",
                    until=entry.get("until"),
                    reason=entry.get("reason"),
                )
            )
    return out


def _companion(
    finding: Finding,
    *,
    severity: str,
    until: Any,
    reason: Any,
) -> Finding:
    reason_txt = f" ({reason})" if reason else ""
    return Finding(
        rule_id=f"{finding.rule_id}.ALLOWLIST",
        severity=severity,
        path=finding.path,
        line=finding.line,
        column=finding.column,
        message=(
            f"allow_paths for {finding.rule_id} expires on {until}{reason_txt}"
        ),
        snippet=finding.snippet,
        help="Renew, remove, or fix the underlying violation before until.",
        refs=finding.refs,
        allowlisted=False,
        rule_source=finding.rule_source,
    )


def _index_rules(cfg: LintConfig) -> dict[str, dict[str, Any]]:
    indexed: dict[str, dict[str, Any]] = {}
    for key in ("include_rules", "api_rules", "path_rules"):
        for rule in getattr(cfg, key, []) or []:
            rid = rule.get("id")
            if rid:
                indexed[rid] = rule
    return indexed


def _matching_allow_entry(
    rel_path: str, allow_paths: list[dict[str, Any]]
) -> dict[str, Any] | None:
    for entry in allow_paths:
        pattern = entry.get("path")
        if pattern and path_matches_allow(rel_path, pattern):
            return entry
    return None


def parse_today_arg(value: str | None) -> date | None:
    """Parse CLI --today YYYY-MM-DD; None if unset."""
    if not value:
        return None
    return datetime.strptime(value, "%Y-%m-%d").date()
