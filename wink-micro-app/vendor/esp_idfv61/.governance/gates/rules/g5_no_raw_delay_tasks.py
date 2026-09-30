# SPDX-License-Identifier: Apache-2.0
"""
g5_no_raw_delay_tasks.py
========================
Gate 5 Rule: Prohibits raw ad-hoc delay tasks inside protocol stacks and drivers.
Enforces the use of FreeRTOS Timer Daemon delayed work items instead of spawning
temporary fibers with xTaskCreate to avoid exhausting task slots in LITE profile.
"""

import re
from pathlib import Path

RULE_ID = "g5.no_raw_delay_tasks"

# Banned temporary task names
BANNED_TASK_NAMES = re.compile(
    r'xTaskCreate\s*\([^,]+,\s*"(?:wifi_connect_task|mqtt_connect_task|delay_task|temp_task|sleep_task|connect_task)[^"]*"',
    re.IGNORECASE,
)

# Any xTaskCreate in wifi or network subsystems (must use timer work items)
BANNED_TASK_SUBSYSTEMS = ("wifi", "network")


def run(context: dict, config: dict | None = None) -> list[dict]:
    findings = []
    ws_root = Path(context.get("workspace_root", "."))
    src_dir = ws_root / "wink-micro-os" / "frameworks" / "esp_idf" / "src"

    if not src_dir.exists():
        return findings

    changed_files = context.get("changed_files", [])
    target_files = []
    if changed_files:
        for f in changed_files:
            p = ws_root / f
            if p.suffix == ".c" and src_dir in p.parents:
                target_files.append(p)
    else:
        target_files = list(src_dir.rglob("*.c"))

    for c_file in target_files:
        # Exclude core FreeRTOS implementation
        if "freertos" in c_file.parts:
            continue

        try:
            with open(c_file, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()
                lines = content.splitlines()
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

        # Check banned task names
        for line_no, line in enumerate(lines, start=1):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
                continue

            m = BANNED_TASK_NAMES.search(line)
            if m:
                findings.append({
                    "rule_id": RULE_ID,
                    "severity": "error",
                    "entry_id": None,
                    "display_id": None,
                    "config_id": None,
                    "file_path": str(c_file.relative_to(ws_root)),
                    "message": (
                        f"Line {line_no}: Banned temporary delay task creation: '{m.group(0)}'. "
                        f"Must use esp_freertos_timer_post_work_item() with generation token."
                    ),
                })

        # Check any xTaskCreate in wifi or network subsystems
        subsystem = None
        for sub in BANNED_TASK_SUBSYSTEMS:
            if sub in c_file.parts:
                subsystem = sub
                break

        if subsystem and "xTaskCreate" in content:
            for line_no, line in enumerate(lines, start=1):
                stripped = line.strip()
                if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
                    continue
                if "xTaskCreate" in line:
                    findings.append({
                        "rule_id": RULE_ID,
                        "severity": "error",
                        "entry_id": None,
                        "display_id": None,
                        "config_id": None,
                        "file_path": str(c_file.relative_to(ws_root)),
                        "message": (
                            f"Line {line_no}: Calling xTaskCreate() in '{subsystem}' subsystem is prohibited. "
                            f"Network and Wi-Fi state machines must use Timer Daemon delayed work items."
                        ),
                    })

    return findings
