# SPDX-License-Identifier: Apache-2.0
"""
g5_no_inline_mock.py
====================
Gate 5 Rule: Verifies that C source files under frameworks/esp_idf/src/network/
do not contain inlined business domain URLs, test JSON fixtures, or ad-hoc if-else
URL mock branches. All mock routing must be registered dynamically via sim_net_responder.
"""

import re
from pathlib import Path

RULE_ID = "g5.no_inline_mock"

# 禁止在门面 C 代码中内联包含具体业务测试域名
BANNED_DOMAIN_PATTERNS = [
    re.compile(r'https?://[a-zA-Z0-9.-]+\.(org|com|net|io|cn|edu)', re.IGNORECASE),
    re.compile(r'mqtt://[a-zA-Z0-9.-]+', re.IGNORECASE),
]

# 禁止在门面 C 代码中出现针对特定测试 URL 的 if-else 分支特判
BANNED_MOCK_BRANCHES = [
    re.compile(r'strstr\s*\(\s*[a-zA-Z0-9_>.-]*url\s*,\s*"[^"]+"\s*\)'),
    re.compile(r'strcmp\s*\(\s*[a-zA-Z0-9_>.-]*url\s*,\s*"[^"]+"\s*\)'),
]


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    ws_root = Path(context.get("workspace_root", "."))
    net_dir = ws_root / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "network"

    if not net_dir.exists():
        return findings

    for c_file in net_dir.glob("*.c"):
        try:
            with open(c_file, "r", encoding="utf-8", errors="replace") as f:
                lines = f.readlines()
        except Exception as e:
            findings.append({
                "rule_id": RULE_ID,
                "severity": "error",
                "entry_id": None,
                "display_id": None,
                "config_id": None,
                "file_path": str(c_file.relative_to(ws_root)),
                "message": f"Failed to read file: {e}",
            })
            continue

        for line_no, line in enumerate(lines, start=1):
            stripped = line.strip()
            # 允许纯注释行
            if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
                continue

            for pat in BANNED_DOMAIN_PATTERNS:
                m = pat.search(line)
                if m:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": None,
                        "display_id": None,
                        "config_id": None,
                        "file_path": str(c_file.relative_to(ws_root)),
                        "message": (
                            f"Line {line_no}: Hardcoded business domain/URL detected: '{m.group(0)}'. "
                            f"All mock endpoints must be registered dynamically via sim_net_responder."
                        ),
                    })

            if c_file.name != "sim_net_responder.c":
                for pat in BANNED_MOCK_BRANCHES:
                    m = pat.search(line)
                    if m:
                        findings.append({
                            "rule_id": RULE_ID,
                            "severity": "error",
                            "entry_id": None,
                            "display_id": None,
                            "config_id": None,
                            "file_path": str(c_file.relative_to(ws_root)),
                            "message": (
                                f"Line {line_no}: Inlined URL branch check detected: '{m.group(0)}'. "
                                f"URL routing must be delegated to sim_net_responder_match()."
                            ),
                        })

    return findings
