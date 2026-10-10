#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
cli.triage_soc - SoC Target Compatibility & Triage CLI
=====================================================
Standard CLI entry point to analyze target SoC support across examples.
"""
from __future__ import annotations

import argparse
import io
import json
import sys
from pathlib import Path

# Windows 终端编码防御
if isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if isinstance(sys.stderr, io.TextIOWrapper):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

_GOV_DIR = Path(__file__).resolve().parent.parent
if str(_GOV_DIR) not in sys.path:
    sys.path.insert(0, str(_GOV_DIR))

from loop.harness.idf_paths import get_idf_examples_dir
from loop.services.soc_triage import DATA_PATH, apply_gap_remediation_and_triage


def main() -> int:
    parser = argparse.ArgumentParser(description="SoC Target Compatibility & Triage Tool")
    parser.add_argument("--dry-run", action="store_true", help="Print triage report without modifying files")
    parser.add_argument("--apply", action="store_true", help="Apply triage and gap fixes to checklist.data.json")
    args = parser.parse_args()

    if not DATA_PATH.is_file():
        print(f"Error: {DATA_PATH} not found", file=sys.stderr)
        return 1

    examples_root = get_idf_examples_dir()
    data = json.loads(DATA_PATH.read_text(encoding="utf-8"))
    dry_run = not args.apply

    report = apply_gap_remediation_and_triage(data, examples_root, dry_run=dry_run)

    print("========================================================================")
    print("📍 ESP-IDF 81 项未列 ESP32 支持表冲突分流与清单缺口修复报告")
    print("========================================================================")
    print(f"  模式: {'[DRY-RUN 只读预览]' if dry_run else '[APPLY 已持久化写入]'}")
    print(f"  修复能力缺口 (blecent / NimBLE_Security) : {report['fixed_caps_count']}")
    print(f"  修复 deep_sleep 能力映射               : {'是' if report['fixed_deep_sleep'] else '否'}")
    print(f"  补充 ULP 协处理器能力 (27 项)          : {report['fixed_ulp_count']}")
    print(f"  补充构建系统能力 (19 项)              : {report['fixed_build_system_count']}")
    print(f"  补全已落地示例 header/config 闭包      : {report['filled_landed_closures']}")
    print("  ----------------------------------------------------------------------")
    print(f"  三态分流 1: 物理硬件独占 (deferred)   : {report['triage_hardware_exclusive_deferred']}")
    print(f"  三态分流 2: Kconfig 兼容验证          : {report['triage_kconfig_verified']}")
    print(f"  三态分流 3: 跨芯片通用行为模拟        : {report['triage_generic_behavioral']}")
    print("========================================================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
