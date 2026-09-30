# SPDX-License-Identifier: Apache-2.0
"""
g5_no_app_specific_branch.py
============================
Gate 5 Rule: Prohibits application-specific branching inside ESP-IDF facade C code.
Scans frameworks/esp_idf/src/** for hardcoded app names, strcmp(app, ...),
CONFIG_APP_* macros, or sample-specific bypass branches.
"""

import re
from pathlib import Path

RULE_ID = "g5.no_app_specific_branch"

BANNED_APP_PATTERNS = [
    # Comparing against app name variables
    re.compile(r'str(?:case)?(?:cmp|ncmp|str)\s*\(\s*(?:app|app_name|app_id|template_id)\b', re.IGNORECASE),
    # Hardcoded known carrier app names in strcmp/strstr
    re.compile(
        r'str(?:case)?(?:cmp|str)\s*\([^,]+,\s*"(?:esp_idfv61_|blink|blink_gpio|wifi_sta|mqtt_tcp|i2c_basic|ledc_basic|uart_echo|gptimer_alarm|bleprph)[^"]*"\s*\)',
        re.IGNORECASE,
    ),
    # App-specific CONFIG macros
    re.compile(r'#\s*ifdef\s+CONFIG_APP_', re.IGNORECASE),
    re.compile(r'#\s*if\s+defined\s*\(\s*CONFIG_APP_', re.IGNORECASE),
]


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    ws_root = Path(context.get("workspace_root", "."))
    src_dir = ws_root / "wink-micro-os" / "frameworks" / "esp_idf" / "src"

    if not src_dir.exists():
        return findings

    # Check changed files if provided, or all .c files in src
    changed_files = context.get("changed_files", [])
    target_files = []
    if changed_files:
        for f in changed_files:
            p = ws_root / f
            if p.suffix in (".c", ".h") and src_dir in p.parents:
                target_files.append(p)
    else:
        target_files = list(src_dir.rglob("*.c")) + list(src_dir.rglob("*.h"))

    for c_file in target_files:
        if not c_file.is_file():
            continue
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
            if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
                continue

            for pat in BANNED_APP_PATTERNS:
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
                            f"Line {line_no}: App-specific branch detected: '{m.group(0)}'. "
                            f"ESP-IDF facade code must remain 100% generic across all applications."
                        ),
                    })
                    break

    return findings
