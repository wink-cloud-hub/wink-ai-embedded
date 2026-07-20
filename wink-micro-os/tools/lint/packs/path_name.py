"""path_name pack: filename-based path_rules (locator=filename)."""
from __future__ import annotations

import fnmatch
from pathlib import Path
from typing import Any

from tools.lint.engine.config import LintConfig
from tools.lint.engine.models import Finding


def check_path_names(
    file_path: str,
    layer_id: str,
    cfg: LintConfig,
) -> list[Finding]:
    """Evaluate path_rules with deny_filename against one classified file."""
    findings: list[Finding] = []
    name = Path(file_path).name
    for rule in cfg.path_rules:
        if rule.get("disabled"):
            continue
        if rule.get("locator", "filename") not in ("filename", None):
            # Content-hit rules handled by regex_ban pack.
            if rule.get("deny_content_regex") or rule.get("deny_regex"):
                continue
        patterns = rule.get("deny_filename") or []
        if not patterns:
            continue
        layers = rule.get("in") or []
        if layers and layer_id not in layers:
            continue
        if not any(fnmatch.fnmatch(name, pat) for pat in patterns):
            continue
        findings.append(
            Finding(
                rule_id=rule["id"],
                severity=rule.get("severity") or "error",
                path=file_path.replace("\\", "/"),
                line=None,
                column=None,
                message=rule.get("message")
                or f"forbidden filename pattern: {name}",
                snippet=None,
                help=rule.get("help"),
                refs=tuple(rule.get("refs") or ()),
                allowlisted=False,
                rule_source=rule.get("rule_source") or "sdk",
            )
        )
    return findings
