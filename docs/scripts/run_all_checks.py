#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
run_all_checks.py

One-click comprehensive test runner for WinkMicroOS documentation & i18n governance.
Executes:
1. verify_doc_contracts.py (Black-box insulation & contract integrity)
2. check_ssot_sync.py (ADR back-write SSOT status)
3. lint_i18n_glossary.py (Terminology compliance with glossary.yaml)
4. verify_i18n_alignment.py (1:1 Tree alignment & content structure check)
5. doc_link_governance.py (Global Markdown link health)
"""

import os
import sys
import subprocess
import argparse

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def main():
    parser = argparse.ArgumentParser(description="Run all documentation & i18n governance checks")
    parser.add_argument("--check-content", action="store_true", help="Enable deep Markdown content structural skeleton verification")
    parser.add_argument("--strict", action="store_true", help="Strict CI mode")
    args = parser.parse_args()

    print("=" * 90)
    print(" 🚀 Running WinkMicroOS Comprehensive Documentation & i18n Suite")
    print("=" * 90)

    checks = [
        ("Black-box & Contract Verification", ["verify_doc_contracts.py"]),
        ("ADR SSOT Back-write Synchronization", ["check_ssot_sync.py"]),
        ("i18n Terminology Linter", ["lint_i18n_glossary.py"]),
        ("i18n 1:1 Tree & Structural Alignment", ["verify_i18n_alignment.py"] + (["--check-content"] if args.check_content else []) + (["--strict"] if args.strict else [])),
        ("Global Markdown Link Governance", ["doc_link_governance.py"]),
    ]

    results = []

    for name, cmd in checks:
        script_name = cmd[0]
        script_path = os.path.join(SCRIPT_DIR, script_name)
        full_cmd = [sys.executable, script_path] + cmd[1:]
        print(f"\n▶ Running [{name}] ({' '.join(cmd)})...")
        print("-" * 90)
        try:
            ret = subprocess.run(full_cmd, cwd=os.path.dirname(SCRIPT_DIR))
            success = (ret.returncode == 0)
            results.append((name, success))
        except Exception as e:
            print(f"Error executing {cmd}: {e}")
            results.append((name, False))

    print("\n" + "=" * 90)
    print(" 📊 Suite Summary Results")
    print("=" * 90)
    all_passed = True
    for name, success in results:
        status_str = "✅ PASSED" if success else "❌ FAILED"
        print(f"  • {name:<45} : {status_str}")
        if not success:
            all_passed = False

    print("=" * 90)
    if all_passed:
        print(" 🎉 All documentation & i18n governance checks PASSED 100%!")
        return 0
    else:
        print(" ⚠️ Some checks failed. Please inspect the output above.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
