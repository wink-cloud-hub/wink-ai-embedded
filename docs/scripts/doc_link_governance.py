#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import re
import sys
import argparse

# Force stdout/stderr to use UTF-8 on Windows
if sys.platform.startswith('win'):
    try:
        if hasattr(sys.stdout, 'reconfigure'):
            sys.stdout.reconfigure(encoding='utf-8', errors='replace')
            sys.stderr.reconfigure(encoding='utf-8', errors='replace')
    except Exception:
        pass

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DOCS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
WORKSPACE_DIR = os.path.abspath(os.path.join(DOCS_DIR, ".."))

# Regex patterns
# Pattern A: Standard Markdown links: [label](target.md#anchor)
MD_LINK_PATTERN = re.compile(r'\[([^\]]+)\]\(([^)#\s]+\.md)(#[^)]+)?\)')

# Pattern B: Workspace text path references: docs/path/to/file.md (not preceded by '(')
WS_PATH_PATTERN = re.compile(r'(?<!\()docs/([a-zA-Z0-9_\-\/]+\.md)')

def build_global_file_map():
    """Maps filename.md -> absolute path."""
    file_map = {}
    for root, dirs, files in os.walk(DOCS_DIR):
        for file in files:
            if file.endswith(".md"):
                file_map[file] = os.path.join(root, file)
    return file_map

def check_all_links(file_map):
    diagnostics = []

    for file_name, file_path in sorted(file_map.items()):
        try:
            with open(file_path, "r", encoding="utf-8-sig") as f:
                lines = f.readlines()
        except Exception as e:
            print(f"Warning: Failed to read {file_path}: {e}", file=sys.stderr)
            continue

        dir_of_file = os.path.dirname(file_path)

        for line_num, line in enumerate(lines, start=1):
            # Check Pattern A: Markdown links
            for match in MD_LINK_PATTERN.finditer(line):
                label = match.group(1)
                target = match.group(2)
                anchor = match.group(3) or ""

                if target.startswith(("http://", "https://", "mailto:", "#")):
                    continue

                abs_target = os.path.normpath(os.path.join(dir_of_file, target))
                if not os.path.exists(abs_target):
                    target_filename = os.path.basename(target)
                    new_abs = file_map.get(target_filename)
                    if new_abs:
                        new_rel = os.path.relpath(new_abs, dir_of_file).replace("\\", "/")
                        diagnostics.append({
                            "file": file_path,
                            "line_num": line_num,
                            "type": "MD_LINK",
                            "original": match.group(0),
                            "old_target": target,
                            "new_target": new_rel + anchor,
                            "replacement": f"[{label}]({new_rel}{anchor})"
                        })
                    else:
                        diagnostics.append({
                            "file": file_path,
                            "line_num": line_num,
                            "type": "MISSING_MD_LINK",
                            "original": match.group(0),
                            "old_target": target,
                            "new_target": None,
                            "replacement": None
                        })

            # Check Pattern B: Workspace text paths (docs/.../*.md)
            for match in WS_PATH_PATTERN.finditer(line):
                rel_ws_path = "docs/" + match.group(1)
                abs_ws_path = os.path.normpath(os.path.join(WORKSPACE_DIR, rel_ws_path))

                if not os.path.exists(abs_ws_path):
                    target_filename = os.path.basename(rel_ws_path)
                    new_abs = file_map.get(target_filename)
                    if new_abs:
                        new_ws_rel = os.path.relpath(new_abs, WORKSPACE_DIR).replace("\\", "/")
                        diagnostics.append({
                            "file": file_path,
                            "line_num": line_num,
                            "type": "WS_PATH",
                            "original": match.group(0),
                            "old_target": rel_ws_path,
                            "new_target": new_ws_rel,
                            "replacement": new_ws_rel
                        })
                    else:
                        diagnostics.append({
                            "file": file_path,
                            "line_num": line_num,
                            "type": "MISSING_WS_PATH",
                            "original": match.group(0),
                            "old_target": rel_ws_path,
                            "new_target": None,
                            "replacement": None
                        })

    return diagnostics

