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
            header = f"allowlisted[{f.rule_id}]: {f.message}"
        else:
            header = f"{f.severity}[{f.rule_id}]: {f.message}"
        loc = _location_line(f)
        lines = [header, f"  --> {loc}"]
        if f.snippet and f.line is not None:
            lines.append("   |")
            lines.append(f" {f.line} | {f.snippet}")
            caret = (
                " " * max(1, len(str(f.line)))
                + " | "
                + "^" * max(1, len(f.snippet))
            )
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
    """SARIF 2.1.0 minimal subset for IDE / CI consumers."""
    rules_seen: dict[str, dict[str, Any]] = {}
    results: list[dict[str, Any]] = []

    for f in findings:
        if f.rule_id not in rules_seen:
            rules_seen[f.rule_id] = {
                "id": f.rule_id,
                "shortDescription": {"text": f.message},
                "properties": {"rule_source": f.rule_source},
            }
        start_line = 1 if f.line is None else f.line
        region: dict[str, Any] = {"startLine": start_line}
        if f.column is not None:
            region["startColumn"] = f.column
        props: dict[str, Any] = {"fingerprint": f.fingerprint}
        if f.line is None:
            props["locator"] = "filename"
        if f.allowlisted:
            props["allowlisted"] = True
        results.append(
            {
                "ruleId": f.rule_id,
                "level": _sarif_level(f.severity),
                "message": {"text": f.message},
                "locations": [
                    {
                        "physicalLocation": {
                            "artifactLocation": {
                                "uri": f.path.replace("\\", "/")
                            },
                            "region": region,
                        }
                    }
                ],
                "properties": props,
            }
        )

    doc = {
        "$schema": "https://json.schemastore.org/sarif-2.1.0.json",
        "version": "2.1.0",
        "runs": [
            {
                "tool": {
                    "driver": {
                        "name": "wink-lint",
                        "informationUri": (
                            "https://github.com/wink-ai/wink-ai-embedded"
                        ),
                        "rules": list(rules_seen.values()),
                    }
                },
                "results": results,
            }
        ],
    }
    return json.dumps(doc, indent=2, ensure_ascii=False) + "\n"


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


def _sarif_level(severity: str) -> str:
    if severity == "error":
        return "error"
    if severity == "warning":
        return "warning"
    return "note"


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
