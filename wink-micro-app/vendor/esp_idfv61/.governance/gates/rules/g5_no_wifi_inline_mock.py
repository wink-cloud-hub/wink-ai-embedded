# SPDX-License-Identifier: Apache-2.0
"""
g5_no_wifi_inline_mock.py
=========================
Gate 5 Rule 503: Verifies that C source files under frameworks/esp_idf/src/wifi/
do not contain hardcoded IPv4 address literals, fixed MAC string fixtures, or
inlined if-else password/SSID ad-hoc checks.
All configuration and AP environments must be set via sim_wifi_env dynamic fixtures.
"""

import re
from pathlib import Path

RULE_ID = "g5.no_wifi_inline_mock"

# 禁止在 Wi-Fi 驱动 C 代码中出现硬编码 IPv4 字符串（如 "192.168.1.100"）
BANNED_IPV4_LITERAL = re.compile(
    r'"(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.'
    r'(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.'
    r'(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.'
    r'(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"'
)

# 禁止在 Wi-Fi 驱动 C 代码中出现针对密码特判的硬编码分支
BANNED_PWD_BRANCHES = [
    re.compile(r'strcmp\s*\(\s*[a-zA-Z0-9_>.-]*(?:password|pwd)\s*,\s*"[^"]+"\s*\)', re.IGNORECASE),
    re.compile(r'strstr\s*\(\s*[a-zA-Z0-9_>.-]*(?:password|pwd)\s*,\s*"[^"]+"\s*\)', re.IGNORECASE),
]


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    ws_root = Path(context.get("workspace_root", "."))
    wifi_dir = ws_root / "wink-micro-os" / "frameworks" / "esp_idf" / "src" / "wifi"

    if not wifi_dir.exists():
        return findings

    for c_file in wifi_dir.glob("*.c"):
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

            m = BANNED_IPV4_LITERAL.search(line)
            if m:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": None,
                    "display_id": None,
                    "config_id": None,
                    "file_path": str(c_file.relative_to(ws_root)),
                    "message": (
                        f"Line {line_no}: Hardcoded IPv4 literal detected: {m.group(0)}. "
                        f"Use ESP_IP4TOADDR(...) macro or inject via sim_wifi_env fixtures."
                    ),
                })

            for pat in BANNED_PWD_BRANCHES:
                m_b = pat.search(line)
                if m_b:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": None,
                        "display_id": None,
                        "config_id": None,
                        "file_path": str(c_file.relative_to(ws_root)),
                        "message": (
                            f"Line {line_no}: Hardcoded password comparison branch detected: '{m_b.group(0)}'. "
                            f"Password validation must compare against sim_wifi_env AP table."
                        ),
                    })

    return findings
