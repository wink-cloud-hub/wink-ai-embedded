#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
WinkMicroOS Arduino Isolation Linter (ADR-0035)
Ensures that C++ Arduino headers or symbols do not bleed into the C-based core kernel (pal, dal, trace, runtime, targets).
"""

import os
import sys
import re

# Directories that must remain strictly isolated from Arduino Core C++ dependencies
ISOLATED_DIRS = ['pal', 'dal', 'trace', 'runtime', 'targets']

# Keywords or includes that indicate isolation leakage
FORBIDDEN_PATTERNS = [
    (re.compile(r'#include\s*[<"](?:api/)?Arduino(?:API)?\.h[>"]'), 'Forbidden Arduino.h or ArduinoAPI.h include'),
    (re.compile(r'#include\s*[<"](?:api/)?Hardware(?:Serial|I2C|SPI|CAN)\.h[>"]'), 'Forbidden Hardware bus headers'),
    (re.compile(r'\bTwoWire\b'), 'Forbidden TwoWire type reference'),
    (re.compile(r'\bHardwareSerial\b'), 'Forbidden HardwareSerial type reference'),
    (re.compile(r'\bHardwareSPI\b'), 'Forbidden HardwareSPI type reference'),
]

def scan_file(filepath):
    violations = []
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            for line_no, line in enumerate(f, 1):
                # Ignore comments
                stripped = line.strip()
                if stripped.startswith('//') or stripped.startswith('*') or stripped.startswith('/*'):
                    continue
                for pattern, desc in FORBIDDEN_PATTERNS:
                    if pattern.search(line):
                        violations.append((line_no, line.strip(), desc))
    except Exception as e:
        print(f"Error reading {filepath}: {e}")
    return violations

def main():
    root_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    print(f"Scanning kernel core targets under {root_dir} for Arduino leaks...")

    total_violations = 0
    for folder in ISOLATED_DIRS:
        folder_path = os.path.join(root_dir, folder)
        if not os.path.exists(folder_path):
            continue

        for root, _, files in os.walk(folder_path):
            for file in files:
                if not (file.endswith('.c') or file.endswith('.h')):
                    continue
                filepath = os.path.join(root, file)
                violations = scan_file(filepath)
                if violations:
                    rel_path = os.path.relpath(filepath, root_dir)
                    print(f"\n[FAIL] Isolation violation in {rel_path}:")
                    for line_no, line_content, desc in violations:
                        print(f"  Line {line_no}: {line_content}  <-- {desc}")
                        total_violations += 1

    if total_violations > 0:
        print(f"\n[ERROR] Found {total_violations} isolation violations! ADR-0035 forbids C++ Arduino dependencies in the core kernel.")
        sys.exit(1)

    print("[PASS] Core kernel is strictly isolated from Arduino C++ references.")
    sys.exit(0)

if __name__ == '__main__':
    main()
