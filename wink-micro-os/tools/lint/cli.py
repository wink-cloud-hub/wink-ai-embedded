"""CLI handler for ``wink lint``."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from tools.lint.engine.allowlist import parse_today_arg, resolve_today
from tools.lint.engine.config import LintConfigError, load_configs
from tools.lint.engine.report import (
    exit_code_for,
    format_json,
    format_sarif,
    format_text,
)
from tools.lint.engine.runner import run_lint


def handle_lint(args) -> None:
    """Entry point wired from wink.py; may call ``sys.exit``."""
    root = _resolve_root(getattr(args, "root", None))
    cfg_paths = _config_paths(root, getattr(args, "config", None) or [])

    if getattr(args, "explain", None):
        try:
            cfg = load_configs(cfg_paths)
        except LintConfigError as exc:
            print(f"[wink lint] config error: {exc}", file=sys.stderr)
            sys.exit(2)
        _print_explain(args.explain, cfg)
        sys.exit(0)

    try:
        cfg = load_configs(cfg_paths)
    except LintConfigError as exc:
        print(f"[wink lint] config error: {exc}", file=sys.stderr)
        sys.exit(2)

    today = resolve_today(parse_today_arg(getattr(args, "today", None)))
    packs = getattr(args, "pack", None)
    path_args = _resolve_paths(root, getattr(args, "paths", None), getattr(args, "changed", None))

    findings = run_lint(
        root,
        cfg,
        packs=packs,
        paths=path_args,
        today=today,
    )

    rule_filter = getattr(args, "rule", None)
    if rule_filter:
        findings = [f for f in findings if f.rule_id == rule_filter]

    report_allowlist = bool(getattr(args, "report_allowlist", False))
    if report_allowlist:
        findings = [
            f
            for f in findings
            if f.allowlisted or f.rule_id.endswith(".ALLOWLIST")
        ]

    fmt = getattr(args, "format", "text") or "text"
    if fmt == "json":
        body = format_json(findings)
    elif fmt == "sarif":
        body = format_sarif(findings)
    else:
        # Default text: hide allowlisted primaries (companions stay visible).
        visible = findings if report_allowlist else [f for f in findings if not f.allowlisted]
        body = format_text(visible)

    out_path = getattr(args, "output", None)
    if out_path:
        Path(out_path).write_text(body, encoding="utf-8")
    else:
        sys.stdout.write(body)

    code = exit_code_for(findings, strict=bool(getattr(args, "strict", False)))
    sys.exit(code)


def _resolve_root(root_arg: str | None) -> Path:
    if root_arg:
        return Path(root_arg).resolve()
    # wink.py lives in tools/; SDK root is parent of tools/
    here = Path(__file__).resolve()
    # .../wink-micro-os/tools/lint/cli.py → wink-micro-os
    return here.parents[2]


def _config_paths(root: Path, extras: list[str]) -> list[Path]:
    rules_dir = root / "tools" / "lint" / "rules"
    defaults = sorted(rules_dir.glob("*.yaml")) if rules_dir.is_dir() else []
    paths = list(defaults)
    for item in extras:
        paths.append(Path(item).resolve())
    if not paths:
        raise LintConfigError(f"no rule packs found under {rules_dir}")
    return paths


def _resolve_paths(
    root: Path, paths: list[str] | None, changed: str | None
) -> list[Path] | None:
    collected: list[Path] = []
    if paths:
        collected.extend(Path(p) for p in paths)
    if changed is not None:
        rev = changed or "HEAD"
        try:
            out = subprocess.check_output(
                ["git", "diff", "--name-only", rev],
                cwd=root,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
        except (subprocess.CalledProcessError, FileNotFoundError) as exc:
            print(f"[wink lint] --changed failed: {exc}", file=sys.stderr)
            sys.exit(2)
        for line in out.splitlines():
            line = line.strip()
            if line:
                collected.append(Path(line))
    return collected or None


def _print_explain(rule_id: str, cfg) -> None:
    """Print tech-design §8.2 explain template for RULE_ID."""
    rule = _find_rule(cfg, rule_id)
    if rule is None:
        print(f"Rule: {rule_id}  [unknown — not found in loaded packs]")
        print()
        print(
            "note: lint does not evaluate preprocessor conditions (#if 0) "
            "or macro-concatenated #include."
        )
        return

    sev = rule.get("severity", "?")
    src = rule.get("rule_source", "sdk")
    imm = "true" if rule.get("immutable") else "false"
    msg = (rule.get("message") or "").strip().replace("\n", " ")
    refs = ", ".join(rule.get("refs") or []) or "(none)"

    print(f"Rule: {rule_id}       [severity: {sev} | source: {src} | immutable: {imm}]")
    print(f"Message: {msg}")
    print()
    print("Rationale:")
    print(f"  See references: {refs}")
    print("  Layering / API shape gates keep App/BAL/DAL/PAL boundaries enforceable.")
    print()
    print(f"References: {refs}")
    print()
    print("Examples:")
    print("  BAD:  violate the deny pattern / filename / API shape for this rule")
    print("  GOOD: move HAL/PAL usage to .c; use named static APIs (ADR-0004)")
    print()
    print("Allowlist policy:")
    print(
        "  must include reason; prefer until ≤ 90 days; "
        "≤30d emit info, ≤7d emit warning (fail only with --strict)."
    )
    print()
    print("Active allowlist:")
    allows = rule.get("allow_paths") or []
    if not allows:
        print("  (none)")
    else:
        for entry in allows:
            until = entry.get("until") or "no-until"
            reason = entry.get("reason") or ""
            print(f"  - {entry.get('path')}    until {until}  ({reason})")
    print()
    print(
        "note: lint does not evaluate preprocessor conditions (#if 0) "
        "or macro-concatenated #include."
    )


def _find_rule(cfg, rule_id: str):
    for key in ("include_rules", "api_rules", "path_rules"):
        for rule in getattr(cfg, key, []) or []:
            if rule.get("id") == rule_id:
                return rule
    return None
