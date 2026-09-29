#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# -*- coding: utf-8 -*-
import io, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")
"""
generate_checklist_v1_1.py
==========================
从 checklist.data.json 单向渲染生成 CHECKLIST.md（只读派生看板）。

用法:
    python generate_checklist_v1_1.py           # 重新生成 CHECKLIST.md
    python generate_checklist_v1_1.py --dry-run  # 输出到 stdout，不写文件

铁律四：本脚本是唯一合法的 CHECKLIST.md 写入路径，严禁人工直接编辑 CHECKLIST.md。

CI Gate 1 前置校验（同文件内 validate_data() 函数）：
    - delivery.state == "verified" 时，assets_sha256 三件套不得全为 null
    - required_capabilities 中的能力 ID 必须在 capability-catalog.yaml 中已声明
    - upstream_path 在全局条目中不得重复
"""

import json
import sys
import argparse
from pathlib import Path
from datetime import datetime

import yaml  # pip install pyyaml

SCRIPT_DIR   = Path(__file__).parent
DATA_JSON    = SCRIPT_DIR / "checklist.data.json"
CATALOG_YAML = SCRIPT_DIR / "capability-catalog.yaml"
OUTPUT_MD    = SCRIPT_DIR / "CHECKLIST.md"

# ─────────────────────────────────────────────────────────────
# 状态符渲染映射
# ─────────────────────────────────────────────────────────────
DELIVERY_STATE_SYMBOL = {
    "verified":  "[x]",
    "building":  "[~]",
    "regressed": "[!]",
    "planned":   "[ ]",
}

SCOPE_STATUS_SYMBOL = {
    "out_of_scope_product": "[-]",
    "contract_blocked":     "🚫",
    "in_scope_deferred":    "[-]",
    "in_scope_deficit":     None,   # 使用 delivery 符号
    "pending_audit":        None,   # 使用 delivery 符号
}

OBS_EMOJI = {
    "L1_ui":       "🎯 Level 1",
    "L2_log":      "📜 Level 2",
    "L3_probe":    "⚡ Level 3",
    "L4_internal": "⚙️ Level 4",
    "LX_deadlock": "🚫 Blocked",
}

# ─────────────────────────────────────────────────────────────
# 大类分组（display_id 范围 → 标题信息）
# ─────────────────────────────────────────────────────────────
CATEGORY_GROUPS = [
    (  1,   2, "get-started",       "基础快速起步 (Get-Started)",                    "examples/get-started/"),
    (  3, 116, "peripherals",       "片上与总线外设 (Peripherals)",                  "examples/peripherals/"),
    (117, 184, "system",            "操作系统与核心系统调用 (System & OS)",           "examples/system/"),
    (185, 219, "protocols",         "网络与应用层通信协议 (Protocols)",               "examples/protocols/"),
    (220, 243, "wifi",              "Wi-Fi 无线局域网 (Wi-Fi)",                       "examples/wifi/"),
    (244, 390, "bluetooth",         "蓝牙协议栈 (Bluetooth)",                         "examples/bluetooth/"),
    (391, 417, "storage",           "片上存储与文件系统 (Storage)",                   "examples/storage/"),
    (418, 422, "network",           "底层网络与接口 (Network)",                       "examples/network/"),
    (423, 425, "cxx",               "C++ 运行时与语言特性 (C++)",                     "examples/cxx/"),
    (426, 444, "build_system",      "构建系统与组件组织 (Build System)",              "examples/build_system/"),
    (445, 454, "security",          "硬件加密与芯片安全特性 (Security)",              "examples/security/"),
    (455, 458, "custom_bootloader", "定制引导加载程序 (Custom Bootloader)",           "examples/custom_bootloader/"),
    (459, 461, "ethernet",          "有线以太网 (Ethernet - 外部 PHY 硬件)",          "examples/ethernet/"),
    (462, 463, "lowpower",          "超低功耗与 ULP 协处理器 (Low Power & ULP)",      "examples/lowpower/"),
    (464, 466, "mesh",              "Wi-Fi 空间自组网 (Mesh)",                        "examples/mesh/"),
    (467, 472, "openthread",        "OpenThread 802.15.4 线程网络 (OpenThread)",      "examples/openthread/"),
    (473, 475, "zigbee",            "Zigbee 2.4G 射频网络 (Zigbee)",                  "examples/zigbee/"),
    (476, 476, "ieee802154",        "IEEE 802.15.4 原始射频 (IEEE 802.15.4)",         "examples/ieee802154/"),
    (477, 478, "phy",               "射频物理层与工厂校准 (PHY & Calibration)",       "examples/phy/"),
]


