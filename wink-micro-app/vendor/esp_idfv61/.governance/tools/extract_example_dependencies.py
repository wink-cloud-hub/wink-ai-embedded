#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
extract_example_dependencies.py — 官方 478 个示例静态依赖与符号提取器
=============================================================================
依据 ADR-0092 与 Sprint 0 实施计划：
自底向上递归扫描 ESP-IDF v6.1 478 个官方示例源码中的 #include 头文件、
核心 API 符号与 Kconfig 开关，生成客观、机器验证的依赖图谱草稿。

用法:
    python extract_example_dependencies.py [--output-report report.json]
"""

from __future__ import annotations

import argparse
import io
import json
import os
import re
import sys
from collections import Counter
from pathlib import Path
from typing import Any, Dict, List, Set, Tuple

# Windows 终端编码防御
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

SCRIPT_DIR = Path(__file__).resolve().parent
GOV_DIR = SCRIPT_DIR.parent if SCRIPT_DIR.name == "tools" else SCRIPT_DIR
DATA_JSON = (
    GOV_DIR / "data" / "checklist.data.json"
    if (GOV_DIR / "data" / "checklist.data.json").exists()
    else GOV_DIR / "checklist.data.json"
)
CATALOG_YAML = (
    GOV_DIR / "catalog" / "capability-catalog.yaml"
    if (GOV_DIR / "catalog" / "capability-catalog.yaml").exists()
    else GOV_DIR / "capability-catalog.yaml"
)

# 导入统一路径解析器
try:
    from esp_path_resolver import get_idf_examples_dir, add_idf_cli_arguments
except ImportError:
    if str(SCRIPT_DIR) not in sys.path:
        sys.path.insert(0, str(SCRIPT_DIR))
    from esp_path_resolver import get_idf_examples_dir, add_idf_cli_arguments

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+["<]([^">]+)[">]', re.MULTILINE)
CONFIG_RE = re.compile(r'^\s*(CONFIG_[A-Za-z0-9_]+)\s*=', re.MULTILINE)

# 物理硬件不可逆特征关键字（用于判断 scope_out）
PHYSICAL_OUT_PATTERNS = {
    "efuse": "涉及硬件 eFuse 物理熔断写入与防篡改烧录",
    "ethernet": "依赖板外物理变压器及外部 PHY 芯片（RMII/SMI 总线）",
    "phy": "芯片工厂模拟射频电气校准与功率表",
    "ftm": "Wi-Fi 纳秒级微波空间飞行时间精密测距",
    "csi": "Wi-Fi 信道状态空间电磁微波探针",
    "zigbee": "依赖 2.4GHz 空间物理射频网卡与 IEEE 802.15.4 基带解调",
    "openthread": "依赖 802.15.4 空间物理射频网卡",
    "ieee802154": "依赖 802.15.4 空间物理射频网卡",
    "mesh": "多机空间电磁自组网，需分布式拓扑模拟",
    "custom_bootloader": "芯片二级引导程序，纯软件仿真直接进入 app_main",
    "build_system": "构建工具链自身测试，非嵌入式运行时业务代码",
}


def extract_includes_from_file(file_path: Path) -> Set[str]:
    try:
        content = file_path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return set()
    return set(INCLUDE_RE.findall(content))


def extract_configs_from_file(file_path: Path) -> Set[str]:
    try:
        content = file_path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return set()
    return set(CONFIG_RE.findall(content))


def scan_example_directory(example_dir: Path) -> Dict[str, Any]:
    """扫描单个官方示例工程目录，收集头文件与配置"""
    includes: Set[str] = set()
    configs: Set[str] = set()
    source_files: List[str] = []

    for ext in ("*.c", "*.h", "*.cpp", "*.hpp"):
        for p in example_dir.rglob(ext):
            # 过滤 build 和 managed_components
            parts = p.parts
            if "build" in parts or "managed_components" in parts:
                continue
            includes.update(extract_includes_from_file(p))
            source_files.append(p.relative_to(example_dir).as_posix())

    # 扫描 sdkconfig.defaults
    for p in example_dir.glob("sdkconfig*"):
        if p.is_file():
            configs.update(extract_configs_from_file(p))

    return {
        "source_count": len(source_files),
        "includes": sorted(list(includes)),
        "configs": sorted(list(configs)),
    }


def analyze_all_examples(
    examples_root: Path, data_path: Path
) -> Tuple[List[Dict[str, Any]], Dict[str, Any]]:
    with open(data_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    entries = data.get("entries", [])
    results: List[Dict[str, Any]] = []

    header_counter: Counter[str] = Counter()
    category_summary: Dict[str, int] = {}
    out_of_scope_counter: Counter[str] = Counter()

    for e in entries:
        eid = e["id"]
        up = e["upstream_path"]
        rel = up[len("examples/") :] if up.startswith("examples/") else up
        ex_dir = examples_root / rel

        cat = rel.split("/")[0]
        category_summary[cat] = category_summary.get(cat, 0) + 1

        scan_res = (
            scan_example_directory(ex_dir)
            if ex_dir.is_dir()
            else {"source_count": 0, "includes": [], "configs": []}
        )

        for inc in scan_res["includes"]:
            header_counter[inc] += 1

        # 判定是否属于物理不可逆范围排除 (Out-of-Scope)
        suggested_scope = "in_scope"
        exclusion_reason = None

        # 1. 按大类判定
        if cat in PHYSICAL_OUT_PATTERNS:
            suggested_scope = "out_of_scope"
            exclusion_reason = PHYSICAL_OUT_PATTERNS[cat]
            out_of_scope_counter[cat] += 1
        elif "bluedroid" in rel:
            suggested_scope = "out_of_scope"
            exclusion_reason = "架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存"
            out_of_scope_counter["bluetooth_bluedroid"] += 1
        else:
            # 2. 按使用的头文件排查是否有强物理硬件依赖
            inc_set = set(scan_res["includes"])
            if any("efuse" in h.lower() for h in inc_set) and cat not in ("get-started",):
                # 涉及硬件熔丝安全烧录
                suggested_scope = "out_of_scope"
                exclusion_reason = "调用底层硬件 eFuse 物理熔断驱动，软件仿真无法进行不可逆电气熔断"
                out_of_scope_counter["hardware_efuse"] += 1

        results.append(
            {
                "id": eid,
                "display_id": e.get("display_id"),
                "upstream_path": up,
                "category": cat,
                "exists_on_disk": ex_dir.is_dir(),
                "source_count": scan_res["source_count"],
                "suggested_scope": suggested_scope,
                "exclusion_reason": exclusion_reason,
                "current_scope": e.get("scope", {}).get("inclusion"),
                "includes": scan_res["includes"],
                "configs": scan_res["configs"],
            }
        )

    stats = {
        "total_examples": len(entries),
        "total_categories": len(category_summary),
        "category_distribution": category_summary,
        "suggested_scope_distribution": Counter(r["suggested_scope"] for r in results),
        "top_50_headers": header_counter.most_common(50),
        "out_of_scope_reasons": out_of_scope_counter,
    }

    return results, stats


def main() -> int:
    parser = argparse.ArgumentParser(description="ESP-IDF 官方示例静态依赖提取器")
    add_idf_cli_arguments(parser)
    parser.add_argument(
        "--output-report",
        type=Path,
        default=SCRIPT_DIR / "extracted_dependencies_report.json",
        help="输出的结构化分析报告路径",
    )
    args = parser.parse_args()

    examples_root = get_idf_examples_dir(args.idf_examples, args.idf_path)
    print(f"[extract] 使用 ESP-IDF 示例根目录: {examples_root}")
    print(f"[extract] 正在扫描 {DATA_JSON} 中 478 个示例的源码依赖...")

    results, stats = analyze_all_examples(examples_root, DATA_JSON)

    print("\n" + "=" * 70)
    print("📊 静态依赖提取与全量特征统计报告")
    print("=" * 70)
    print(f"总计示例数: {stats['total_examples']}")
    print("\n建议产品范围分布 (Suggested Scope):")
    for scope, count in stats["suggested_scope_distribution"].items():
        pct = (count / stats["total_examples"]) * 100
        print(f"  - {scope:15s}: {count:4d} ({pct:5.1f}%)")

    print("\n前 20 项最核心头文件 (Top 20 Core Headers):")
    for inc, cnt in stats["top_50_headers"][:20]:
        print(f"  {cnt:3d} 次 | {inc}")

    output_path = args.output_report
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump({"stats": stats, "results": results}, f, indent=2, ensure_ascii=False)

    print(f"\n[extract] 完整静态分析报告已写入: {output_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
