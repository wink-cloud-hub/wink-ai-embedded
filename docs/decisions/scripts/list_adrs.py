#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import re
import sys
import argparse

# Try to force stdout/stderr to use UTF-8 with fallback replacement to prevent console crashes on Windows
if sys.platform.startswith('win'):
    try:
        if hasattr(sys.stdout, 'reconfigure'):
            sys.stdout.reconfigure(encoding='utf-8', errors='replace')
            sys.stderr.reconfigure(encoding='utf-8', errors='replace')
        else:
            import codecs
            if hasattr(sys.stdout, 'buffer'):
                sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer, errors='replace')
            if hasattr(sys.stderr, 'buffer'):
                sys.stderr = codecs.getwriter('utf-8')(sys.stderr.buffer, errors='replace')
    except Exception:
        pass

# Define directories relative to this script
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DECISIONS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

def get_adr_status():
    adrs = []
    # Match status line: | 状态 | **XXXX** | or | Status | **XXXX** |
    status_pattern = re.compile(r"\|\s*(状态|Status)\s*\|\s*\*\*(?P<status>[^*]+)\*\*\s*\|", re.IGNORECASE)
    # Match title line: # ADR-XXXX：Title or # ADR-XXXX: Title
    title_pattern = re.compile(r"^#\s*ADR-(?P<id>\d{4})[：:]\s*(?P<title>.+)$")

    if not os.path.exists(DECISIONS_DIR):
        print(f"Error: Decisions directory not found at {DECISIONS_DIR}", file=sys.stderr)
        return adrs

    for root, dirs, files in os.walk(DECISIONS_DIR):
        for file in sorted(files):
            if not file.endswith(".md") or file == "README.md":
                continue
            
            file_path = os.path.join(root, file)
            rel_path = os.path.relpath(file_path, DECISIONS_DIR).replace("\\", "/")
            status, title, adr_id = "Unknown", rel_path, ""
        
            try:
                with open(file_path, "r", encoding="utf-8-sig") as f:
                    for line in f:
                        title_match = title_pattern.match(line)
                        if title_match:
                            title = title_match.group("title").strip()
                            adr_id = title_match.group("id").strip()
                        
                        status_match = status_pattern.search(line)
                        if status_match:
                            status = status_match.group("status").strip()
            except Exception as e:
                print(f"Warning: Failed to read {file}: {e}", file=sys.stderr)
                continue
            
            domain = rel_path.split("/")[0] if "/" in rel_path else "core"
            if adr_id:
                adrs.append({
                    "id": adr_id,
                    "title": title,
                    "status": status,
                    "file": file,
                    "domain": domain,
                    "path": file_path
                })
            
    return adrs

def main():
    parser = argparse.ArgumentParser(description="Scan and list Architecture Decision Records (ADRs) and their status.")
    parser.add_argument("--all", "-a", action="store_true", help="List all ADRs, including Accepted and Rejected.")
    parser.add_argument("--status", "-s", type=str, help="Filter ADRs by specific status (e.g., Proposed, Accepted, Rejected).")
    parser.add_argument("--domain", "-d", type=str, help="Filter ADRs by domain (core, tools, frontend, unisim).")
    args = parser.parse_args()

    adrs = get_adr_status()
    if not adrs:
        print("No ADR files found.")
        sys.exit(0)

    # Filter based on args
    filtered = adrs
    if args.domain:
        filtered = [a for a in filtered if args.domain.lower() in a["domain"].lower()]

    if args.status:
        filter_keyword = args.status.lower()
        filtered = [a for a in filtered if filter_keyword in a["status"].lower()]
        title_text = f"ADRs matching status '{args.status}'"
        is_detailed_view = True
    elif args.all:
        title_text = "所有 ADR 状态概览"
        is_detailed_view = False
    else:
        # Default: Only Proposed / Pending
        filtered = [a for a in filtered if any(keyword in a["status"] for keyword in ["Proposed", "提议", "Pending", "待定"])]
        title_text = f"尚未决议的 ADR (Proposed / Pending) - 共 {len(filtered)} 项"
        is_detailed_view = True

    # Output results
    if is_detailed_view:
        print("\n" + "=" * 70)
        print(f" [*] {title_text}")
        print("=" * 70)
        if filtered:
            for a in filtered:
                clean_status = a["status"].split("（")[0].split("(")[0].strip()
                print(f"  • [ADR-{a['id']}] [{a['domain']}] {a['title']}")
                print(f"    文件: docs/decisions/{a['domain']}/{a['file']}")
                print(f"    状态: {a['status']}")
                print("-" * 70)
        else:
            print("  没有找到匹配的 ADR。")
            print("=" * 70)
            print("  提示: 使用 '-a' 或 '--all' 参数可以查看所有决策（包括已采纳/已拒绝的决策）。")
            print("=" * 70)
    else:
        # Table view for overview
        print("\n" + "=" * 70)
        print(f" [=] {title_text}")
        print("=" * 70)
        print(f"  {'编号':<6} | {'领域':<8} | {'决策标题':<30} | {'状态'}")
        print("  " + "-" * 66)
        for a in filtered:
            title_disp = a["title"]
            if len(title_disp) > 20:
                title_disp = title_disp[:18] + "..."
            
            # Calculate visual length for alignment (approximate for Chinese characters)
            visual_len = sum(2 if ord(c) > 127 else 1 for c in title_disp)
            padding = max(0, 30 - visual_len)
            
            print(f"  ADR-{a['id']} | {a['domain']:<8} | {title_disp}{' ' * padding} | {a['status']}")
        print("=" * 70 + "\n")

if __name__ == "__main__":
    main()