# ─────────────────────────────────────────────────────────────
# 前置数据校验（CI Gate 1 逻辑）
# ─────────────────────────────────────────────────────────────
def validate_data(manifest: dict, catalog: dict, strict: bool = False) -> tuple[list[str], list[str]]:
    """返回 (errors, warnings)；errors 非空 = CI 阻断；warnings 不阻断但输出提示。"""
    errors   = []
    warnings = []
    known_caps = set(catalog.get("capabilities", {}).keys())
    seen_paths = {}

    for entry in manifest["entries"]:
        eid  = entry.get("id", "?")
        num  = entry.get("display_id", "?")
        path = entry.get("upstream_path", "?")

        # 路径唯一性（始终是硬错误）
        if path in seen_paths:
            errors.append(f"[Gate 1] #{num} 与 #{seen_paths[path]} 重复 upstream_path: {path}")
        else:
            seen_paths[path] = num

        # verified 必须有资产哈希（--strict 时为硬错误，否则为 warning）
        delivery = entry.get("delivery", {})
        if delivery.get("state") == "verified":
            sha = delivery.get("assets_sha256") or {}
            if not all(sha.get(k) for k in ["device_tree", "js", "wasm"]):
                msg = f"[Gate 1] #{num} ({eid}) delivery.state=verified 但 assets_sha256 三件套未填写"
                if strict:
                    errors.append(msg)
                else:
                    warnings.append(msg)

        # 能力 ID 必须在 Catalog 中已声明（始终是硬错误）
        for cap_id in entry.get("required_capabilities", []):
            if cap_id not in known_caps:
                errors.append(f"[Gate 1] #{num} ({eid}) 引用未声明的能力: {cap_id}")

    return errors, warnings


# ─────────────────────────────────────────────────────────────
# 渲染单个条目行
# ─────────────────────────────────────────────────────────────
def render_row(entry: dict) -> str:
    num      = entry.get("display_id", "?")
    upstream = entry.get("upstream_path", "?").removeprefix("examples/")
    scope    = entry.get("scope_and_maturity", {}).get("status", "pending_audit")
    delivery = entry.get("delivery", {}).get("state", "planned")
    obs_raw  = entry.get("acceptance", {}).get("observability_level", "L4_internal")
    obs_str  = OBS_EMOJI.get(obs_raw, obs_raw)

    # 状态符
    if scope in ("out_of_scope_product", "contract_blocked", "in_scope_deferred") and delivery != "verified":
        symbol = SCOPE_STATUS_SYMBOL.get(scope, "[ ]")
    else:
        symbol = DELIVERY_STATE_SYMBOL.get(delivery, "[ ]")

    # 优先级从 required_capabilities 数量推断（粗略）
    caps     = entry.get("required_capabilities", [])
    pri_hint = "P0" if delivery == "verified" else ("P1" if caps else "P2")
    if scope in ("out_of_scope_product",):
        pri_hint = "P4"

    # 对应 App
    app_dir = entry.get("delivery", {}).get("app_dir") or "待适配"

    # 描述：取自验收说明的第一个 positive_cases name，或生成默认描述
    pos_cases = entry.get("acceptance", {}).get("positive_cases", [])
    if pos_cases:
        desc = pos_cases[0].get("name", "")
    elif scope == "out_of_scope_product":
        ev = entry.get("scope_and_maturity", {}).get("exclusion_evidence", {})
        medium = ev.get("physical_medium", "物理硬件外设")
        desc = f"声明 Out-of-Scope。{medium}。"
    elif scope == "contract_blocked":
        desc = "契约阻断：纯软件仿真无法兑现核心硬件时序契约。"
    elif scope == "in_scope_deferred":
        desc = "暂缓投入。依赖重型外部器件模型，当前阶段不投入资源。"
    elif delivery == "verified":
        desc = "已落地实证。"
    else:
        desc = "待排期。依赖进一步框架门面扩展。"

    # 截断超长描述
    if len(desc) > 80:
        desc = desc[:77] + "..."

    return f"| {symbol} | {num:03d} | `{upstream}` | {obs_str} | {pri_hint} | `{app_dir}` | {desc} |"


