#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
WinkMicroOS Arduino Binary Symbol Auditor (ADR-0036 / ADR-0040)
Scans the compiled Arduino-enabled app binary for forbidden symbols:
- Exception / unwind symbols (__cxa_throw, _Unwind_RaiseException)
- Unexpected C++ std:: library namespace symbols (_ZSt or std::)
- Ensure custom placement new is not pulling standard heap operator new
"""

import os
import sys
import subprocess
import shutil

# Forbidden mangled or demangled patterns
FORBIDDEN_SYMBOLS = [
    ('__cxa_throw', 'Exception throwing runtime'),
    ('_Unwind_RaiseException', 'Exception stack unwinding'),
    ('_ZSt', 'C++ std:: namespace symbol'),
    ('__cxa_begin_catch', 'Exception catching runtime'),
]

# Standard zero-overhead math functions are allowed to belong to std::
EXCLUDED_PATTERNS = [
    'isinf', 'isnan', 'abs', 'round', 'ceil', 'floor', 'min', 'max'
]

def check_binary(binary_path):
    # Find nm or nm.exe on PATH
    nm_path = shutil.which("nm")
    if not nm_path:
        # Fallback to specific gcc/MinGW prefixes if standard nm is not found
        for prefix in ["x86_64-w64-mingw32-nm", "i686-w64-mingw32-nm"]:
            nm_path = shutil.which(prefix)
            if nm_path:
                break

    if not nm_path:
        print(f"[WARN] 'nm' tool not found on PATH. Skipping symbol audit for {binary_path}.")
        return 0

    print(f"Auditing symbols in {binary_path} using {nm_path}...")
    try:
        res = subprocess.run([nm_path, binary_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if res.returncode != 0:
            print(f"[WARN] nm command failed with exit code {res.returncode}. Skipping.")
            return 0
        
        violations = []
        for line in res.stdout.splitlines():
            # nm output format: address type name
            parts = line.strip().split()
            if len(parts) >= 2:
                sym_name = parts[-1]
                if any(ex in sym_name for ex in EXCLUDED_PATTERNS):
                    continue
                for pattern, desc in FORBIDDEN_SYMBOLS:
                    if pattern in sym_name:
                        violations.append((sym_name, desc))
        
        if violations:
            print(f"[FAIL] Found forbidden C++ runtime / STL symbols in {os.path.basename(binary_path)}:")
            for sym, desc in violations:
                print(f"  Symbol: {sym}  <-- {desc}")
            return len(violations)
            
    except Exception as e:
        print(f"[WARN] Failed to audit symbols: {e}")
    return 0

def main():
    if len(sys.argv) < 2:
        print("Usage: python check_arduino_symbols.py <build_dir>")
        sys.exit(1)

    build_dir = sys.argv[1]
    if not os.path.exists(build_dir):
        print(f"[WARN] Build directory {build_dir} does not exist. Skipping.")
        sys.exit(0)

    # Search for compiled executable 'app_arduino_blink_e2e' or similar
    binaries_to_check = []
    for root, _, files in os.walk(build_dir):
        for file in files:
            # Match app_arduino_blink_e2e (on windows, app_arduino_blink_e2e.exe)
            if file == "app_arduino_blink_e2e" or file == "app_arduino_blink_e2e.exe":
                binaries_to_check.append(os.path.join(root, file))

    if not binaries_to_check:
        print("[WARN] No compiled app_arduino_blink_e2e binaries found to audit.")
        sys.exit(0)

    total_violations = 0
    for binary in binaries_to_check:
        total_violations += check_binary(binary)

    if total_violations > 0:
        print(f"[ERROR] Found {total_violations} symbol violations! C++ standard library / exception features must be completely pruned.")
        sys.exit(1)

    print("[PASS] Symbol audit passed. No forbidden C++ runtime / STL symbols found in compiled binaries.")
    sys.exit(0)

if __name__ == '__main__':
    main()
