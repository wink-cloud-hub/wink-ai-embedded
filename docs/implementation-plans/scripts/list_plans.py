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
PLANS_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))

def get_visual_width(text):
    return sum(2 if ord(c) > 127 else 1 for c in text)

def pad_string(text, length, char=' '):
    width = get_visual_width(text)
    if width >= length:
        return text
    return text + char * (length - width)

def truncate_string(text, max_width):
    if get_visual_width(text) <= max_width:
        return text
    
    # Truncate and add ellipsis
    current_width = 0
    truncated = []
    for c in text:
        char_width = 2 if ord(c) > 127 else 1
        if current_width + char_width + 3 > max_width:
            break
        truncated.append(c)
        current_width += char_width
    return "".join(truncated) + "..."

def classify_status(status_str):
    status_str = status_str.strip()
    status_lower = status_str.lower()
    
    # Check for template pattern (contains options delimited by slash)
    if '/' in status_str and ('草稿' in status_str or '已完成' in status_str) and len(status_str) > 20:
        return "Template"
        
    if any(k in status_lower for k in ["已完成", "实施完成", "已执行", "执行完成", "结项", "done", "completed"]):
        return "Completed"
    elif any(k in status_lower for k in ["执行中", "进行中", "in progress", "in-progress", "doing"]):
        return "In Progress"
    elif any(k in status_lower for k in ["草稿", "草拟", "待确认", "待用户确认", "draft", "proposed"]):
        return "Draft"
    elif any(k in status_lower for k in ["暂停", "paused"]):
        return "Paused"
    elif any(k in status_lower for k in ["取消", "作废", "废弃", "canceled", "cancelled", "obsolete"]):
        return "Canceled"
    else:
        return "Unknown"

def get_status_emoji(category):
    mapping = {
        "Completed": "✅ 已完成",
        "In Progress": "🔄 执行中",
        "Draft": "📋 草拟",
        "Paused": "⏸️ 暂停",
        "Canceled": "❌ 取消",
        "Template": "📝 模板",
        "Unknown": "❓ 未知"
    }
    return mapping.get(category, "❓ 未知")

def parse_plans():
    plans = []
    
    # Regex patterns
    title_pattern = re.compile(r"^#\s+(?P<title>.+)$")
    
    # Match tables with | Key | Value |
    status_pat = re.compile(r"\|\s*(?:\*\*)?(计划状态|当前状态|状态)(?:\*\*)?\s*\|\s*(?P<val>[^|]+)\|")
    id_pat = re.compile(r"\|\s*(?:\*\*)?(计划编号)(?:\*\*)?\s*\|\s*(?P<val>[^|]+)\|")
    date_pat = re.compile(r"\|\s*(?:\*\*)?(创建日期)(?:\*\*)?\s*\|\s*(?P<val>[^|]+)\|")
    prio_pat = re.compile(r"\|\s*(?:\*\*)?(优先级)(?:\*\*)?\s*\|\s*(?P<val>[^|]+)\|")
    owner_pat = re.compile(r"\|\s*(?:\*\*)?(计划负责人)(?:\*\*)?\s*\|\s*(?P<val>[^|]+)\|")

    if not os.path.exists(PLANS_DIR):
        print(f"Error: Plans directory not found at {PLANS_DIR}", file=sys.stderr)
        return plans

    for root, dirs, files in os.walk(PLANS_DIR):
        for file in sorted(files):
            file_path = os.path.join(root, file)
            if not os.path.isfile(file_path) or not file.endswith(".md"):
                continue
            
            # Skip template file or README
            if file in ["00-IMPLEMENTATION-PLAN-TEMPLATE.md", "README.md"]:
                continue
                
            rel_path = os.path.relpath(file_path, PLANS_DIR).replace("\\", "/")
            domain = rel_path.split("/")[0] if "/" in rel_path else "core"

            title = file
            status = "Unknown"
            plan_id = "Unknown"
            date = "Unknown"
            priority = "Unknown"
            owner = "Unknown"
            
            try:
                with open(file_path, "r", encoding="utf-8-sig") as f:
                    found_title = False
                    found_status = False
                    found_id = False
                    found_date = False
                    found_prio = False
                    found_owner = False
                    for line in f:
                        # Match title
                        if not found_title:
                            title_match = title_pattern.match(line)
                            if title_match:
                                title = title_match.group("title").strip()
                                title = re.sub(r"^【[^】]+】", "", title).strip()
                                found_title = True
                                continue
                        
                        # Match status
                        if not found_status:
                            status_match = status_pat.search(line)
                            if status_match:
                                status = status_match.group("val").strip()
                                status = re.sub(r"\*\*|\*", "", status).strip()
                                found_status = True
                                continue
                        
                        # Match ID
                        if not found_id:
                            id_match = id_pat.search(line)
                            if id_match:
                                plan_id = id_match.group("val").strip()
                                plan_id = re.sub(r"\*\*|\*|`", "", plan_id).strip()
                                found_id = True
                                continue

                        # Match date
                        if not found_date:
                            date_match = date_pat.search(line)
                            if date_match:
                                date = date_match.group("val").strip()
                                date = re.sub(r"\*\*|\*|`", "", date).strip()
                                found_date = True
                                continue
                            
                        # Match priority
                        if not found_prio:
                            prio_match = prio_pat.search(line)
                            if prio_match:
                                priority = prio_match.group("val").strip()
                                priority = re.sub(r"\*\*|\*|`", "", priority).strip()
                                found_prio = True
                                continue

                        # Match owner
                        if not found_owner:
                            owner_match = owner_pat.search(line)
                            if owner_match:
                                owner = owner_match.group("val").strip()
                                owner = re.sub(r"\*\*|\*|`|\[|\]", "", owner).strip()
                                found_owner = True
                                continue
            except Exception as e:
                print(f"Warning: Failed to read {file}: {e}", file=sys.stderr)
                continue
                
            category = classify_status(status)
            
            # Don't list raw templates
            if category == "Template":
                continue
                
            plans.append({
                "file": rel_path,
                "path": file_path,
                "domain": domain,
                "title": title,
                "status": status,
                "category": category,
                "plan_id": plan_id,
                "date": date,
                "priority": priority,
                "owner": owner
            })
        
    return plans

