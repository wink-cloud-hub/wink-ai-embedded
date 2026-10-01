#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
esp_path_resolver.py — ESP-IDF 源码与官方示例路径统一解析器
=============================================================================
在 ESP-IDF v6.1 官方示例治理体系与 Checklist 计划中提供统一、可插拔的路径解析：
优先级自顶向下依次为：
  1. CLI 命令行参数 (--idf-path 或 --idf-examples)
  2. 环境变量 (IDF_PATH, ESP_IDF_PATH, ESP_IDF_EXAMPLES_DIR)
  3. 本地配置文件 (.governance/local_config.json, gitignore 隔离)
  4. 典型路径与已知环境候选列表自动探查 (Windows/Linux/macOS)

提供函数：
  - get_idf_root(): 获取 ESP-IDF 源码根目录
  - get_idf_examples_dir(): 获取 examples/ 目录
  - get_idf_components_dir(): 获取 components/ 目录
  - find_example(pattern): 检索官方示例路径
  - find_component(name): 检索官方组件路径
  - add_idf_cli_arguments(parser): 为脚本一键注入统一 CLI 参数
  - save_local_config(path): 持久化写入本地路径配置

用法示例:
    python esp_path_resolver.py                      # 诊断并打印当前探测到的环境
    python esp_path_resolver.py --set-path <PATH>    # 持久化保存本机 ESP-IDF 路径
    python esp_path_resolver.py --find-example wifi  # 检索匹配的示例工程
