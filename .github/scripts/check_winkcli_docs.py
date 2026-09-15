#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Public winkcli docs gate (offline, deterministic).

Checks:
1. **Command parity** - every ``winkcli ...`` invocation found in the public
   toolchain surface (``wink-tools/docs/**``, ``wink-tools/README*.md``) is
   validated against ``wink-tools/docs/cli-help-tree.json``. Unknown command
   paths or flags fail with ``file:line``. Placeholders (``<...>`` / ``[...]``)
   are exempt from value checking; ``--`` switches to passthrough args.
2. **Generated appendix integrity** - the CLI-TREE block in
   ``docs/{zh,en}/01-cli-reference.md`` must list exactly the command paths
   present in the snapshot (catches hand edits of generated content).
3. **Redlines + ADR allowlist** - patterns from
   ``.github/scripts/redlines/public-docs.yaml`` must not appear anywhere in the
   public toolchain surface; ADR references are only valid when the number
   exists under ``docs/decisions/**`` of this repository.

Exit code 0 when all checks pass, 1 otherwise.
"""
from __future__ import annotations

import json
import re
import shlex
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Set, Tuple

REPO_ROOT = Path(__file__).resolve().parents[2]
SURFACE_ROOT = REPO_ROOT / "wink-tools"
SNAPSHOT = SURFACE_ROOT / "docs" / "cli-help-tree.json"
REDLINES = REPO_ROOT / ".github" / "scripts" / "redlines" / "public-docs.yaml"
DECISIONS = REPO_ROOT / "docs" / "decisions"

_APPENDIX_BEGIN = "<!-- BEGIN AUTO-GENERATED: CLI-TREE -->"
_APPENDIX_END = "<!-- END AUTO-GENERATED: CLI-TREE -->"
_APENDIX_HEADING = re.compile(r"^#{3,6}\s+`winkcli\s+([^`]+)`", re.MULTILINE)
_ADR_REF = re.compile(r"ADR-(\d{4})")
_APPENDIX_BLOCK = re.compile(
    re.escape(_APPENDIX_BEGIN) + r".*?" + re.escape(_APPENDIX_END), re.DOTALL
)
_WORD_TOKEN = re.compile(r"[A-Za-z0-9_-]+")
_FLAG_TOKEN = re.compile(r"--?[A-Za-z0-9-]+")

Findings = List[str]


# ---------------------------------------------------------------------------
# Snapshot / command-surface model
# ---------------------------------------------------------------------------

def _load_tree() -> Dict[str, Any]:
    if not SNAPSHOT.is_file():
        raise SystemExit(f"[FAIL] snapshot not found: {SNAPSHOT}")
    return json.loads(SNAPSHOT.read_text(encoding="utf-8"))


def _flag_strings(entry: Dict[str, Any]) -> Set[str]:
    flags: Set[str] = set()
    for item in entry.get("flags", []):
        for flag in item.get("flags") or []:
            flags.add(flag)
    return flags


def _node_names(node: Dict[str, Any]) -> Set[str]:
    return {node["name"], *node.get("aliases", [])}


def _command_index(tree: Dict[str, Any]) -> Dict[str, Dict[str, Any]]:
    index: Dict[str, Dict[str, Any]] = {}
    for command in tree["commands"]:
        for name in _node_names(command):
            index[name] = command
    return index


def _subcommand_index(node: Dict[str, Any]) -> Dict[str, Dict[str, Any]]:
    index: Dict[str, Dict[str, Any]] = {}
    for sub in node.get("subcommands", []):
        for name in _node_names(sub):
            index[name] = sub
    return index


def _tree_paths(tree: Dict[str, Any]) -> Set[str]:
    paths: Set[str] = set()

    def walk(nodes: Iterable[Dict[str, Any]], prefix: str) -> None:
        for node in nodes:
            path = f"{prefix} {node['name']}".strip()
            paths.add(path)
            for alias in node.get("aliases", []):
                paths.add(f"{prefix} {alias}".strip())
            walk(node.get("subcommands", []), path)

    walk(tree["commands"], "")
    return paths


# ---------------------------------------------------------------------------
# Markdown extraction
# ---------------------------------------------------------------------------

def _logical_lines(text: str) -> Iterable[Tuple[int, str]]:
    """Yield (start_line, joined_line) folding trailing \\ and ` continuations."""
    buffer = ""
    start = 0
    for lineno, raw in enumerate(text.splitlines(), start=1):
        if not buffer:
            start = lineno
        stripped = raw.rstrip()
        if stripped.endswith("\\") or stripped.endswith("`"):
            buffer += stripped[:-1] + " "
            continue
        yield start, buffer + stripped
        buffer = ""
    if buffer:
        yield start, buffer


def _parse_invocation(line: str) -> Optional[List[str]]:
    idx = line.find("winkcli")
    if idx < 0:
        return None
    if idx > 0 and (line[idx - 1].isalnum() or line[idx - 1] in "-_"):
        return None
    snippet = line[idx:]
    tick = snippet.find("`")
    if tick > 0:
        snippet = snippet[:tick]
    for sep in (" #", "\t#", "#"):
        pos = snippet.find(sep)
        if pos > 0:
            snippet = snippet[:pos]
            break
    snippet = snippet.rstrip(";&|").strip()
    if snippet.strip() in ("", "winkcli"):
        return None
    try:
        tokens = shlex.split(snippet, posix=False)
    except ValueError:
        tokens = snippet.split()
    tokens = [t.strip("'\"") for t in tokens]
    if not tokens or tokens[0] != "winkcli":
        return None
    return tokens


def _clean_word(token: str) -> Optional[str]:
    match = _WORD_TOKEN.match(token)
    return match.group(0) if match else None


def _clean_flag(token: str) -> Optional[str]:
    match = _FLAG_TOKEN.match(token)
    return match.group(0) if match else None


def _is_placeholder(token: str) -> bool:
    return token.startswith(("<", "[", "{"))


def _validate_invocation(
    doc_rel: str, lineno: int, tokens: List[str], tree: Dict[str, Any]
) -> Findings:
    findings: Findings = []
    global_flags = _flag_strings({"flags": tree.get("global_flags", [])}) | {"-h", "--help"}
    commands = _command_index(tree)

    i = 1
    if i >= len(tokens):
        return findings
    token = tokens[i]
    if _clean_flag(token) and token.startswith("-"):
        for raw in tokens[i:]:
            if raw == "--":
                break
            flag = _clean_flag(raw)
            if flag and flag not in global_flags:
                findings.append(f"{doc_rel}:{lineno}: unknown flag {flag!r} before any command")
        return findings

    word = _clean_word(token)
    if word is None:
        return findings
    node = commands.get(word)
    if node is None:
        findings.append(f"{doc_rel}:{lineno}: unknown command {word!r}")
        return findings
    i += 1

    subs = _subcommand_index(node)
    if subs and i < len(tokens) and not tokens[i].startswith("-") and not _is_placeholder(tokens[i]):
        sub_word = _clean_word(tokens[i])
        if sub_word is not None:
            sub = subs.get(sub_word)
            if sub is None:
                findings.append(
                    f"{doc_rel}:{lineno}: unknown subcommand {sub_word!r} for 'winkcli {word}'"
                )
                return findings
            node = sub
            i += 1

    allowed = _flag_strings(node) | global_flags
    passthrough = False
    while i < len(tokens):
        tok = tokens[i]
        if tok == "--":
            passthrough = True
        elif not passthrough:
            flag = _clean_flag(tok)
            if flag and flag not in allowed:
                findings.append(f"{doc_rel}:{lineno}: unknown flag {flag!r} for this command")
        i += 1
    return findings


def _check_command_parity(tree: Dict[str, Any]) -> Findings:
    findings: Findings = []
    targets = sorted((SURFACE_ROOT / "docs").rglob("*.md"))
    targets += [p for p in (SURFACE_ROOT / "README.md", SURFACE_ROOT / "README.zh-CN.md") if p.is_file()]

    for path in targets:
        rel = path.relative_to(REPO_ROOT).as_posix()
        text = _APPENDIX_BLOCK.sub("", path.read_text(encoding="utf-8"))
        for lineno, line in _logical_lines(text):
            if "winkcli" not in line:
                continue
            tokens = _parse_invocation(line)
            if tokens is None:
                continue
            findings.extend(_validate_invocation(rel, lineno, tokens, tree))
    return findings


# ---------------------------------------------------------------------------
# Appendix integrity
# ---------------------------------------------------------------------------

def _check_appendix(tree: Dict[str, Any]) -> Findings:
    findings: Findings = []
    expected = _tree_paths(tree)
    docs = [
        SURFACE_ROOT / "docs" / "zh" / "01-cli-reference.md",
        SURFACE_ROOT / "docs" / "en" / "01-cli-reference.md",
    ]
    for doc in docs:
        if not doc.is_file():
            findings.append(f"{doc.relative_to(REPO_ROOT).as_posix()}: missing")
            continue
        rel = doc.relative_to(REPO_ROOT).as_posix()
        text = doc.read_text(encoding="utf-8")
        if _APPENDIX_BEGIN not in text or _APPENDIX_END not in text:
            findings.append(f"{rel}: CLI-TREE markers missing")
            continue
        block = text.split(_APPENDIX_BEGIN, 1)[1].split(_APPENDIX_END, 1)[0]
        actual: Set[str] = set()
        for match in _APENDIX_HEADING.finditer(block):
            label = re.sub(r"\s*\(alias:.*$", "", match.group(1)).strip()
            actual.add(label)
        for missing in sorted(expected - actual):
            findings.append(f"{rel}: appendix missing 'winkcli {missing}'")
        for extra in sorted(actual - expected):
            findings.append(f"{rel}: appendix lists stale entry 'winkcli {extra}'")
    return findings


# ---------------------------------------------------------------------------
# Redlines + ADR allowlist
# ---------------------------------------------------------------------------

def _public_adr_numbers() -> Set[str]:
    nums: Set[str] = set()
    if DECISIONS.is_dir():
        for file in DECISIONS.rglob("*.md"):
            match = re.match(r"^(\d{4})-", file.name)
            if match:
                nums.add(match.group(1))
    return nums


def _load_redlines() -> Dict[str, Any]:
    if not REDLINES.is_file():
        raise SystemExit(f"[FAIL] redline rules not found: {REDLINES}")
    try:
        import yaml  # type: ignore
    except ImportError:
        print("[WARN] PyYAML not installed; skipping redline pattern scan (ADR check still runs).")
        return {"patterns": [], "allowlist": [], "adr": {"enabled": True}}
    return yaml.safe_load(REDLINES.read_text(encoding="utf-8")) or {}


def _check_redlines(rules: Dict[str, Any]) -> Findings:
    findings: Findings = []
    patterns = [re.compile(p) for p in rules.get("patterns", [])]
    allowlist = [re.compile(p) for p in rules.get("allowlist", [])]
    adr_enabled = bool((rules.get("adr") or {}).get("enabled", True))
    allowed_adrs = _public_adr_numbers() if adr_enabled else set()

    targets = sorted(SURFACE_ROOT.rglob("*.md")) + sorted(SURFACE_ROOT.rglob("*.yaml"))
    targets += [p for p in SURFACE_ROOT.rglob("*.json") if p.name == "cli-help-tree.json"]

    for path in targets:
        rel = path.relative_to(REPO_ROOT).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        for lineno, line in enumerate(text.splitlines(), start=1):
            if any(p.search(line) for p in allowlist):
                continue
            for pattern in patterns:
                if pattern.search(line):
                    findings.append(
                        f"{rel}:{lineno}: redline /{pattern.pattern}/ -> {line.strip()[:120]}"
                    )
            if adr_enabled:
                for match in _ADR_REF.finditer(line):
                    if match.group(1) not in allowed_adrs:
                        findings.append(
                            f"{rel}:{lineno}: references unpublished ADR-{match.group(1)}"
                        )
    return findings


def main() -> int:
    tree = _load_tree()
    rules = _load_redlines()

    findings: Findings = []
    findings += _check_command_parity(tree)
    findings += _check_appendix(tree)
    findings += _check_redlines(rules)

    if findings:
        print(f"[FAIL] winkcli docs gate: {len(findings)} finding(s)")
        for item in findings[:80]:
            print(f"  - {item}")
        if len(findings) > 80:
            print(f"  ... and {len(findings) - 80} more")
        return 1

    print(
        f"[PASS] winkcli docs gate "
        f"(winkcli v{tree.get('winkcli_version')}, {len(tree.get('commands', []))} commands)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
