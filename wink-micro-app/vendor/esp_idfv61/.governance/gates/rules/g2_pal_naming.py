# SPDX-License-Identifier: Apache-2.0
"""
g2_pal_naming.py
================
Gate 2 Rule: Scans changed files in pal/**, targets/**, osal/** against forbidden
protocol/device keywords.

Matching is token-aware rather than regex word-boundary based. A plain ``\b``
boundary cannot fire on identifiers such as ``pal_ws2812_write`` because ``_`` is
a regex word character, so the rule previously detected *mentions* in comments
while remaining blind to the actual API -- the precise class of violation it
exists to catch. We therefore tokenize identifiers and match contiguous
underscore-separated word runs.

Scope note: ``dal/`` is deliberately NOT scanned. Per the six-layer decision
tree, device-specific semantics (e.g. the WS2812 encoder) legitimately belong to
Layer 4 in ``dal/``; widening the scan there would flag architecturally correct
code. Only the generic layers (PAL / OSAL / targets) are in scope.
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
    "touch_pad",
    "servo",
    "oled",
]
# `i2c_addr` was removed: an I2C bus address is a cross-MCU generic bus concept
# (PAL_RESOURCE_I2C_ADDR is the correct way to express it), not device leakage.
# Device-specific addressing stays covered by `at24`. See gates.yaml for rationale.

# Generic-abstraction layers only. `dal/` is intentionally absent -- see module docstring.
TARGET_GLOB_PATTERNS = [
    "wink-micro-os/pal/**",
    "wink-micro-os/targets/**",
    "wink-micro-os/osal/**",
    "pal/**",
    "targets/**",
    "osal/**",
]

IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def split_words(text: str) -> list[str]:
    """Splits an identifier into its underscore-separated words, lowercased.

    ``PAL_RESOURCE_I2C_ADDR`` -> ['pal', 'resource', 'i2c', 'addr']
    """
    words: list[str] = []
    for token in IDENTIFIER.findall(text):
        words.extend(part.lower() for part in token.split("_") if part)
    return words


def match_forbidden_token(line: str, forbidden: list[str]) -> str | None:
    """Returns the first forbidden token whose word-run appears in the line, else None.

    Token-aware so that ``pal_ws2812_write`` and ``js_pal_ws2812_write`` are
    detected, while prose mentions in comments continue to be detected too.
    """
    words = split_words(line)
    if not words:
        return None
    for pattern in forbidden:
        needle = [w for w in pattern.lower().split("_") if w]
        n = len(needle)
        if n == 0:
            continue
        for i in range(len(words) - n + 1):
            if words[i:i + n] == needle:
                return pattern
    return None


def _is_allowlisted(rel_path: str, token: str, allowlist: list[dict]) -> bool:
    """Checks a (path, token) pair against reviewed allowlist entries."""
    for entry in allowlist:
        if not isinstance(entry, dict):
            continue
        path_glob = entry.get("path", "")
        if path_glob and not fnmatch.fnmatch(rel_path, path_glob):
            continue
        allowed_token = entry.get("token", "")
        if allowed_token and allowed_token.lower() != token.lower():
            continue
        return True
    return False


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    config = config or {}
    forbidden = config["forbidden_patterns"] if "forbidden_patterns" in config else DEFAULT_FORBIDDEN
    allowlist = config.get("allowlist") or []
    if not isinstance(forbidden, list) or not forbidden:
        raise ValueError("Gate 2: forbidden_patterns must be a non-empty list; refusing to run an empty scan.")

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
            token = match_forbidden_token(line, forbidden)
            if not token:
                continue
            if _is_allowlisted(rel_path, token, allowlist):
                continue
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": rel_path,
                "message": (
                    f"Forbidden device/protocol token '{token}' found in "
                    f"{rel_path}:{line_idx} -> '{line.strip()}'"
                ),
            })

    return findings
