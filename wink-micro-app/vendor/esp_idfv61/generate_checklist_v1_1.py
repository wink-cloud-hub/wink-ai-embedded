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
支持 Schema v1.1 与 Schema v2.0，对齐 ADR-0091 与纯函数门禁规范。

用法:
    python generate_checklist_v1_1.py           # 重新生成 CHECKLIST.md
    python generate_checklist_v1_1.py --dry-run  # 输出到 stdout，不写文件
    python generate_checklist_v1_1.py --strict   # 严格模式：存量隔离区外任何未补哈希直接报错

铁律四：本脚本是唯一合法的 CHECKLIST.md 写入路径，严禁人工直接编辑 CHECKLIST.md。
"""

import json
import sys
import argparse
from pathlib import Path
from datetime import datetime, timezone

import yaml

SCRIPT_DIR   = Path(__file__).parent
DATA_JSON    = SCRIPT_DIR / "checklist.data.json"
CATALOG_YAML = SCRIPT_DIR / "capability-catalog.yaml"
QUARANTINE_YAML = SCRIPT_DIR / ".gates" / "quarantine.yaml"
OUTPUT_MD    = SCRIPT_DIR / "CHECKLIST.md"

# ─────────────────────────────────────────────────────────────
# 状态符渲染映射
# ─────────────────────────────────────────────────────────────
DELIVERY_STATE_SYMBOL = {
    "verified":  "[x]",
    "building":  "[~]",
    "regressed": "[!]",
    "stale":     "[!]",
    "planned":   "[ ]",
}

SCOPE_STATUS_SYMBOL = {
    "out_of_scope_product": "[-]",
    "contract_blocked":     "🚫",
    "in_scope_deferred":    "[-]",
    "in_scope_deficit":     None,
    "pending_audit":        None,
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


def load_quarantine() -> dict[str, dict]:
    """读取存量隔离区白名单，返回 {id: entry_dict}。"""
    if not QUARANTINE_YAML.exists():
        return {}
    with open(QUARANTINE_YAML, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f)
    result = {}
    for item in data.get("quarantined_entries", []):
        result[item["id"]] = item
    return result


# ─────────────────────────────────────────────────────────────
# 前置数据校验（CI Gate 1 逻辑，支持纯函数校验与隔离区 TTL）
# ─────────────────────────────────────────────────────────────
def validate_data(manifest: dict, catalog: dict, quarantine: dict[str, dict], strict: bool = False) -> tuple[list[str], list[str]]:
    """返回 (errors, warnings)；errors 非空 = CI 阻断；warnings 不阻断但输出提示。"""
    errors   = []
    warnings = []
    known_caps = set(catalog.get("capabilities", {}).keys())
    seen_paths = {}
    now_utc = datetime.now(timezone.utc)
    is_v2 = manifest.get("spec_version", "").startswith("2.")

    for entry in manifest["entries"]:
        eid  = entry.get("id", "?")
        num  = entry.get("display_id", "?")
        path = entry.get("upstream_path", "?")

        # 路径唯一性（始终是硬错误）
        if path in seen_paths:
            errors.append(f"[Gate 1] #{num} 与 #{seen_paths[path]} 重复 upstream_path: {path}")
        else:
            seen_paths[path] = num

        # 能力 ID 必须在 Catalog 中已声明（始终是硬错误）
        for cap_id in entry.get("required_capabilities", []):
            if cap_id not in known_caps:
                errors.append(f"[Gate 1] #{num} ({eid}) 引用未声明的能力: {cap_id}")

        # 隔离区与 Verified 证据核验
        if is_v2:
            executions = entry.get("executions", [])
            if not executions:
                errors.append(f"[Gate 1] #{num} ({eid}) executions 数组不能为空")
                continue

            for ex in executions:
                if ex.get("delivery_state") == "verified":
                    evidence = ex.get("evidence")
                    is_quarantined = eid in quarantine

                    if is_quarantined:
                        q_item = quarantine[eid]
                        exp_str = q_item.get("grace_period_expires")
                        exp_dt = datetime.fromisoformat(exp_str.replace("Z", "+00:00"))
                        if now_utc > exp_dt:
                            errors.append(f"[Gate 1] #{num} ({eid}) 存量隔离区 TTL 已于 {exp_str} 逾期！必须提交 PR 回退为 planned。")
                        else:
                            warnings.append(f"[Gate 1] #{num} ({eid}) 处于存量债务隔离区白名单中（TTL 至 {exp_str}）")
                    else:
                        if not evidence or not isinstance(evidence, dict):
                            errors.append(f"[Gate 1] #{num} ({eid}) delivery_state=verified 但 evidence 缺失或为 null")
                        else:
                            sha = evidence.get("assets_sha256")
                            if not sha or sha == "0000000000000000000000000000000000000000000000000000000000000000":
                                msg = f"[Gate 1] #{num} ({eid}) delivery_state=verified 但未在隔离区且 assets_sha256 无效"
                                if strict:
                                    errors.append(msg)
                                else:
                                    warnings.append(msg)
        else:
            # v1.1 兼容分支
            delivery = entry.get("delivery", {})
            if delivery.get("state") == "verified":
                sha = delivery.get("assets_sha256") or {}
                if not all(sha.get(k) for k in ["device_tree", "js", "wasm"]):
                    msg = f"[Gate 1] #{num} ({eid}) delivery.state=verified 但 assets_sha256 三件套未填写"
                    if strict:
                        errors.append(msg)
                    else:
                        warnings.append(msg)

    return errors, warnings


# ─────────────────────────────────────────────────────────────
# 渲染单个条目行
# ─────────────────────────────────────────────────────────────
def render_row(entry: dict, quarantine: dict[str, dict]) -> tuple[str, str]:
    num      = entry.get("display_id", "?")
    upstream = entry.get("upstream_path", "?").removeprefix("examples/")
    eid      = entry.get("id", "")
    is_v2    = entry.get("written_at_spec_version", "").startswith("2.") or "scope" in entry

    if is_v2:
        scope_obj  = entry.get("scope", {})
        inclusion  = scope_obj.get("inclusion", "unknown")
        schedule   = scope_obj.get("schedule", "active")
        executions = entry.get("executions", [{}])
        ex0        = executions[0] if executions else {}
        delivery   = ex0.get("delivery_state", "planned")
        acceptance = ex0.get("acceptance", {})
        obs_raw    = acceptance.get("observability_level", "L4_internal")
        obs_str    = OBS_EMOJI.get(obs_raw, obs_raw)
        pos_cases  = acceptance.get("positive_cases", [])
        evidence   = ex0.get("evidence")

        app_dir = "待适配"
        if eid in quarantine:
            app_dir = f"esp_idfv61_{Path(upstream).name}"

        # 状态符与描述裁判
        if eid in quarantine:
            symbol = "[?]"
            desc = "待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13)"
            pri_hint = "P1"
            metric_tag = "quarantined"
        elif inclusion == "out_of_scope":
            symbol = "[-]"
            reason = scope_obj.get("exclusion_reason") or "物理硬件介质"
            desc = f"声明 Out-of-Scope。{reason}。"
            pri_hint = "P4"
            metric_tag = "out_of_scope"
        elif delivery == "verified" and evidence and evidence.get("assets_sha256") and evidence["assets_sha256"] != "0" * 64:
            symbol = "[x]"
            desc = "已完成实证。"
            pri_hint = "P0"
            metric_tag = "verified"
        elif schedule == "deferred":
            symbol = "[-]"
            reason = scope_obj.get("exclusion_reason") or "依赖外部模型"
            desc = f"暂缓投入。{reason}。"
            pri_hint = "P3"
            metric_tag = "deferred"
        elif inclusion == "unknown":
            symbol = "[ ]"
            desc = "待深度审定。尚未完成源码调用链与硬件特性审定。"
            pri_hint = "P2"
            metric_tag = "pending_audit"
        else:
            symbol = DELIVERY_STATE_SYMBOL.get(delivery, "[ ]")
            desc = pos_cases[0].get("name", "待排期。") if pos_cases else "待排期。依赖进一步框架门面扩展。"
            pri_hint = "P1"
            metric_tag = "planned"
    else:
        # v1.1 渲染分支
        scope    = entry.get("scope_and_maturity", {}).get("status", "pending_audit")
        delivery = entry.get("delivery", {}).get("state", "planned")
        obs_raw  = entry.get("acceptance", {}).get("observability_level", "L4_internal")
        obs_str  = OBS_EMOJI.get(obs_raw, obs_raw)
        pos_cases = entry.get("acceptance", {}).get("positive_cases", [])

        if eid in quarantine:
            symbol = "[?]"
            desc = "待补凭证 (存量隔离区债务，14天 TTL)"
            metric_tag = "quarantined"
        elif scope in ("out_of_scope_product", "contract_blocked", "in_scope_deferred") and delivery != "verified":
            symbol = SCOPE_STATUS_SYMBOL.get(scope, "[ ]")
            metric_tag = "out_of_scope" if scope == "out_of_scope_product" else "deferred"
            desc = "声明 Out-of-Scope。"
        else:
            symbol = DELIVERY_STATE_SYMBOL.get(delivery, "[ ]")
            metric_tag = "verified" if delivery == "verified" else "planned"
            desc = pos_cases[0].get("name", "待排期。") if pos_cases else "待排期。"

        caps     = entry.get("required_capabilities", [])
        pri_hint = "P0" if symbol == "[x]" else ("P4" if symbol == "[-]" else "P1")
        app_dir  = entry.get("delivery", {}).get("app_dir") or "待适配"

    # 截断超长描述
    if len(desc) > 80:
        desc = desc[:77] + "..."

    row_str = f"| {symbol} | {num:03d} | `{upstream}` | {obs_str} | {pri_hint} | `{app_dir}` | {desc} |"
    return row_str, metric_tag


# ─────────────────────────────────────────────────────────────
# 主渲染函数
# ─────────────────────────────────────────────────────────────
def render_checklist(manifest: dict, quarantine: dict[str, dict]) -> str:
    entries  = manifest["entries"]
    total    = manifest.get("total_entries", len(entries))
    spec_ver = manifest.get("spec_version", "2.0.0")
    gen_date = datetime.now(timezone.utc).strftime("%Y-%m-%d")

    rendered_rows: list[tuple[str, str, int]] = []
    metric_counts = {
        "verified": 0,
        "quarantined": 0,
        "planned": 0,
        "out_of_scope": 0,
        "deferred": 0,
        "pending_audit": 0
    }

    for e in entries:
        row, tag = render_row(e, quarantine)
        rendered_rows.append((row, tag, e["display_id"]))
        metric_counts[tag] = metric_counts.get(tag, 0) + 1

    lines: list[str] = []

    # ── 文件头 ───────────────────────────────────────────────
    lines += [
        "<!-- SPDX-License-Identifier: Apache-2.0 -->",
        "<!-- ⚠️  此文件由 generate_checklist_v1_1.py 自动生成，严禁人工直接编辑！修改请编辑 checklist.data.json -->",
        "# ESP-IDF v6.1 官方示例全量仿真适配核对清单 (Checklist)",
        "",
        f"> **数据单一事实源（SSOT）**：[`checklist.data.json`](checklist.data.json)（Spec v{spec_ver}，多配置实例与五维正交模型）  ",
        f"> **生成时间**：{gen_date}  ",
        "> **分类规范**：[`CLASSIFICATION-SPEC.md`](CLASSIFICATION-SPEC.md) (v2.0)  ",
        "> **能力字典**：[`capability-catalog.yaml`](capability-catalog.yaml)  ",
        "> **隔离区白名单**：[`.gates/quarantine.yaml`](.gates/quarantine.yaml)（10 项存量债务，14 天 TTL 生效中）  ",
        "> **执行手册**：[`PLAYBOOK.md`](PLAYBOOK.md) (v2.0)  ",
        "",
        "---",
        "",
        "## 一、 总体适配进度统计",
        "",
        f"- **官方独立示例总数**：**{total} 个**",
        f"  - `[x]` **已完成六要素实证 (Verified)**：**{metric_counts['verified']} 项**",
        f"  - `[?]` **存量隔离待补凭证 (Quarantined Debt)**：**{metric_counts['quarantined']} 项**（14 天 TTL 过期硬阻断，至 `2026-10-13`）",
        f"  - `[ ]` **规划中正常排期 (In-Scope Planned)**：**{metric_counts['planned']} 项**",
        f"  - `[-]` **明确产品排除 / 暂缓投入 (Out-of-Scope / Deferred)**：**{metric_counts['out_of_scope'] + metric_counts['deferred']} 项**（编译期 `WINK_SLA_ERROR` Fail-Loud 阻断）",
        f"  - `?` **待深度审定 (Pending Audit / Unknown Scope)**：**{metric_counts['pending_audit']} 项**",
        "",
    ]

    # ── 大类导航索引 ─────────────────────────────────────────
    lines += [
        "### 大类索引导航",
        "",
        "| 序号 | 功能大类 | 包含示例数 | 编号跨度 | 已实证 | 隔离待补 |",
        "| :---: | :--- | :---: | :---: | :---: | :---: |",
    ]
    for idx, (lo, hi, anchor, title, _) in enumerate(CATEGORY_GROUPS, 1):
        group_rows = [r for r in rendered_rows if lo <= r[2] <= hi]
        cnt_v = sum(1 for r in group_rows if r[1] == "verified")
        cnt_q = sum(1 for r in group_rows if r[1] == "quarantined")
        lines.append(f"| {idx:02d} | [{title}](#{anchor}) | {len(group_rows)} 项 | `#{lo:03d} ~ #{hi:03d}` | {cnt_v} 项 | {cnt_q} 项 |")
    lines += ["", "---", ""]

    # ── 符号说明 ─────────────────────────────────────────────
    lines += [
        "## 二、 符号与分类说明",
        "",
        "- `[x]` **已完成实证 (Verified)**：完全满足六要素合取公式（范围有效、审计覆盖、依赖闭包满足、声明验证、防伪哈希匹配、断言全过）。",
        "- `[?]` **存量隔离待补凭证 (Quarantined)**：早期存量打样条目，已入 `.gates/quarantine.yaml` 隔离区白名单，14 天 TTL 内待补齐哈希与执行凭据。",
        "- `[ ]` **待适配 / 待审定 (Planned / Pending)**：在规划范围内待排期，或处于初始抓取状态待进一步核验源码。",
        "- `[-]` **声明 Out-of-Scope / 暂缓投入**：不可逆物理介质在编译期通过 `WINK_SLA_ERROR` Fail-Loud 显式阻断；或暂缓投入。",
        "- `[~]` **正在构建 (Building)**：WIP 开发中，尚未产出完整自动化测试实证。",
        "- `[!]` **回归失败 / 凭据陈旧 (Regressed / Stale)**：断言失败或工作区资产与登记哈希不一致，严禁打勾。",
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
        "| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |\n"
        "| :---: | :---: | :--- | :---: | :---: | :--- | :--- |"
    )

    for lo, hi, anchor, title, _ in CATEGORY_GROUPS:
        group_rows = [r for r in rendered_rows if lo <= r[2] <= hi]
        cnt_v = sum(1 for r in group_rows if r[1] == "verified")
        cnt_q = sum(1 for r in group_rows if r[1] == "quarantined")
        lines += [
            f'<a id="{anchor}"></a>',
            f"### {title}（共 {len(group_rows)} 项 | 编号 `#{lo:03d} ~ #{hi:03d}` | 已实证: {cnt_v} 项 | 隔离待补: {cnt_q} 项）",
            "",
            TABLE_HEADER,
        ]
        for r in group_rows:
            lines.append(r[0])
        lines += ["", "---", ""]

    return "\n".join(lines)


# ─────────────────────────────────────────────────────────────
# 主函数
# ─────────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Generate CHECKLIST.md from checklist.data.json (Schema v2.0)")
    parser.add_argument("--dry-run", action="store_true", help="输出到 stdout，不写文件")
    parser.add_argument("--skip-validate", action="store_true", help="跳过前置数据校验")
    parser.add_argument("--strict", action="store_true", help="严格模式校验")
    args = parser.parse_args()

    # 1. 读取数据源
    print(f"[generate] 读取 {DATA_JSON} ...")
    with open(DATA_JSON, encoding="utf-8") as f:
        manifest = json.load(f)
    print(f"[generate] 共 {manifest['total_entries']} 条条目 (Spec v{manifest.get('spec_version')})")

    # 2. 读取 Catalog
    print(f"[generate] 读取 {CATALOG_YAML} ...")
    with open(CATALOG_YAML, encoding="utf-8") as f:
        catalog = yaml.safe_load(f)

    # 3. 读取 Quarantine
    quarantine = load_quarantine()
    if quarantine:
        print(f"[generate] 载入存量隔离区白名单: {len(quarantine)} 项条目挂载中")

    # 4. Gate 1 数据校验
    if not args.skip_validate:
        print(f"[generate] 执行 Gate 1 前置数据校验 {'(strict)' if args.strict else '(pure-function)'}...")
        errors, warnings = validate_data(manifest, catalog, quarantine, strict=args.strict)
        if warnings:
            print(f"[Gate 1] WARN - {len(warnings)} 条警告:")
            for w in warnings[:5]:
                print(f"  {w}")
            if len(warnings) > 5:
                print(f"  ... 及另外 {len(warnings)-5} 条")
        if errors:
            print(f"[Gate 1] FAIL - 校验失败，{len(errors)} 条硬错误：")
            for e in errors:
                print(f"  {e}")
            sys.exit(1)
        print(f"[Gate 1] PASS - 校验通过 ({len(manifest['entries'])} 条，0 错误，{len(warnings)} 条受控隔离警告)")

    # 5. 渲染
    print(f"[generate] 渲染 CHECKLIST.md ...")
    md_content = render_checklist(manifest, quarantine)

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
