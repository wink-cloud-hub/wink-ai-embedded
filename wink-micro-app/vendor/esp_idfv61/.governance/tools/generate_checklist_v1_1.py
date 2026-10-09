#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# -*- coding: utf-8 -*-
"""
generate_checklist_v1_1.py
==========================
从 checklist.data.json 单向渲染生成 CHECKLIST.md（只读派生看板）。
支持 Schema v2.0，对齐 ADR-0090、ADR-0091 与纯函数门禁规范。

依据 2026-09-29-esp-idf-gate-system-implementation-plan.md：
数据合规性校验已彻底由 .gates/run_gates.py (Gate 1 全量规则) 统一接管，
本脚本仅保留向后兼容的代理验证入口及单向只读 Markdown 渲染职责。

用法:
    python generate_checklist_v1_1.py           # 重新生成 CHECKLIST.md
    python generate_checklist_v1_1.py --dry-run  # 输出到 stdout，不写文件
    python generate_checklist_v1_1.py --skip-validate # 跳过 Gate 1 校验直接渲染

铁律四：本脚本是唯一合法的 CHECKLIST.md 写入路径，严禁人工直接编辑 CHECKLIST.md。
"""

import json
import sys
import argparse
import subprocess
from pathlib import Path
from datetime import datetime, timezone

import yaml

SCRIPT_DIR   = Path(__file__).resolve().parent
GOV_DIR      = SCRIPT_DIR.parent if SCRIPT_DIR.name == "tools" else SCRIPT_DIR
DATA_JSON    = GOV_DIR / "data" / "checklist.data.json" if (GOV_DIR / "data" / "checklist.data.json").exists() else GOV_DIR / "checklist.data.json"
CATALOG_YAML = GOV_DIR / "catalog" / "capability-catalog.yaml" if (GOV_DIR / "catalog" / "capability-catalog.yaml").exists() else GOV_DIR / "capability-catalog.yaml"
QUARANTINE_YAML = (
    GOV_DIR / "gates" / "quarantine.yaml"
    if (GOV_DIR / "gates" / "quarantine.yaml").exists()
    else (GOV_DIR / "quarantine.yaml" if (GOV_DIR / "quarantine.yaml").exists() else GOV_DIR / ".gates" / "quarantine.yaml")
)
OUTPUT_MD    = GOV_DIR.parent / "CHECKLIST.md"
WS_ROOT      = SCRIPT_DIR.parents[4]

GATES_DIR = GOV_DIR / "gates"
if str(GATES_DIR) not in sys.path:
    sys.path.insert(0, str(GATES_DIR))
from twin_evidence import verify_twin_evidence

try:
    from evidence_verifier import verify_evidence
except ImportError:
    GATES_DIR = GOV_DIR / "gates"
    if str(GATES_DIR) not in sys.path:
        sys.path.insert(0, str(GATES_DIR))
    from evidence_verifier import verify_evidence