def main():
    parser = argparse.ArgumentParser(description="查询和过滤实施计划（Implementation Plans）的执行状态。")
    parser.add_argument(
        "--status", "-s", type=str,
        help="按计划状态过滤。可选分类：completed/已完成、progress/执行中、draft/未完成/草拟、paused/暂停、canceled/取消。也可以直接输入任意状态关键字模糊匹配。"
    )
    parser.add_argument("--domain", "-d", type=str, help="按领域（core, tools, frontend, unisim）过滤。")
    parser.add_argument("--owner", "-o", type=str, help="按计划负责人进行过滤。")
    parser.add_argument("--priority", "-p", type=str, help="按优先级（如 P0, P1, P2）进行过滤。")
    parser.add_argument("--all", "-a", action="store_true", help="显示所有计划（默认在未指定筛选条件时显示摘要和非完成计划）。")
    parser.add_argument("--include-unknown", action="store_true", help="在列表中包含无法解析出状态的旧格式或非计划文件。")
    args = parser.parse_args()

    plans = parse_plans()
    if not plans:
        print("没有找到任何实施计划文件。")
        sys.exit(0)

    # By default, filter out Unknown category plans unless explicitly requested
    if not args.include_unknown:
        plans = [p for p in plans if p["category"] != "Unknown"]
        if not plans:
            print("没有找到任何符合标准模板格式的实施计划（可使用 --include-unknown 参数查看旧格式或未知状态文档）。")
            sys.exit(0)

    # Apply filters
    filtered_plans = plans
    
    # 1. Status filter
    status_filter_text = ""
    if args.status:
        sf = args.status.lower()
        if sf in ["completed", "finished", "done", "已完成", "完成"]:
            filtered_plans = [p for p in filtered_plans if p["category"] == "Completed"]
            status_filter_text = "已完成 (Completed)"
        elif sf in ["progress", "doing", "in-progress", "in_progress", "执行中", "进行中"]:
            filtered_plans = [p for p in filtered_plans if p["category"] == "In Progress"]
            status_filter_text = "执行中 (In Progress)"
        elif sf in ["draft", "todo", "未完成", "草拟", "草稿"]:
            filtered_plans = [p for p in filtered_plans if p["category"] == "Draft"]
            status_filter_text = "未完成/草拟 (Draft)"
        elif sf in ["paused", "暂停"]:
            filtered_plans = [p for p in filtered_plans if p["category"] == "Paused"]
            status_filter_text = "暂停 (Paused)"
        elif sf in ["canceled", "cancelled", "obsolete", "取消", "作废", "废弃"]:
            filtered_plans = [p for p in filtered_plans if p["category"] == "Canceled"]
            status_filter_text = "已取消/作废 (Canceled)"
        else:
            # Custom keyword match
            filtered_plans = [p for p in filtered_plans if sf in p["status"].lower() or sf in p["category"].lower()]
            status_filter_text = f"自定义匹配 '{args.status}'"

    # 2. Owner filter
    if args.owner:
        of = args.owner.lower()
        filtered_plans = [p for p in filtered_plans if of in p["owner"].lower()]
        
    # 3. Priority filter
    if args.priority:
        pf = args.priority.lower()
        filtered_plans = [p for p in filtered_plans if pf in p["priority"].lower()]

    # Print summary counts
    counts = {"Completed": 0, "In Progress": 0, "Draft": 0, "Paused": 0, "Canceled": 0, "Unknown": 0}
    for p in plans:
        counts[p["category"]] = counts.get(p["category"], 0) + 1

    total_plans = len(plans)

    # Determine if we should show a detailed list or table view
    # If any specific filter is applied, show detailed view.
    has_filter = args.status or args.owner or args.priority
    
    print("\n" + "=" * 100)
    print(f" 📋 实施计划状态统计: 共 {total_plans} 项 | {get_status_emoji('Completed')}: {counts['Completed']} | {get_status_emoji('In Progress')}: {counts['In Progress']} | {get_status_emoji('Draft')}: {counts['Draft']} | {get_status_emoji('Paused')}: {counts['Paused']} | {get_status_emoji('Canceled')}: {counts['Canceled']}")
    print("=" * 100)

    if has_filter:
        filter_desc = []
        if status_filter_text:
            filter_desc.append(f"状态: {status_filter_text}")
        if args.owner:
            filter_desc.append(f"负责人: {args.owner}")
        if args.priority:
            filter_desc.append(f"优先级: {args.priority}")
        print(f" [*] 筛选条件 -> {' | '.join(filter_desc)} (共 {len(filtered_plans)} 项)")
        print("-" * 100)
        
        if not filtered_plans:
            print("  没有找到匹配的实施计划。")
        else:
            for p in filtered_plans:
                print(f"  • [{p['plan_id']}] {p['title']}")
                print(f"    文件: docs/implementation-plans/{p['file']}")
                print(f"    状态: {get_status_emoji(p['category'])} ({p['status']})")
                print(f"    负责人: {p['owner']}  |  创建日期: {p['date']}  |  优先级: {p['priority']}")
                print("-" * 100)
    else:
        # Default view (or --all): Show table view
        display_plans = filtered_plans
        if not args.all:
            # If not --all, we list the Active (In Progress, Draft, Paused) plans, and omit Completed/Canceled, but list their count
            display_plans = [p for p in filtered_plans if p["category"] not in ["Completed", "Canceled"]]
            print(f" [*] 当前活跃计划列表 (未完成/执行中/暂停) - 共 {len(display_plans)} 项:")
        else:
            print(f" [*] 所有计划列表 - 共 {len(display_plans)} 项:")
            
        print("  " + "-" * 96)
        print(f"  {pad_string('状态类别', 12)} | {pad_string('计划编号', 25)} | {pad_string('负责人', 10)} | {'计划标题'}")
        print("  " + "-" * 96)
        
        for p in display_plans:
            status_disp = get_status_emoji(p["category"])
            id_disp = truncate_string(p["plan_id"], 25)
            owner_disp = truncate_string(p["owner"], 10)
            title_disp = truncate_string(p["title"], 40)
            
            print(f"  {pad_string(status_disp, 12)} | {pad_string(id_disp, 25)} | {pad_string(owner_disp, 10)} | {title_disp}")
            
        print("  " + "-" * 96)
        if not args.all:
            print(f"  提示: 默认仅显示活跃计划。使用 '-a' 或 '--all' 显示包括已完成/已取消在内的所有计划。")
            print(f"        使用 '-s completed' 或 '-s 已完成' 可以查看已完成的计划列表。")
        print("=" * 100 + "\n")

if __name__ == "__main__":
    main()