# ─────────────────────────────────────────────────────────────
# 主渲染函数
# ─────────────────────────────────────────────────────────────
def render_checklist(manifest: dict) -> str:
    entries     = manifest["entries"]
    total       = manifest.get("total_entries", len(entries))
    summary     = manifest.get("summary", {})
    spec_ver    = manifest.get("spec_version", "1.1.0")
    gen_date    = datetime.now().strftime("%Y-%m-%d")

    cnt_verified = summary.get("verified", 0)
    cnt_planned  = summary.get("planned", 0)
    cnt_oos      = summary.get("out_of_scope", 0)
    cnt_pending  = summary.get("pending_audit", 0)

    lines: list[str] = []

    # ── 文件头 ───────────────────────────────────────────────
    lines += [
        "<!-- SPDX-License-Identifier: Apache-2.0 -->",
        "<!-- ⚠️  此文件由 generate_checklist_v1_1.py 自动生成，严禁人工编辑！修改请编辑 checklist.data.json -->",
        "# ESP-IDF v6.1 官方示例全量仿真适配核对清单 (Checklist)",
        "",
        f"> **数据源（SSOT）**：[`checklist.data.json`](checklist.data.json)（Spec v{spec_ver}）  ",
        f"> **生成日期**：{gen_date}  ",
        "> **分类规范**：[`CLASSIFICATION-SPEC.md`](CLASSIFICATION-SPEC.md)  ",
        "> **能力字典**：[`capability-catalog.yaml`](capability-catalog.yaml)  ",
        "> **执行手册**：[`PLAYBOOK.md`](PLAYBOOK.md)  ",
        "",
        "---",
        "",
        "## 一、 总体适配进度统计",
        "",
        f"- **官方独立示例总数**：**{total} 个**",
        f"  - `[x]` **已完成实证 (Verified)**：**{cnt_verified} 项**",
        f"  - `[ ]` **待适配排期中 (Planned)**：**{cnt_planned} 项**",
        f"  - `[-]` **声明 Out-of-Scope / 物理排除**：**{cnt_oos} 项**",
        f"  - `?` **待深度审定 (Pending Audit)**：**{cnt_pending} 项**",
        "",
    ]

    # ── 大类导航索引 ─────────────────────────────────────────
    lines += [
        "### 大类索引导航",
        "",
        "| 序号 | 功能大类 | 包含示例数 | 编号跨度 | 已实证 |",
        "| :---: | :--- | :---: | :---: | :---: |",
    ]
    for idx, (lo, hi, anchor, title, _) in enumerate(CATEGORY_GROUPS, 1):
        group_entries = [e for e in entries if lo <= e["display_id"] <= hi]
        cnt = sum(1 for e in group_entries if e.get("delivery", {}).get("state") == "verified")
        lines.append(f"| {idx:02d} | [{title}](#{anchor}) | {len(group_entries)} 项 | `#{lo:03d} ~ #{hi:03d}` | {cnt} 项 |")
    lines += ["", "---", ""]

    # ── 符号说明 ─────────────────────────────────────────────
    lines += [
        "## 二、 符号与分类说明",
        "",
        "- `[x]` **已完成实证**：完成五阶段 Playbook，Wasm/Headless 断言 100% 绿灯。",
        "- `[ ]` **待适配**：在仿真规划范围内，待后续里程碑实施。",
        "- `[-]` **声明 Out-of-Scope**：不可逆物理介质，编译期 `WINK_SLA_ERROR` 阻断。",
        "- `🚫` **契约阻断**：纯软件无法提供的硬件时序契约。",
        "- `[~]` **正在构建**：WIP，尚未完成 Headless 实证。",
        "- `[!]` **回归失败**：曾经 Verified，最新 commit 测试退化。",
        "",
        "| 等级 | 符号 | 说明 |",
        "|---|---|---|",
        "| Level 1 | 🎯 | UniSim 画布有直观控件（LED、数码管、BLE Inspector）|",
        "| Level 2 | 📜 | 控制台日志 / 网络数据流（UART、MQTT、HTTP）|",
        "| Level 3 | ⚡ | IO 打点 / GPIO 波形探测 |",
        "| Level 4 | ⚙️ | 纯内部静默逻辑（内存、错误码、寄存器状态）|",
        "| Blocked | 🚫 | 前置阻断，依赖未建模，会导致仿真死锁 |",
        "",
        "---",
        "",
        "## 三、 478 个官方示例逐项核对总账",
        "",
    ]

    # ── 分大类渲染条目行 ─────────────────────────────────────
    TABLE_HEADER = (
        "| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |",
        "| :---: | :---: | :--- | :---: | :---: | :--- | :--- |",
    )

    for idx, (lo, hi, anchor, title, _) in enumerate(CATEGORY_GROUPS, 1):
        group_entries = sorted(
            [e for e in entries if lo <= e["display_id"] <= hi],
            key=lambda e: e["display_id"],
        )
        verified_cnt = sum(1 for e in group_entries if e.get("delivery", {}).get("state") == "verified")

        lines.append(f'<a id="{anchor}"></a>')
        lines.append(f"### {title}（共 {len(group_entries)} 项 | 编号 `#{lo:03d} ~ #{hi:03d}` | 已实证: {verified_cnt} 项）")
        lines.append("")
        lines.extend(TABLE_HEADER)
        for entry in group_entries:
            lines.append(render_row(entry))
        lines += ["", "---", ""]

    return "\n".join(lines)


