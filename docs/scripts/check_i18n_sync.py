#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_i18n_sync.py

Inspects synchronization and translation coverage between docs/zh/ and docs/en/ SSOT repositories.
Parses `<!-- i18n-meta ... -->` metadata blocks in docs/en/ files.
"""

import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DOCS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
WORKSPACE_DIR = os.path.abspath(os.path.join(DOCS_DIR, ".."))

ZH_DIR = os.path.join(DOCS_DIR, "zh")
EN_DIR = os.path.join(DOCS_DIR, "en")

META_PATTERN = re.compile(r'<!--\s*i18n-meta\s*(.*?)\s*-->', re.DOTALL | re.IGNORECASE)

def parse_i18n_meta(filepath):
    """Extracts metadata dictionary from frontmatter comments."""
    meta = {}
    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
        match = META_PATTERN.search(content)
        if match:
            meta_str = match.group(1)
            for line in meta_str.splitlines():
                if ":" in line:
                    k, v = line.split(":", 1)
                    meta[k.strip().lower()] = v.strip()
    except Exception:
        pass
    return meta

def check_ssot_sync():
    print("=" * 80)
    print(" 🌐 WinkMicroOS SSOT i18n Synchronization Inspector")
    print("=" * 80)

    # 1. Collect all Chinese SSOT files (docs/zh/design/ and docs/zh/tech-designs/)
    zh_files = []
    for sub in ["design", "tech-designs", "product"]:
        target_dir = os.path.join(ZH_DIR, sub)
        if os.path.exists(target_dir):
            for root, _, files in os.walk(target_dir):
                for f in files:
                    if f.endswith(".md"):
                        rel_path = os.path.relpath(os.path.join(root, f), ZH_DIR).replace("\\", "/")
                        zh_files.append(rel_path)

    # If docs/zh is not yet migrated, fallback to current docs/ paths
    if not zh_files:
        for sub in ["design", "tech-designs", "product"]:
            target_dir = os.path.join(DOCS_DIR, sub)
            if os.path.exists(target_dir):
                for root, _, files in os.walk(target_dir):
                    for f in files:
                        if f.endswith(".md"):
                            rel_path = os.path.relpath(os.path.join(root, f), DOCS_DIR).replace("\\", "/")
                            zh_files.append(rel_path)

    total_zh = len(zh_files)
    translated_files = []
    missing_files = []

    for rel_path in sorted(zh_files):
        en_path = os.path.join(EN_DIR, rel_path)
        if os.path.exists(en_path):
            meta = parse_i18n_meta(en_path)
            sync_status = meta.get("sync-status", "unmarked")
            trans_date = meta.get("translated", "N/A")
            translated_files.append({
                "rel_path": rel_path,
                "status": sync_status,
                "date": trans_date
            })
        else:
            missing_files.append(rel_path)

    trans_count = len(translated_files)
    coverage_pct = (trans_count / total_zh * 100.0) if total_zh > 0 else 0.0

    print(f"\n [📊] Total SSOT Documents (ZH) : {total_zh}")
    print(f" [✓] Translated Documents (EN)  : {trans_count}")
    print(f" [📈] Translation Coverage Rate : {coverage_pct:.1f}%\n")

    if translated_files:
        print(f"--- Translated SSOT Files ({trans_count}) ---")
        for item in translated_files:
            print(f"  • [EN] {item['rel_path']} (Status: {item['status']}, Date: {item['date']})")

    if missing_files:
        print(f"\n--- Pending Translation ({len(missing_files)}) ---")
        for p in missing_files[:20]:
            print(f"  • [Pending] {p}")
        if len(missing_files) > 20:
            print(f"  ... and {len(missing_files) - 20} more files.")

    print("\n" + "=" * 80)
    return 0

if __name__ == "__main__":
    sys.exit(check_ssot_sync())