def print_diagnostics_report(diagnostics):
    print("\n" + "=" * 90)
    print(f" 🔍 全局文档链接与路径检查报告 (共发现 {len(diagnostics)} 项隐蔽或断裂路径)")
    print("=" * 90)

    if not diagnostics:
        print("  🎉 太棒了！全站所有 Markdown 相对链接与 text/code 路径引用 100% 完美有效！")
        print("=" * 90 + "\n")
        return

    fixable = [d for d in diagnostics if d["new_target"] is not None]
    missing = [d for d in diagnostics if d["new_target"] is None]

    print(f"  • 可自动治愈路径: {len(fixable)} 项")
    print(f"  • 彻底缺失丢失文件: {len(missing)} 项")
    print("-" * 90)

    for item in diagnostics[:50]: # Print first 50
        rel_file = os.path.relpath(item["file"], WORKSPACE_DIR).replace("\\", "/")
        status_flag = "🔧 可修复" if item["new_target"] else "❌ 丢失"
        print(f"  [{status_flag}] {rel_file}:{item['line_num']}")
        print(f"    原始引用: {item['original']}")
        if item["new_target"]:
            print(f"    修正建议: {item['replacement']}")
        print("-" * 90)

    if len(diagnostics) > 50:
        print(f"  ... 还有 {len(diagnostics) - 50} 项未列出。")

    print("=" * 90 + "\n")

def fix_all_links(file_map, diagnostics, apply_changes=False):
    if not diagnostics:
        print("无需修复，所有路径均有效！")
        return

    fixable = [d for d in diagnostics if d["new_target"] is not None]
    if not fixable:
        print("无法自动修复，未找到可修正的目标路径。")
        return

    if not apply_changes:
        print("\n" + "=" * 90)
        print(f" 💡 【Dry-Run 模式】即将自动修复 {len(fixable)} 处坏死链接与文本路径：")
        print("=" * 90)
        for item in fixable[:30]:
            rel_file = os.path.relpath(item["file"], WORKSPACE_DIR).replace("\\", "/")
            print(f"  • [{rel_file}:{item['line_num']}] {item['original']}  ===>  {item['replacement']}")
        print("=" * 90)
        print("  提示: 使用 '--apply' 参数可以直接写回磁盘完成修复。")
        print("=" * 90 + "\n")
        return

    # Group diagnostics by file
    file_diag_map = {}
    for d in fixable:
        file_diag_map.setdefault(d["file"], []).append(d)

    fixed_files_count = 0
    fixed_items_count = 0

    for file_path, items in file_diag_map.items():
        try:
            with open(file_path, "r", encoding="utf-8-sig") as f:
                content = f.read()

            new_content = content
            for item in items:
                old_str = item["original"]
                new_str = item["replacement"]
                if old_str in new_content:
                    new_content = new_content.replace(old_str, new_str)
                    fixed_items_count += 1

            if new_content != content:
                with open(file_path, "w", encoding="utf-8") as f:
                    f.write(new_content)
                fixed_files_count += 1
        except Exception as e:
            print(f"Error fixing {file_path}: {e}", file=sys.stderr)

    print("\n" + "=" * 90)
    print(f" ✅ 修复完成！成功更新 {fixed_files_count} 个文件中的 {fixed_items_count} 处路径引用！")
    print("=" * 90 + "\n")

def main():
    parser = argparse.ArgumentParser(description="WinkMicroOS 全局 Markdown 链接与路径引用治理工具")
    subparsers = parser.add_subparsers(dest="command", help="子命令: check 或 fix")

    # Check command
    parser_check = subparsers.add_parser("check", help="全量检查 Markdown 相对链接与代码块中的路径有效性")

    # Fix command
    parser_fix = subparsers.add_parser("fix", help="全量替换与治愈坏死路径")
    parser_fix.add_argument("--dry-run", action="store_true", help="预览替换方案（不修改文件）")
    parser_fix.add_argument("--apply", action="store_true", help="直接写回磁盘完成自动修复")

    args = parser.parse_args()

    file_map = build_global_file_map()
    diagnostics = check_all_links(file_map)

    if args.command == "check":
        print_diagnostics_report(diagnostics)
    elif args.command == "fix":
        if not args.dry_run and not args.apply:
            print("请指定 '--dry-run' 预览，或 '--apply' 执行写回。")
            sys.exit(1)
        fix_all_links(file_map, diagnostics, apply_changes=args.apply)
        if args.apply:
            # Re-check after fix
            print("正在重新核验修正结果...")
            post_diags = check_all_links(build_global_file_map())
            print_diagnostics_report(post_diags)
    else:
        # Default behavior: run check
        print_diagnostics_report(diagnostics)

if __name__ == "__main__":
    main()
