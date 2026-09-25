#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Line-coverage threshold gate for the ESP-IDF facade (M3 Task M3-2, T-011).

Usage: check_coverage.py <coverage_file> <min_line_percent>
Exit code: 0 = PASS, 1 = below threshold or missing line data.
"""
import sys


def main() -> int:
    if len(sys.argv) < 3:
        print("Usage: check_coverage.py <coverage_file> <min_line_percent>")
        return 1

    info_file = sys.argv[1]
    threshold = float(sys.argv[2])

    lines_found = 0
    lines_hit = 0
    branches_found = 0
    branches_hit = 0

    with open(info_file, "r", encoding="utf-8") as f:
        for line in f:
            if line.startswith("LF:"):
                lines_found += int(line.strip().split(":")[1])
            elif line.startswith("LH:"):
                lines_hit += int(line.strip().split(":")[1])
            elif line.startswith("BRF:"):
                branches_found += int(line.strip().split(":")[1])
            elif line.startswith("BRH:"):
                branches_hit += int(line.strip().split(":")[1])

    if lines_found == 0:
        print("Error: No line data found in coverage file!")
        return 1

    percentage = (lines_hit / lines_found) * 100.0
    print(f"ESP-IDF Core Facade Line Coverage: {percentage:.2f}% (Hit {lines_hit}/{lines_found})")
    print(f"Required Threshold: {threshold:.2f}%")

    if branches_found > 0:
        br_percentage = (branches_hit / branches_found) * 100.0
        print(f"ESP-IDF Core Facade Branch Coverage: {br_percentage:.2f}% "
              f"(Hit {branches_hit}/{branches_found}) [Info Only]")

    if percentage < threshold:
        print(f"FAILED: Coverage {percentage:.2f}% is below threshold {threshold:.2f}%!")
        return 1

    print("SUCCESS: Coverage threshold gate PASSED!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
