#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
check_isr_iram_flag.py - Validates that all hardware interrupt registrations in ESP32 specify ESP_INTR_FLAG_IRAM.
"""
import sys
import re
from pathlib import Path

def main():
    repo_root = Path(__file__).resolve().parent.parent
    esp32_dir = repo_root / "targets" / "esp32"

    if not esp32_dir.exists():
        print(f"Error: Missing {esp32_dir}")
        return 1

    violations = []
    for c_file in esp32_dir.glob("*.c"):
        content = c_file.read_text(encoding="utf-8", errors="ignore")
        # Check esp_intr_alloc calls
        for match in re.finditer(r"esp_intr_alloc\s*\([^,]+,\s*([^,]+),", content):
            flags_arg = match.group(1).strip()
            # If flags_arg is a variable (e.g. 'flags'), check if ESP_INTR_FLAG_IRAM is in the file or assigned to flags
            if "ESP_INTR_FLAG_IRAM" not in flags_arg and flags_arg != "0":
                if not re.search(r"ESP_INTR_FLAG_IRAM", content):
                    violations.append((c_file, f"esp_intr_alloc without ESP_INTR_FLAG_IRAM: {flags_arg}"))

    if violations:
        print(f"FAILED: Found {len(violations)} ISR IRAM flag violations:")
        for f, msg in violations:
            print(f"  {f.name}: {msg}")
        return 1

    print("ISR IRAM flag check: PASSED.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