# ─────────────────────────────────────────────────────────────
# 状态符渲染映射
# ─────────────────────────────────────────────────────────────
DELIVERY_STATE_SYMBOL = {
    "verified":      "[x]",
    "verified_v1_1": "[x]",
    "building":      "[~]",
    "regressed":     "[!]",
    "stale":         "[!]",
    "planned":       "[ ]",
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
# 前置数据校验（过渡逻辑已退休，彻底由 run_gates.py Gate 1 统一接管）
# ─────────────────────────────────────────────────────────────
def validate_data(manifest: dict = None, catalog: dict = None, quarantine: dict = None, strict: bool = False) -> tuple[list[str], list[str]]:
    """
    [过渡逻辑已退休]
    依据 2026-09-29-esp-idf-gate-system-implementation-plan.md，
    数据合规性校验已彻底移交 .gates/run_gates.py (Gate 1 全量 10 条规则)。
    保留此函数以便向后兼容，内部直接委托 run_gates.py Gate 1 校验。
    """
    run_gates_script = GOV_DIR / "gates" / "run_gates.py"
    cmd = [
        sys.executable,
        str(run_gates_script),
        "--gate", "1",
        "--mode", "nightly" if strict else "pr",
        "--allow-empty-diff",
    ]
    res = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if res.returncode != 0:
        return [f"Gate 1 门禁校验失败 (退出码 {res.returncode}):\n{res.stdout}\n{res.stderr}"], []
    return [], []

# ─────────────────────────────────────────────────────────────
# 六大正交并发研发泳道与优先级阶梯推导逻辑
# ─────────────────────────────────────────────────────────────
def classify_example_lane_and_priority(
    upstream: str,
    inclusion: str,
    schedule: str,
    has_verified: bool,
    valid_evidence: bool,
) -> tuple[str, str, str]:
    """
    推导示例的并发泳道与优先级阶梯
    返回: (lane_id, priority_tier, lane_tag)
    """
    cat = upstream.split("/")[0]
    sub = upstream.split("/")[1] if "/" in upstream else ""

    # 1. 物理不可逆排除与暂缓投入
    if inclusion == "out_of_scope":
        return ("lane_7_hardware", "P4", "[Lane 7: 硬件排除]")
    if schedule == "deferred":
        return ("lane_7_hardware", "P3", "[Lane 7: 暂缓投入]")

    # 2. 正交并发泳道归属
    if cat in ("get-started", "system", "cxx"):
        lane_id = "lane_1_core"
        lane_tag = "[Lane 1: 内核调度]"
    elif cat == "storage":
        lane_id = "lane_5_storage"
        lane_tag = "[Lane 5: 本地存储]"
    elif cat in ("wifi", "protocols", "bluetooth", "network"):
        lane_id = "lane_6_net_rf"
        lane_tag = "[Lane 6: 无线网络]"
    elif cat == "peripherals":
        if sub in ("ledc", "timer_group", "rmt", "mcpwm", "pcnt", "sigma_delta"):
            lane_id = "lane_3_pulse"
            lane_tag = "[Lane 3: 脉冲定时]"
        elif sub in ("adc", "dac", "analog_comparator", "touch_sensor", "temperature_sensor"):
            lane_id = "lane_4_analog"
            lane_tag = "[Lane 4: 模拟电学]"
        else:
            lane_id = "lane_2_bus"
            lane_tag = "[Lane 2: 数字总线]"
    else:
        lane_id = "lane_7_hardware"
        lane_tag = "[Lane 7: 硬件排除]"

    # 3. 优先级阶梯划分 (P0 核心标杆 -> P1 通用积木 -> P2 进阶多通道 -> P3 复杂长尾)
    if has_verified and valid_evidence:
        return (lane_id, "P0", lane_tag)

    P0_TARGETS = {
        "get-started/blink",
        "peripherals/i2c/i2c_basic",
        "peripherals/uart/uart_echo",
        "peripherals/ledc/ledc_basic",
        "peripherals/timer_group/gptimer",
        "peripherals/adc/oneshot_read",
        "storage/nvs/nvs_rw_value",
        "wifi/getting_started/station",
    }
    if upstream in P0_TARGETS:
        return (lane_id, "P0", lane_tag)

    P1_KEYWORDS = (
        "spi_master", "generic_gpio", "gpio", "oneshot", "cosine", "nvs_rw",
        "http_client", "mqtt", "bleprph", "basic", "echo", "alarm", "freertos",
        "hello_world"
    )
    if any(k in upstream for k in P1_KEYWORDS):
        return (lane_id, "P1", lane_tag)

    P3_KEYWORDS = ("server", "camera", "lcd", "usb", "ulp", "bitscrambler", "h264", "jpeg")
    if any(k in upstream for k in P3_KEYWORDS):
        return (lane_id, "P3", lane_tag)

    return (lane_id, "P2", lane_tag)


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
        executions = entry.get("executions", [])
        has_verified = False
        has_twin_evidence = False
        has_regressed = False
        valid_evidence = False
        pos_cases = []
        obs_raw = "L4_internal"

        for ex in executions:
            acceptance = ex.get("acceptance", {})
            if acceptance.get("observability_level"):
                obs_raw = acceptance.get("observability_level")
            if acceptance.get("positive_cases"):
                pos_cases = acceptance.get("positive_cases")

            d_state = ex.get("delivery_state", "planned")
            if d_state in ("verified", "verified_v1_1"):
                has_verified = True
                ok, _ = verify_evidence(entry, ex, WS_ROOT, strict_disk=True)
                if ok:
                    valid_evidence = True
                    twin_ok, _ = verify_twin_evidence(entry, ex, WS_ROOT)
                    has_twin_evidence = has_twin_evidence or twin_ok
                else:
                    has_regressed = True

        obs_str = OBS_EMOJI.get(obs_raw, obs_raw)

        target_dir = entry.get("target_app_dir") or ""
        app_exists = (OUTPUT_MD.parent / target_dir / "wink-app.json").is_file() if target_dir else False

        if app_exists:
            app_col = f"[`{target_dir}`]({target_dir})"
        elif eid in quarantine:
            app_col = f"`{target_dir or ('esp_idfv61_' + Path(upstream).name)}`"
        elif inclusion == "out_of_scope":
            app_col = "—"
        elif target_dir:
            app_col = f"`{target_dir}`"
        else:
            app_col = "待适配"

        # 推导并发泳道与优先级
        lane_id, pri_tier, lane_tag = classify_example_lane_and_priority(
            upstream, inclusion, schedule, has_verified, valid_evidence
        )

        # 状态符与描述裁判
        if eid in quarantine:
            symbol = "[?]"
            desc = f"{lane_tag} 待补凭证 (存量隔离区债务，14天 TTL 至 2026-10-13)"
            pri_hint = "P1"
            metric_tag = "quarantined"
        elif inclusion == "out_of_scope":
            symbol = "[-]"
            reason = scope_obj.get("exclusion_reason") or "物理硬件介质"
            desc = f"声明 Out-of-Scope。{reason}。"
            pri_hint = "P4"
            metric_tag = "out_of_scope"
        elif has_verified and valid_evidence:
            symbol = "[x]"
            if has_twin_evidence:
                desc = f"{lane_tag} 🟢 [Green ✅ | Red 🛡️] 已完成红绿双实证 (TWIN-PROOF)。"
                metric_tag = "verified_twin"
            else:
                desc = f"{lane_tag} [Green ✅ | Red ⏳] 已完成实证。"
                metric_tag = "verified"
            pri_hint = "P0"
        elif has_regressed:
            symbol = "[!]"
            desc = f"{lane_tag} 实证凭据核验未通过，需重新回归。"
            pri_hint = "P0"
            metric_tag = "regressed"
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
            symbol = "[ ]"
            case_name = pos_cases[0].get("name") if pos_cases else None
            desc = f"{lane_tag} {case_name}" if case_name else f"{lane_tag} 待排期。依赖进一步框架门面扩展。"
            pri_hint = pri_tier
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
        target_dir = entry.get("target_app_dir") or entry.get("delivery", {}).get("app_dir") or ""
        app_exists = (OUTPUT_MD.parent / target_dir / "wink-app.json").is_file() if target_dir else False
        if app_exists:
            app_col = f"[`{target_dir}`]({target_dir})"
        elif target_dir:
            app_col = f"`{target_dir}`"
        else:
            app_col = "待适配"

    # 截断超长描述
    if len(desc) > 80:
        desc = desc[:77] + "..."

    row_str = f"| {symbol} | {num:03d} | `{upstream}` | {obs_str} | {pri_hint} | {app_col} | {desc} |"
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
        "verified_twin": 0,
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

    total_verified = metric_counts["verified"] + metric_counts["verified_twin"]
    lines: list[str] = []

    # ── 文件头 ───────────────────────────────────────────────
    lines += [
        "<!-- SPDX-License-Identifier: Apache-2.0 -->",
        "<!-- ⚠️  此文件由 generate_checklist_v1_1.py 自动生成，严禁人工直接编辑！修改请编辑 checklist.data.json -->",
        "# ESP-IDF v6.1 官方示例全量仿真适配核对清单 (Checklist)",
        "",
        f"> **数据单一事实源（SSOT）**：[`checklist.data.json`](.governance/data/checklist.data.json)（Spec v{spec_ver}，多配置实例与五维正交模型）  ",
        f"> **生成时间**：{gen_date}  ",
        "> **分类规范**：[`CLASSIFICATION-SPEC.md`](.governance/specs/CLASSIFICATION-SPEC.md) (v2.0)  ",
        "> **能力字典**：[`capability-catalog.yaml`](.governance/catalog/capability-catalog.yaml)  ",
        "> **隔离区白名单**：[`gates/quarantine.yaml`](.governance/gates/quarantine.yaml)（10 项存量债务，14 天 TTL 生效中）  ",
        "> **执行手册**：[`PLAYBOOK.md`](.governance/specs/PLAYBOOK.md) (v2.0)  ",
        "",
        "---",
        "",
        "## 一、 总体适配进度统计",
        "",
        f"- **官方独立示例总数**：**{total} 个**",
        f"  - `[x]` **已完成六要素实证 (Verified)**：**{total_verified} 项**",
        f"    - `🟢` **双实证闭环 (Twin-Proof: Green ✅ + Red 🛡️)**：**{metric_counts['verified_twin']} 项**",
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
        cnt_v = sum(1 for r in group_rows if r[1] in ("verified", "verified_twin"))
        cnt_q = sum(1 for r in group_rows if r[1] == "quarantined")
        lines.append(f"| {idx:02d} | [{title}](#{anchor}) | {len(group_rows)} 项 | `#{lo:03d} ~ #{hi:03d}` | {cnt_v} 项 | {cnt_q} 项 |")
    lines += ["", "---", ""]

    # ── 并发研发泳道与优先级调度矩阵 ──────────────────────────────
    lines += [
        "### 并发研发泳道与优先级调度矩阵 (Concurrency Lanes & Execution Matrix)",
        "",
        "> **并发编排说明**：为支持后续多个实施计划与 AI Coding Agents **安全并发推进**，478 个官方示例划分为 **6 条正交并发研发泳道**。各泳道在运行时门面（`wink-micro-os/frameworks/esp_idf/`）与外设驱动上物理隔离，支持并行认领开发，杜绝底座代码冲突。",
        "",
        "| 泳道代号与技术领域 | 覆盖示例数 (Active) | 核心标杆 (P0) | 通用积木 (P1) | 进阶模式 (P2) | 复杂生态 (P3) | 关键底座依赖 | 并发隔离与协作建议 |",
        "| :--- | :---: | :---: | :---: | :---: | :---: | :--- | :--- |",
        "| **Lane 1: 核心系统与内核调度** | 70 项 | 2 项 | 11 项 | 54 项 | 3 项 | FreeRTOS 纤程调度器、代际令牌 | 底座筑基泳道，优先收敛核心语义；其他泳道的前提 |",
        "| **Lane 2: 通用数字总线与通信** | 46 项 | 2 项 | 12 项 | 24 项 | 8 项 | `pal_i2c`, `pal_spi`, `pal_uart` | 纯外设模型，与 Lane 3~6 物理正交，可独立并行推进 |",
        "| **Lane 3: 脉冲发生与硬件定时器** | 23 项 | 2 项 | 3 项 | 15 项 | 3 项 | 定点 PWM、虚拟微秒因果推进 | 纯波形与硬件定时，与 Lane 2, 4~6 独立并行推进 |",
        "| **Lane 4: 模拟量转换与电学传感** | 12 项 | 1 项 | 4 项 | 7 项 | 0 项 | `pal_adc`, `pal_dac` | 模拟电学采样，与通信总线及网络完全正交，可独立推进 |",
        "| **Lane 5: 本地存储与虚拟文件系统**| 27 项 | 1 项 | 4 项 | 20 项 | 2 项 | 内存虚拟块设备、VFS 句柄抽象 | 纯内存/虚拟块，与外设及网络零耦合，可完全独立推进 |",
        "| **Lane 6: 无线网络与通信协议栈**| 113 项 | 1 项 | 3 项 | 95 项 | 14 项 | 确定性虚拟空口、Netif、NimBLE | 虚拟网络 Broker 闭环，与 Lane 2~5 独立并行推进 |",
        "",
        "#### 🎯 各泳道推荐优先并发认领就绪清单 (Ready-to-Claim P0/P1 Backlog)",
        "",
        "| 泳道 | 编号 | 官方子示例相对路径 | 优先级 | 对应 App 目录 | 推荐理由与解锁价值 |",
        "| :--- | :---: | :--- | :---: | :--- | :--- |",
        "| **Lane 1: 系统** | `#001` | `get-started/blink` | P0 | [`get-started/blink_gpio`](get-started/blink_gpio) | ✅ 已完成实证。建立最小 FreeRTOS 任务与 GPIO 输出范式 |",
        "| **Lane 1: 系统** | `#002` | `get-started/hello_world` | P1 | `get-started/hello_world` | 解锁系统控制台输出与基础芯片信息获取 |",
        "| **Lane 2: 总线** | `#019` | `peripherals/i2c/i2c_basic` | P0 | `peripherals/i2c_basic` | 解锁对象式 I2C Master 总线与传感器寄存器通信 |",
        "| **Lane 2: 总线** | `#108` | `peripherals/uart/uart_echo` | P0 | `peripherals/uart_echo` | 解锁双任务环形缓冲与串口交互终端 |",
        "| **Lane 2: 总线** | `#088` | `peripherals/spi_master/hd_eeprom` | P1 | `peripherals/spi_eeprom` | 解锁高速 SPI 总线全双工读写支持 |",
        "| **Lane 3: 脉冲** | `#047` | `peripherals/ledc/ledc_basic` | P0 | `peripherals/ledc_basic` | 解锁定点 PWM 占空比无浮点呼吸调光 |",
        "| **Lane 3: 脉冲** | `#096` | `peripherals/timer_group/gptimer` | P0 | `peripherals/gptimer_alarm` | 解锁高精度硬件定时器 Alarm 与中断回调 |",
        "| **Lane 4: 模拟** | `#004` | `peripherals/adc/oneshot_read` | P0 | `peripherals/adc_oneshot` | 解锁电压校准与多通道电位器模拟采样 |",
        "| **Lane 4: 模拟** | `#015` | `peripherals/dac/dac_cosine` | P1 | `peripherals/dac_cosine` | 解锁 DAC 连续余弦波音频发生 |",
        "| **Lane 5: 存储** | `#395` | `storage/nvs/nvs_rw_value` | P0 | `storage/nvs_rw_value` | 解锁键值对非易失性持久化，为 Wi-Fi 凭证打底 |",
        "| **Lane 5: 存储** | `#403` | `storage/spiffs/spiffs` | P1 | `storage/spiffs` | 解锁片上文件系统与文件读写接口 |",
        "| **Lane 6: 网络** | `#220` | `wifi/getting_started/station` | P0 | `wifi/wifi_sta` | 解锁虚拟 AP 状态机与 DHCP 虚拟 IP 分配 |",
        "| **Lane 6: 网络** | `#185` | `protocols/esp_http_client` | P1 | `protocols/http_client` | 解锁 REST GET/POST 网络通信客户端 |",
        "| **Lane 6: 网络** | `#186` | `protocols/mqtt/tcp` | P1 | `protocols/mqtt_tcp` | 解锁轻量 Broker Pub/Sub 实时闭环 |",
        "| **Lane 6: 蓝牙** | `#245` | `bluetooth/nimble/bleprph` | P1 | `bluetooth/bleprph` | 解锁 NimBLE GATT 特征值读写与 Virtual Inspector |",
        "",
        "---",
        "",
    ]

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
        "### 优先级与泳道编排说明",
        "",
        "- **P0（核心筑基标杆）**：已完成验证（`[x]`）或该泳道最核心的基础设施标杆，解锁该泳道后续一切前置依赖。",
        "- **P1（高频通用积木）**：覆盖通用业务场景中约 70% 的核心功能（如 SPI 全双工、GPTimer 报警、NVS 键值存储、HTTP GET/POST）。",
        "- **P2（进阶模式与多通道）**：ADC 连续采样 DMA、LEDC 多通道平滑呼吸、SoftAP + STA 级联、BLE Central 多连接。",
        "- **P3（复杂组合与长尾）**：WebSocket 服务端长连接、mDNS 组播、复杂总线级联传感器、暂缓投入项。",
        "- **P4（硬件约束排除）**：不可逆物理介质（eFuse、外部 PHY 以太网、空间 RF 测试等），编译期显式阻断。",
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
        cnt_v = sum(1 for r in group_rows if r[1] in ("verified", "verified_twin"))
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

    # 4. Gate 1 数据校验（由 .gates/run_gates.py 统一接管）
    if not args.skip_validate:
        print(f"[generate] 执行 Gate 1 前置数据校验 (委托 .gates/run_gates.py Gate 1)...")
        errors, warnings = validate_data(strict=args.strict)
        if errors:
            print(f"[Gate 1] FAIL - 校验失败：")
            for e in errors:
                print(f"  {e}")
            sys.exit(1)
        print(f"[Gate 1] PASS - Gate 1 校验通过，继续单向渲染 CHECKLIST.md。")

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
    # Configure CLI encoding without replacing/closing a caller's streams.
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    main()
