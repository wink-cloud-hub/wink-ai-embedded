# SPDX-License-Identifier: Apache-2.0
"""
g3_winkcli_lint.py
==================
Gate 3 Rule: Delegates layering and architecture boundary checks to winkcli lint.
"""

import os
import sys
import json
import shutil
import subprocess
from pathlib import Path

RULE_ID = "g3.winkcli_lint"

DEFAULT_PACKS = ["layering", "api"]


def find_winkcli_executable(ws_root: Path) -> list[str] | None:
    """Finds available winkcli or wink.py executable invocation."""
    # 1. System winkcli
    which_cli = shutil.which("winkcli")
    if which_cli:
        return [which_cli, "lint"]

    # 2. WINK_TOOLS_ROOT environment variable
    tools_root_env = os.environ.get("WINK_TOOLS_ROOT")
    if tools_root_env:
        wink_py = Path(tools_root_env) / "wink.py"
        if wink_py.exists():
            return [sys.executable, str(wink_py), "lint"]

    # 3. Local workspace candidate paths
    candidates = [
        ws_root / "packages" / "wink-tools" / "wink.py",
        ws_root.parent / "wink-ai" / "packages" / "wink-tools" / "wink.py",
        ws_root.parent / "packages" / "wink-tools" / "wink.py",
        ws_root / "wink-micro-os" / "tools" / "run_lint.py",
    ]
    for c in candidates:
        if c.exists():
            return [sys.executable, str(c), "lint"]

    return None


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    config = config or {}
    packs = config.get("packs", DEFAULT_PACKS)
    strict_env = config.get("strict_environment", True)
    timeout_sec = config.get("timeout_seconds", 120)

    ws_root = Path(context.get("workspace_root", "."))
    cmd_base = find_winkcli_executable(ws_root)

    # In CI or strict_environment, missing toolchain is a blocking ERROR
    is_ci = os.environ.get("CI") == "true" or os.environ.get("GITHUB_ACTIONS") == "true"
    if not cmd_base:
        if strict_env or is_ci:
            return [{
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": (
                    "Gate 3 Fail-Closed: 'winkcli' toolchain not found in environment. "
                    "In CI or strict mode, skipping is strictly forbidden."
                ),
            }]
        else:
            return [{
                "rule_id": RULE_ID,
                "severity": "warning",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": "Gate 3: 'winkcli' toolchain not found, skipping lint checks in non-strict mode.",
            }]

    # Filter changed files under wink-micro-os
    changed_files = context.get("changed_files", [])
    os_changed_files = []
    for f in changed_files:
        if f.startswith("wink-micro-os/"):
            rel_to_os = f[len("wink-micro-os/"):]
            if rel_to_os.endswith((".c", ".h", ".cpp", ".hpp")):
                os_changed_files.append(rel_to_os)

    cmd = list(cmd_base)
    cmd.extend([
        "--skip-toolchain-check",
        "--skip-auth-check",
        "--root", str(ws_root / "wink-micro-os"),
        "--format", "json",
    ])

    for p in packs:
        cmd.extend(["--pack", p])

    # In PR mode with changed files, perform incremental scan
    if context.get("mode") == "pr" and os_changed_files:
        cmd.append("--paths")
        cmd.extend(os_changed_files)
    elif context.get("mode") == "pr" and changed_files and not os_changed_files:
        # PR does not modify wink-micro-os C code: check layering and api only
        cmd = list(cmd_base) + [
            "--skip-toolchain-check",
            "--skip-auth-check",
            "--root", str(ws_root / "wink-micro-os"),
            "--pack", "layering",
            "--pack", "api",
            "--format", "json",
        ]

    try:
        proc = subprocess.run(
            cmd,
            cwd=str(ws_root),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout_sec,
        )
    except subprocess.TimeoutExpired:
        return [{
            "rule_id": RULE_ID,
            "severity": "error",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": None,
            "message": f"Gate 3: winkcli lint timed out after {timeout_sec} seconds.",
        }]
    except Exception as e:
        return [{
            "rule_id": RULE_ID,
            "severity": "error",
            "entry_id": None,
            "display_id": None,
            "config_id": None,
            "file_path": None,
            "message": f"Gate 3: winkcli lint failed to execute: {e}",
        }]

    # Parse JSON output
    stdout = proc.stdout.strip()
    if not stdout:
        if proc.returncode != 0:
            return [{
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": f"Gate 3: winkcli lint returned exit code {proc.returncode} with no output. Stderr: {proc.stderr[:200]}",
            }]
        return findings

    try:
        items = json.loads(stdout)
    except json.JSONDecodeError:
        # Non-JSON output (maybe plain text or error)
        if proc.returncode != 0:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": None,
                "message": f"Gate 3: winkcli lint returned error output:\n{stdout[:500]}",
            })
        return findings

    if isinstance(items, list):
        for item in items:
            if item.get("allowlisted"):
                continue  # Suppress allowlisted violations
            sev = item.get("severity", "error")
            rule_name = item.get("rule_id", "lint")
            path = item.get("path")
            line = item.get("line")
            msg = item.get("message", "")
            loc = f"{path}:{line}" if line else str(path)

            findings.append({
                "rule_id": f"g3.winkcli.{rule_name}",
                "severity": sev,
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": path,
                "message": f"{loc} - {msg}",
            })

    return findings
