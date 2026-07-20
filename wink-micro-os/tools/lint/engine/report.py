"""Finding report formatters for wink lint."""
from __future__ import annotations

import json
from typing import Any

from tools.lint.engine.models import Finding


def format_text(findings: list[Finding]) -> str:
    """Render findings in rustc-like text form with ``-->`` locations."""
    if not findings:
        return "No lint findings.\n"
    blocks: list[str] = []
    for f in findings:
        if f.allowlisted and f.severity == "error":
            # Still show allowlisted errors when --report-allowlist; default
            # runners may filter. Keep them printable with a marker.
            header = f"allowlisted[{f.rule_id}]: {f.message}"
        else:
            header = f"{f.severity}[{f.rule_id}]: {f.message}"
        loc = _location_line(f)
        lines = [header, f"  --> {loc}"]
        if f.snippet and f.line is not None:
            lines.append("   |")
            lines.append(f" {f.line} | {f.snippet}")
            caret = " " * max(1, len(str(f.line))) + " | " + "^" * max(1, len(f.snippet))
            lines.append(caret)
        if f.help:
            lines.append(f"   = help: {f.help}")
        if f.refs:
            lines.append(f"   = refs: {', '.join(f.refs)}")
        lines.append(f"   = source: {f.rule_source}")
        blocks.append("\n".join(lines))
    return "\n\n".join(blocks) + "\n"


def format_json(findings: list[Finding]) -> str:
    """Minimal JSON list of finding dicts."""
    payload = [_finding_to_dict(f) for f in findings]
    return json.dumps(payload, indent=2, ensure_ascii=False) + "\n"


def format_sarif(findings: list[Finding]) -> str:
    """Placeholder SARIF; full schema lands in Task 10a."""
    # Minimal stub so --format sarif does not crash before Task 10a.
    return format_json(findings)


def exit_code_for(findings: list[Finding], *, strict: bool = False) -> int:
    """Map findings to process exit code.

    Unallowlisted errors always fail. With ``--strict``, warnings also fail.
    """
    for f in findings:
        if f.allowlisted:
            continue
        if f.severity == "error":
            return 1
        if strict and f.severity == "warning":
            return 1
    return 0


def _location_line(f: Finding) -> str:
    path = f.path.replace("\\", "/")
    if f.line is None:
        return path
    if f.column is not None:
        return f"{path}:{f.line}:{f.column}"
    return f"{path}:{f.line}"


def _finding_to_dict(f: Finding) -> dict[str, Any]:
    return {
        "rule_id": f.rule_id,
        "severity": f.severity,
        "path": f.path,
        "line": f.line,
        "column": f.column,
        "message": f.message,
        "snippet": f.snippet,
        "help": f.help,
        "refs": list(f.refs),
        "allowlisted": f.allowlisted,
        "rule_source": f.rule_source,
        "fingerprint": f.fingerprint,
    }
