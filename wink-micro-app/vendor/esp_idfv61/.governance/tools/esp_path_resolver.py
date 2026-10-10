#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
esp_path_resolver.py — ESP-IDF 源码与官方示例路径统一解析器 (兼容代理与诊断入口)
=============================================================================
在 ESP-IDF v6.1 官方示例治理体系中提供统一路径解析。
底层核心逻辑由 loop.harness.idf_paths 承接，本脚本保留为 CLI 诊断入口及兼容层。

用法示例:
    python esp_path_resolver.py                      # 诊断并打印当前探测到的环境
    python esp_path_resolver.py --set-path <PATH>    # 持久化保存本机 ESP-IDF 路径
    python esp_path_resolver.py --find-example wifi  # 检索匹配的示例工程
"""
from __future__ import annotations

import argparse
import io
import sys
from pathlib import Path

# Windows 终端编码防御
if isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if isinstance(sys.stderr, io.TextIOWrapper):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

_SCRIPT_DIR = Path(__file__).resolve().parent
_GOV_DIR = _SCRIPT_DIR.parent if _SCRIPT_DIR.name == "tools" else _SCRIPT_DIR
if str(_GOV_DIR) not in sys.path:
    sys.path.insert(0, str(_GOV_DIR))

from loop.harness.idf_paths import (
    LOCAL_CONFIG_FILE,
    WELL_KNOWN_CANDIDATES,
    add_idf_cli_arguments,
    find_component,
    find_example,
    get_idf_components_dir,
    get_idf_examples_dir,
    get_idf_root,
    get_idf_version,
    load_local_config,
    save_local_config,
    validate_idf_root,
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="ESP-IDF 源码与官方示例路径统一解析工具"
    )
    add_idf_cli_arguments(parser)
    parser.add_argument(
        "--set-path",
        type=str,
        default=None,
        help="持久化配置本机 ESP-IDF 路径至 local_config.json",
    )
    parser.add_argument(
        "--find-example",
        type=str,
        default=None,
        help="按关键字检索官方示例工程",
    )
    parser.add_argument(
        "--find-component",
        type=str,
        default=None,
        help="按名称检索官方组件目录",
    )
    args = parser.parse_args()

    if args.set_path:
        saved_path = save_local_config(args.set_path)
        print(f"✅ 已成功保存本地 ESP-IDF 路径至 {LOCAL_CONFIG_FILE}:")
        print(f"   {saved_path}")
        return 0

    try:
        idf_root, source = get_idf_root(args.idf_path)
        examples_dir = get_idf_examples_dir(args.idf_examples, idf_root)
        components_dir = get_idf_components_dir(idf_root)
        version_str = get_idf_version(idf_root)
    except FileNotFoundError as e:
        print(f"❌ 错误: {e}")
        return 1

    if args.find_example:
        results = find_example(args.find_example, idf_root)
        print(f"\n🔍 检索示例关键字 '{args.find_example}' (共找到 {len(results)} 个匹配工程):")
        for r in results:
            rel = r.relative_to(examples_dir).as_posix()
            print(f"  • examples/{rel} -> {r}")
        return 0

    if args.find_component:
        res = find_component(args.find_component, idf_root)
        if res:
            print(f"\n🔍 找到组件 '{args.find_component}': {res}")
        else:
            print(f"\n❌ 未找到匹配的组件: '{args.find_component}'")
        return 0

    example_cmakes = list(examples_dir.rglob("CMakeLists.txt"))
    filtered_examples = [
        c.parent for c in example_cmakes
        if not any(part in ("build", "managed_components", "main") for part in c.parts)
    ]
    components_count = len([d for d in components_dir.iterdir() if d.is_dir()])

    print("=" * 72)
    print("📍 ESP-IDF 路径统一解析器环境诊断报告")
    print("=" * 72)
    print(f"  源码根目录 (IDF_ROOT)   : {idf_root}")
    print(f"  解析来源 (Source)       : {source}")
    print(f"  版本标识 (Version)      : {version_str}")
    print(f"  示例目录 (Examples)     : {examples_dir} ({len(filtered_examples)} 个示例)")
    print(f"  组件目录 (Components)   : {components_dir} ({components_count} 个组件)")
    print(f"  本地配置文件            : {LOCAL_CONFIG_FILE} ({'已生效' if LOCAL_CONFIG_FILE.exists() else '未创建'})")
    print("=" * 72)
    print("💡 提示: 若要在后续 Checklist 任务中随时覆盖，可配置环境变量 IDF_PATH，")
    print("        或运行: python esp_path_resolver.py --set-path <DIR>")
    print("=" * 72)
    return 0


__all__ = [
    "LOCAL_CONFIG_FILE",
    "WELL_KNOWN_CANDIDATES",
    "load_local_config",
    "save_local_config",
    "validate_idf_root",
    "get_idf_root",
    "get_idf_examples_dir",
    "get_idf_components_dir",
    "get_idf_version",
    "find_example",
    "find_component",
    "add_idf_cli_arguments",
    "main",
]

if __name__ == "__main__":
    sys.exit(main())
