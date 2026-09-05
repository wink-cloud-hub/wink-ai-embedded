#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Check SSOT Back-write Synchronization Status across ADRs and Tech-Designs.
"""
import os
import re
import sys

DOCS_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DECISIONS_DIR = os.path.join(DOCS_DIR, "decisions")


def scan_adrs():
    print("=" * 60)
    print(" Wink-AI SSOT Back-write Synchronization Inspector")
    print("=" * 60)

    pending_list = []
    completed_list = []

    for root, _, files in os.walk(DECISIONS_DIR):
        for f in files:
            if f.endswith(".md") and f != "README.md":
                filepath = os.path.join(root, f)
                relpath = os.path.relpath(filepath, DOCS_DIR)
                with open(filepath, "r", encoding="utf-8", errors="ignore") as file:
                    content = file.read()
                    status_match = re.search(r"\|\s*SSOT\s*回写状态\s*\|\s*(.*?)\s*\|", content, re.IGNORECASE)
                    target_match = re.search(r"\|\s*回写\s*SSOT\s*目标文档\s*\|\s*(.*?)\s*\|", content, re.IGNORECASE)

                    status = status_match.group(1).strip() if status_match else "N/A"
                    target = target_match.group(1).strip() if target_match else "Unspecified"

                    if "Pending" in status:
                        pending_list.append((relpath, target))
                    elif "Completed" in status or "Done" in status:
                        completed_list.append((relpath, target))

    print(f"\n[✓] Completed SSOT Back-writes: {len(completed_list)}")
    for relpath, target in completed_list:
        print(f"  • {relpath} -> {target}")

    print(f"\n[!] Pending SSOT Back-writes: {len(pending_list)}")
    for relpath, target in pending_list:
        print(f"  • {relpath} -> {target}")

    print("\n" + "=" * 60)
    return 0


if __name__ == "__main__":
    sys.exit(scan_adrs())
