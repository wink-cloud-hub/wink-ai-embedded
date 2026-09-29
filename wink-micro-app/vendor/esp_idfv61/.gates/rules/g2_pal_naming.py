# SPDX-License-Identifier: Apache-2.0
"""
g2_pal_naming.py
================
Gate 2 Rule: Scans changed files in pal/**, targets/**, osal/** against forbidden protocol/device keywords.
"""

import re
import subprocess
import fnmatch
from pathlib import Path

RULE_ID = "g2.pal_naming"

DEFAULT_FORBIDDEN = [
    "ws2812",
    "rgb",
    "pixel",
    "necir",
    "at24",
    "i2c_addr",
    "touch_pad",
    "servo",
    "oled",
]

TARGET_GLOB_PATTERNS = [
    "wink-micro-os/pal/**",
    "wink-micro-os/targets/**",
    "wink-micro-os/osal/**",
    "pal/**",
    "targets/**",
    "osal/**",
]


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    config = config or {}
    forbidden = config.get("forbidden_patterns", DEFAULT_FORBIDDEN)
    regex_pattern = re.compile(r"\b(" + "|".join(re.escape(p) for p in forbidden) + r")\b", re.IGNORECASE)

    ws_root = Path(context.get("workspace_root", "."))
    changed_files = context.get("changed_files", [])

    # Filter changed files in scope
    scoped_files = []
    for cf in changed_files:
        for pat in TARGET_GLOB_PATTERNS:
            if fnmatch.fnmatch(cf, pat):
                scoped_files.append(cf)
                break

    for rel_path in scoped_files:
        abs_path = ws_root / rel_path
        if not abs_path.exists():
            continue

        # Try to obtain added lines from git diff
        added_lines = []
        try:
            res = subprocess.run(
                ["git", "diff", "HEAD", "--", rel_path],
                cwd=str(ws_root),
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=5,
            )
            if res.returncode == 0 and res.stdout.strip():
                for line in res.stdout.splitlines():
                    if line.startswith("+") and not line.startswith("+++"):
                        added_lines.append(line[1:])
        except Exception:
            pass

        # If not under git or diff empty, scan the file directly
        if not added_lines:
            try:
                with open(abs_path, "r", encoding="utf-8", errors="replace") as f:
                    added_lines = f.readlines()
            except Exception:
                continue

        for line_idx, line in enumerate(added_lines, 1):
            m = regex_pattern.search(line)
            if m:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": None,
                    "display_id": None,
                    "config_id": None,
                    "file_path": rel_path,
                    "message": (
                        f"Forbidden device/protocol keyword '{m.group(0)}' found in "
                        f"{rel_path}:{line_idx} -> '{line.strip()}'"
                    ),
                })

    return findings