"""

from __future__ import annotations

import argparse
import io
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# Windows 终端编码防御
if isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if isinstance(sys.stderr, io.TextIOWrapper):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

SCRIPT_DIR = Path(__file__).resolve().parent
GOV_DIR = SCRIPT_DIR.parent if SCRIPT_DIR.name == "tools" else SCRIPT_DIR
LOCAL_CONFIG_FILE = GOV_DIR / "local_config.json"

# 已知典型探查路径候选表 (由近及远、由具体到常规)
WELL_KNOWN_CANDIDATES = [
    Path(r"D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf"),
    Path(r"C:\Espressif\frameworks\esp-idf-v6.1"),
    Path(r"C:\Espressif\frameworks\esp-idf"),
    Path.home() / ".espressif" / "v6.1" / "esp-idf",
    Path.home() / "esp" / "esp-idf",
]


def load_local_config() -> Dict[str, Any]:
    """读取本地配置文件 (如果存在)"""
    if LOCAL_CONFIG_FILE.is_file():
        try:
            with open(LOCAL_CONFIG_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return {}
    return {}


def save_local_config(idf_path: Path | str) -> Path:
    """持久化保存本机 ESP-IDF 路径到 local_config.json"""
    path_obj = Path(idf_path).resolve()
    if not path_obj.is_dir():
        raise FileNotFoundError(f"指定的路径不存在或不是目录: {path_obj}")
    
    # 验证是否包含 examples 或 components
    if not (path_obj / "examples").is_dir() and not (path_obj / "components").is_dir():
        # 如果传入的是 examples 目录本身，尝试回退到上级
        if path_obj.name == "examples" and (path_obj.parent / "components").is_dir():
            path_obj = path_obj.parent

    config = load_local_config()
    config["idf_path"] = str(path_obj)

    with open(LOCAL_CONFIG_FILE, "w", encoding="utf-8") as f:
        json.dump(config, f, indent=2, ensure_ascii=False)
    
    return path_obj


def validate_idf_root(p: Path) -> bool:
    """验证目录是否为合法的 ESP-IDF 根目录"""
    if not p.is_dir():
        return False
    # ESP-IDF 根目录通常包含 examples 和 components
    return (p / "examples").is_dir() or (p / "components").is_dir()


def get_idf_root(explicit_path: Optional[Path | str] = None) -> Tuple[Path, str]:
    """
    解析 ESP-IDF 源码根目录，返回 (Path, source_description)
    """
    # 1. 显式参数
    if explicit_path:
        p = Path(explicit_path).resolve()
        if validate_idf_root(p):
            return p, "CLI explicit parameter"
        # 兼容用户传入 examples 目录的情况
        if p.name == "examples" and validate_idf_root(p.parent):
            return p.parent, "CLI explicit parameter (parent of examples)"
        if p.is_dir():
            return p, "CLI explicit parameter"

    # 2. 环境变量 (优先 IDF_PATH，标准原厂环境变量)
    for env_var in ("IDF_PATH", "ESP_IDF_PATH"):
        env_val = os.environ.get(env_var, "").strip()
        if env_val:
            p = Path(env_val).resolve()
            if validate_idf_root(p):
                return p, f"Environment variable {env_var}"

    # 3. 环境变量 (ESP_IDF_EXAMPLES_DIR -> 获取其父目录)
    examples_env = os.environ.get("ESP_IDF_EXAMPLES_DIR", "").strip()
    if examples_env:
        p = Path(examples_env).resolve()
        if p.is_dir():
            parent = p.parent
            if validate_idf_root(parent):
                return parent, "Derived from ESP_IDF_EXAMPLES_DIR parent"
            return p, "Direct ESP_IDF_EXAMPLES_DIR (as root fallback)"

    # 4. 本地配置文件 (local_config.json)
    local_cfg = load_local_config()
    cfg_path = local_cfg.get("idf_path", "").strip()
    if cfg_path:
        p = Path(cfg_path).resolve()
        if validate_idf_root(p):
            return p, f"Local config ({LOCAL_CONFIG_FILE.name})"

    # 5. 探查已知候选路径列表
    for cand in WELL_KNOWN_CANDIDATES:
        if validate_idf_root(cand):
            return cand.resolve(), "Well-known system candidate"

    # 未找到
    searched_hints = [
        "1. CLI: --idf-path <DIR>",
        "2. Env: set IDF_PATH=<DIR>",
        f"3. Config: python {Path(__file__).name} --set-path <DIR>",
        "4. Standard paths: " + ", ".join(str(c) for c in WELL_KNOWN_CANDIDATES[:2]),
    ]
    raise FileNotFoundError(
        "未能定位有效的 ESP-IDF 源码目录！\n可通过以下任一方式指定：\n"
        + "\n".join("   " + h for h in searched_hints)
    )


def get_idf_examples_dir(explicit_examples: Optional[Path | str] = None,
                         explicit_root: Optional[Path | str] = None) -> Path:
    """获取 ESP-IDF 官方 examples 目录"""
    if explicit_examples:
        p = Path(explicit_examples).resolve()
        if p.is_dir():
            return p

    env_examples = os.environ.get("ESP_IDF_EXAMPLES_DIR", "").strip()
    if env_examples:
        p = Path(env_examples).resolve()
        if p.is_dir():
            return p

    idf_root, _ = get_idf_root(explicit_root)
    examples_dir = idf_root / "examples"
    if examples_dir.is_dir():
        return examples_dir.resolve()
    
    # 如果 idf_root 本身即是 examples
    if idf_root.name == "examples" and idf_root.is_dir():
        return idf_root.resolve()

    raise FileNotFoundError(f"在 ESP-IDF 目录中未找到 examples 子目录: {idf_root}")


def get_idf_components_dir(explicit_root: Optional[Path | str] = None) -> Path:
    """获取 ESP-IDF 官方 components 目录"""
    idf_root, _ = get_idf_root(explicit_root)
    comp_dir = idf_root / "components"
    if comp_dir.is_dir():
        return comp_dir.resolve()
    raise FileNotFoundError(f"在 ESP-IDF 目录中未找到 components 子目录: {idf_root}")


def get_idf_version(idf_root: Path) -> str:
    """探测 ESP-IDF 版本号（通过 git describe 或 version.txt）"""
    version_file = idf_root / "version.txt"
    if version_file.is_file():
        try:
            return version_file.read_text(encoding="utf-8").strip()
        except Exception:
            pass

    # 尝试 git describe
    if (idf_root / ".git").exists():
        try:
            res = subprocess.run(
                ["git", "-C", str(idf_root), "describe", "--tags", "--always"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=3,
            )
            if res.returncode == 0 and res.stdout.strip():
                return res.stdout.strip()
        except Exception:
            pass

    return "unknown (no git tag or version.txt)"


def find_example(pattern: str, explicit_root: Optional[Path | str] = None) -> List[Path]:
    """模糊或精确查找官方示例工程目录"""
    examples_dir = get_idf_examples_dir(explicit_root=explicit_root)
    matched = []
    pat = pattern.lower().replace("\\", "/")
    
    # 遍历所有含有 CMakeLists.txt 的示例目录
    for cm in examples_dir.rglob("CMakeLists.txt"):
        ex_dir = cm.parent
        # 排除 build / managed_components
        if any(part in ("build", "managed_components", "main") for part in ex_dir.parts):
            continue
        rel = ex_dir.relative_to(examples_dir).as_posix().lower()
        if pat in rel:
            matched.append(ex_dir)
            
    return matched


def find_component(name: str, explicit_root: Optional[Path | str] = None) -> Optional[Path]:
    """查找官方组件目录"""
    comp_dir = get_idf_components_dir(explicit_root=explicit_root)
    target = comp_dir / name
    if target.is_dir():
        return target
    # 模糊匹配
    for d in comp_dir.iterdir():
        if d.is_dir() and d.name.lower() == name.lower():
            return d
    return None


def add_idf_cli_arguments(parser: argparse.ArgumentParser) -> None:
    """向 ArgumentParser 添加标准统一的 ESP-IDF 路径参数"""
    parser.add_argument(
        "--idf-path",
        type=Path,
        default=None,
        help="ESP-IDF 源码根目录 (默认自动按 IDF_PATH、local_config.json 或已知路径解析)",
    )
    parser.add_argument(
        "--idf-examples",
        type=Path,
        default=None,
        help="ESP-IDF 官方 examples 目录路径 (默认解析自 --idf-path/examples)",
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

    # 统计示例工程数量
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


if __name__ == "__main__":
    sys.exit(main())
