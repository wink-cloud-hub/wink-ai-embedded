#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lint_i18n_glossary.py

Linter to enforce terminology standards from docs/i18n/glossary.yaml.
Checks:
1. docs/en/ markdown files for forbidden mistranslations.
2. C/C++ header Doxygen comments for forbidden mistranslations.
"""

import os
import re
import sys
import argparse

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DOCS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
WORKSPACE_DIR = os.path.abspath(os.path.join(DOCS_DIR, ".."))
GLOSSARY_PATH = os.path.join(DOCS_DIR, "i18n", "glossary.yaml")

def parse_yaml_simple(filepath):
    """Simple YAML parser for glossary.yaml forbidden patterns without heavy dependencies."""
    forbidden = []
    if not os.path.exists(filepath):
        return forbidden

    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    # Match forbidden section
    # - pattern: "..."
    #   reason: "..."
    forbidden_matches = re.findall(
        r'-\s+pattern:\s*["\'](.*?)["\']\s*\n\s+reason:\s*["\'](.*?)["\']',
        content,
        re.MULTILINE
    )
    for pat, reason in forbidden_matches:
        forbidden.append({"pattern": pat, "reason": reason})

    return forbidden

def scan_files(forbidden_rules):
    violations = []
    scan_targets = []

    # 1. English documentation
    en_docs_dir = os.path.join(DOCS_DIR, "en")
    if os.path.exists(en_docs_dir):
        for root, _, files in os.walk(en_docs_dir):
            for file in files:
                if file.endswith(".md"):
                    scan_targets.append(os.path.join(root, file))

    # 2. C SDK header files
    c_dirs = [
        os.path.join(WORKSPACE_DIR, "wink-micro-os"),
        os.path.join(WORKSPACE_DIR, "esp32_firmware"),
    ]
    for c_dir in c_dirs:
        if os.path.exists(c_dir):
            for root, _, files in os.walk(c_dir):
                for file in files:
                    if file.endswith((".h", ".hpp", ".c", ".cpp")):
                        scan_targets.append(os.path.join(root, file))

    for filepath in scan_targets:
        rel_path = os.path.relpath(filepath, WORKSPACE_DIR).replace("\\", "/")
        try:
            with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
        except Exception:
            continue

        for line_idx, line in enumerate(lines, 1):
            for rule in forbidden_rules:
                pat = rule["pattern"]
                reason = rule["reason"]
                if re.search(pat, line, re.IGNORECASE):
                    violations.append({
                        "file": rel_path,
                        "line": line_idx,
                        "content": line.strip(),
                        "pattern": pat,
                        "reason": reason
                    })

    return violations

def main():
    parser = argparse.ArgumentParser(description="WinkMicroOS i18n Terminology Linter")
    parser.add_argument("--exit-code", action="store_true", help="Exit with non-zero code on violations")
    args = parser.parse_args()

    print("=" * 80)
    print(" 🔍 WinkMicroOS i18n Terminology Linter")
    print("=" * 80)

    forbidden_rules = parse_yaml_simple(GLOSSARY_PATH)
    print(f" Loaded {len(forbidden_rules)} forbidden pattern rules from glossary.yaml")

    violations = scan_files(forbidden_rules)

    if not violations:
        print("\n [✓] 100% Terminology Check Passed! No forbidden patterns found.\n")
        print("=" * 80)
        return 0

    print(f"\n [!] Found {len(violations)} terminology violations:\n")
    for v in violations:
        print(f"  • {v['file']}:{v['line']}")
        print(f"    Content: {v['content']}")
        print(f"    Rule: {v['reason']}")
        print("-" * 80)

    print("=" * 80)
    if args.exit_code:
        sys.exit(1)
    return 1

if __name__ == "__main__":
    main()
