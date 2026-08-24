#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
check_isr_no_log.py - Enforces zero-logging and zero-heap-allocation in ISR/IRAM functions.
"""
import sys
import re
from pathlib import Path

FORBIDDEN_CALLS = [
    r"\bmalloc\s*\(",
    r"\bcalloc\s*\(",
    r"\brealloc\s*\(",
    r"\bfree\s*\(",
    r"\bprintf\s*\(",
    r"\bvsnprintf\s*\(",
    r"\bsprintf\s*\(",
    r"\bsnprintf\s*\(",
    r"\bLOG_E\s*\(",
    r"\bLOG_W\s*\(",
    r"\bLOG_I\s*\(",
    r"\bLOG_D\s*\(",
    r"\bpal_os_sleep_ms\s*\(",
    r"\bvTaskDelay\s*\(",
]

def strip_comments_and_strings(code: str) -> str:
    # Strip block comments /* ... */
    code = re.sub(r"/\*.*?\*/", "", code, flags=re.DOTALL)
    # Strip line comments // ...
    code = re.sub(r"//.*", "", code)
    # Strip string literals "..."
    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
    return code

def scan_file(file_path: Path) -> list:
    violations = []
    content = file_path.read_text(encoding="utf-8", errors="ignore")

    # Match function definitions marked PAL_ISR, IRAM_ATTR, PAL_IRAM_TEXT, or PAL_DEFINE_ISR
    isr_pattern = re.compile(
        r"(?:(?:static\s+)?(?:void|bool|uint32_t|int)\s+(?:PAL_ISR|IRAM_ATTR|PAL_IRAM_TEXT)\s+(\w+)\s*\([^)]*\)\s*\{([^}]+)\}|"
        r"PAL_DEFINE_ISR\s*\(\s*(\w+)[^)]*\)\s*\{([^}]+)\})",
        re.MULTILINE | re.DOTALL
    )

    for match in isr_pattern.finditer(content):
        fn_name = match.group(1) or match.group(3)
        fn_body = match.group(2) or match.group(4)
        clean_body = strip_comments_and_strings(fn_body)

        for pattern in FORBIDDEN_CALLS:
            if re.search(pattern, clean_body):
                violations.append((file_path, fn_name, pattern))

    return violations

def main():
    repo_root = Path(__file__).resolve().parent.parent
    search_dirs = [repo_root / "pal", repo_root / "targets", repo_root / "dal", repo_root / "runtime"]

    all_violations = []
    for d in search_dirs:
        if d.exists():
            for c_file in d.rglob("*.c"):
                # Skip log driver implementation itself which defines fallback handlers
                if "pal_log_" in c_file.name:
                    continue
                violations = scan_file(c_file)
                all_violations.extend(violations)

    if all_violations:
        print(f"FAILED: Found {len(all_violations)} violations in ISR/IRAM code:")
        for path, fn, pattern in all_violations:
            clean_pattern = pattern.replace(r"\b", "").replace(r"\s*\(", "()")
            print(f"  {path.name}: {fn}() calls {clean_pattern}")
        return 1

    print("ISR zero-allocation & zero-log check: PASSED (0 violations).")
    return 0

if __name__ == "__main__":
    sys.exit(main())