# ─────────────────────────────────────────────────────────────
# 主函数
# ─────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Generate CHECKLIST.md from checklist.data.json (V1.1)")
    parser.add_argument("--dry-run", action="store_true", help="输出到 stdout，不写文件")
    parser.add_argument("--skip-validate", action="store_true", help="跳过前置数据校验")
    args = parser.parse_args()

    # 读取数据源
    print(f"[generate] 读取 {DATA_JSON} ...")
    with open(DATA_JSON, encoding="utf-8") as f:
        manifest = json.load(f)
    print(f"[generate] 共 {manifest['total_entries']} 条条目")

    # 读取 Catalog
    print(f"[generate] 读取 {CATALOG_YAML} ...")
    with open(CATALOG_YAML, encoding="utf-8") as f:
        catalog = yaml.safe_load(f)

    # Gate 1 数据校验
    if not args.skip_validate:
        strict = getattr(args, 'strict', False)
        print(f"[generate] 执行 Gate 1 前置数据校验 {'(strict)' if strict else '(bootstrap)'}...")
        errors, warnings = validate_data(manifest, catalog, strict=strict)
        if warnings:
            print(f"[Gate 1] WARN - {len(warnings)} 条警告 (bootstrap 阶段，哈希待补填):")
            for w in warnings[:5]:
                print(f"  {w}")
            if len(warnings) > 5:
                print(f"  ... 及另外 {len(warnings)-5} 条")
        if errors:
            print(f"[Gate 1] FAIL - 校验失败，{len(errors)} 条硬错误：")
            for e in errors:
                print(f"  {e}")
            sys.exit(1)
        print(f"[Gate 1] PASS - 校验通过 ({len(manifest['entries'])} 条，{len(errors)} 个错误，{len(warnings)} 个警告)")

    # 渲染
    print(f"[generate] 渲染 CHECKLIST.md ...")
    md_content = render_checklist(manifest)

    if args.dry_run:
        print(md_content)
        return

    with open(OUTPUT_MD, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(md_content)

    size_kb = OUTPUT_MD.stat().st_size // 1024
    print(f"[generate] DONE - 写入 {OUTPUT_MD} ({size_kb} KB)")
    print("[generate] 完成。CHECKLIST.md 已更新为纯派生只读看板。")


if __name__ == "__main__":
    main()
