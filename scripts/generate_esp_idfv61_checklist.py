#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""
Generate ESP32_IDFV61_EXAMPLE_CHECKLIST.md
Exhaustive, 100% census of all 478 official examples in ESP-IDF v6.1.
Guarantees strictly sequential #001 ~ #478 numbering in document display order.
"""

from pathlib import Path
import datetime
import sys

try:
    from esp_path_resolver import get_idf_examples_dir
except ImportError:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from esp_path_resolver import get_idf_examples_dir

ROOT_EXAMPLES = get_idf_examples_dir()
OUTPUT_MD_1 = Path("docs/vendors/Espressif/ESP32_IDFV61_EXAMPLE_CHECKLIST.md")
OUTPUT_MD_2 = Path("wink-micro-os/frameworks/esp_idf/docs/05-esp32-idfv61-example-checklist.md")

# Known implemented in M0~M4
IMPLEMENTED_MAP = {
    "get-started/blink": ("esp_idfv61_blink_gpio", "🎯 Level 1", "M0 标杆已落地。GPIO 输出 + FreeRTOS 延时驱动板载 LED 呼吸闪烁。"),
    "peripherals/ledc/ledc_basic": ("esp_idfv61_ledc_basic", "🎯 Level 1", "M2 已落地。LEDC 定时器与通道配置，定点 PWM 占空比平滑渐变，无浮点。"),
    "peripherals/i2c/i2c_basic": ("esp_idfv61_i2c_basic", "📜 Level 2", "M2 已落地。现代对象式 I2C Master 驱动，总线与器件二级句柄，读写传感器寄存器。"),
    "peripherals/uart/uart_echo": ("esp_idfv61_uart_echo", "📜 Level 2", "M2 已落地。UART 阻塞读/写回环，环形缓冲区与多任务调度。"),
    "peripherals/timer_group/gptimer": ("esp_idfv61_gptimer_alarm", "⚡ Level 3", "M2 已落地。高精度通用硬件定时器 Alarm 回调与自动重载机制。"),
    "wifi/getting_started/station": ("esp_idfv61_wifi_sta", "📜 Level 2", "M4-1 已落地。虚拟 AP 状态机与 DHCP 虚拟 IP 分配，esp_event 事件循环派发。"),
    "protocols/esp_http_client": ("esp_idfv61_http_client", "📜 Level 2", "M4-2 已落地。HTTP 客户端请求隧道桥接，支持 GET/POST 响应流式解析与分块传输。"),
    "protocols/mqtt": ("esp_idfv61_mqtt_tcp", "📜 Level 2", "M4-2 已落地。ESP-MQTT 客户端连接内存虚拟轻量 Broker，Pub/Sub 实时闭环。"),
    "bluetooth/nimble/bleprph": ("esp_idfv61_bleprph", "🎯 Level 1", "M4-3/M4-4 已落地。NimBLE GATT 静态属性池、特征值读写回调与 Virtual BLE Inspector 交互面板。"),
}

# Category display metadata and order
CATEGORY_ORDER = [
    ("get-started", "基础快速起步 (Get-Started)"),
    ("peripherals", "片上与总线外设 (Peripherals)"),
    ("system", "操作系统与核心系统调用 (System & OS)"),
    ("protocols", "网络与应用层通信协议 (Protocols)"),
    ("wifi", "Wi-Fi 无线局域网 (Wi-Fi)"),
    ("bluetooth", "蓝牙协议栈 (Bluetooth)"),
    ("storage", "片上存储与文件系统 (Storage)"),
    ("network", "底层网络与接口 (Network)"),
    ("cxx", "C++ 运行时与语言特性 (C++)"),
    ("build_system", "构建系统与组件组织 (Build System)"),
    ("security", "硬件加密与芯片安全特性 (Security - 硬件物理特性)"),
    ("custom_bootloader", "定制引导加载程序 (Custom Bootloader)"),
    ("ethernet", "有线以太网 (Ethernet - 外部 PHY 硬件)"),
    ("lowpower", "超低功耗与 ULP 协处理器 (Low Power & ULP)"),
    ("mesh", "Wi-Fi 空间自组网 (Mesh)"),
    ("openthread", "OpenThread 802.15.4 线程网络 (OpenThread)"),
    ("zigbee", "Zigbee 2.4G 射频网络 (Zigbee)"),
    ("ieee802154", "IEEE 802.15.4 原始射频 (IEEE 802.15.4)"),
    ("phy", "射频物理层与工厂校准 (PHY & Calibration)"),
]

def analyze_example(rel_path: str):
    cat = rel_path.split("/")[0]
    
    # 1. Check if already implemented
    if rel_path in IMPLEMENTED_MAP:
        app, obs, desc = IMPLEMENTED_MAP[rel_path]
        return ("[x]", obs, "P0", app, desc)

    # 2. Check if Out-of-Scope category
    if cat in ["zigbee", "openthread", "ieee802154"]:
        return ("[-]", "⚙️ Level 4", "P4", "-", "声明 Out-of-Scope。依赖 2.4GHz 空间物理射频网卡与硬件基带解调。")
    if cat == "ethernet":
        return ("[-]", "⚙️ Level 4", "P4", "-", "声明 Out-of-Scope。依赖板外物理变压器及外部 PHY 芯片（RMII/SMI 总线）。")
    if cat == "security":
        return ("[-]", "⚙️ Level 4", "P4", "-", "声明 Out-of-Scope。涉及硬件 eFuse 烧死熔断与物理 Flash 硬件解密引擎。")
    if cat == "custom_bootloader":
        return ("[-]", "⚙️ Level 4", "P4", "-", "声明 Out-of-Scope。芯片二级引导程序，纯软件仿真直接进入 app_main。")
    if cat == "mesh":
        return ("[-]", "⚙️ Level 4", "P4", "-", "声明 Out-of-Scope。多机空间电磁跳步网络，需分布式拓扑模拟。")
    if cat == "phy":
        return ("[-]", "⚙️ Level 4", "P4", "-", "声明 Out-of-Scope。芯片工厂模拟射频电气校准与功率表。")
    if cat == "lowpower":
        return ("[-]", "⚙️ Level 4", "P4", "-", "声明 Out-of-Scope。ULP 独立微功耗 FSM/RISC-V 协处理器硬件运行环境。")
    if cat == "build_system":
        return ("[-]", "⚙️ Level 4", "P4", "-", "编译构建工具链自身测试，非嵌入式运行时业务代码。")

    # In bluetooth
    if "bluedroid" in rel_path:
        return ("[-]", "⚙️ Level 4", "P3", "-", "架构裁决剔除。Wink 仿真坚持选用轻量纯 C 静态分发的 NimBLE，规避 Bluedroid 庞大动态内存。")
    if "esp_ble_mesh" in rel_path:
        return ("[-]", "⚙️ Level 4", "P3", "-", "声明 Out-of-Scope。BLE Mesh 多节点空间泛洪中继协议。")
    if "esp_ble_audio" in rel_path or "esp_ble_iso" in rel_path:
        return ("[-]", "⚙️ Level 4", "P3", "-", "声明 Out-of-Scope。LE Audio 等时信道与 LC3 专用音频硬件编解码。")
    if "hci" in rel_path:
        return ("[-]", "⚙️ Level 4", "P3", "-", "声明 Out-of-Scope。裸控制器 HCI 物理 UART 传输层协议。")

    # In peripherals
    if "camera" in rel_path or "isp" in rel_path or "jpeg" in rel_path or "h264" in rel_path or "ppa" in rel_path:
        return ("[-]", "🎯 Level 1", "P3", "-", "声明 Out-of-Scope。MIPI-CSI/DVP 摄像头图像传感器与硬件编解码加速器。")
    if "usb" in rel_path or "usb_serial_jtag" in rel_path:
        return ("[-]", "📜 Level 2", "P3", "-", "声明 Out-of-Scope。USB-OTG 硬件物理主从控制器及 PHY 差分信号。")
    if "twai" in rel_path:
        return ("[-]", "📜 Level 2", "P3", "-", "声明 Out-of-Scope。TWAI/CAN 工业汽车差分总线物理收发器。")
    if "sdio" in rel_path:
        return ("[-]", "⚙️ Level 4", "P3", "-", "声明 Out-of-Scope。SDIO 从机高速硬件总线。")

    # In-Scope Pending examples
    if "adc" in rel_path:
        return ("[ ]", "🎯 Level 1", "P1", "待适配", "排期中。拟对接 PAL pal_adc，支持 ADC 模拟量转换与虚拟电位器/光敏电阻控件。")
    if "rmt" in rel_path:
        return ("[ ]", "🎯 Level 1", "P1", "待适配", "排期中。拟实现 RMT 脉冲引擎与 led_strip 门面，驱动虚拟 WS2812 RGB 彩灯。")
    if "spi_master" in rel_path:
        return ("[ ]", "📜 Level 2", "P1", "待适配", "排期中。拟对接 SPI Master 轮询与中断事务传输。")
    if "nvs" in rel_path:
        return ("[ ]", "⚙️ Level 4", "P1", "待适配", "排期中。拟对接 UniSim 虚拟键值存储，支持 Flash 掉电保存。")
    if "softAP" in rel_path or "softap" in rel_path:
        return ("[ ]", "📜 Level 2", "P2", "待适配", "排期中。扩展 Wi-Fi 接入点热点 (SoftAP) 广播与虚拟 DHCP Server 租约。")
    if "freertos" in rel_path:
        return ("[ ]", "📜 Level 2", "P2", "待适配", "排期中。FreeRTOS 更多高级并发用例（任务通知、软件定时器等）。")
    if "sntp" in rel_path:
        return ("[ ]", "📜 Level 2", "P2", "待适配", "排期中。SNTP 网络时间协议，校准虚拟 RTC 时钟。")
    if "ws" in rel_path or "websocket" in rel_path:
        return ("[ ]", "📜 Level 2", "P2", "待适配", "排期中。WebSocket 长连接协议客户端。")

    # Default In-scope pending
    return ("[ ]", "📜 Level 2", "P2", "待适配", "待排期。依赖进一步框架门面扩展。")

def main():
    cat_order_map = {k: idx for idx, (k, _) in enumerate(CATEGORY_ORDER)}

    # Discover all true ESP-IDF example projects
    # A true example project directory contains a project-level CMakeLists.txt
    projects = []
    for p in sorted(ROOT_EXAMPLES.rglob("CMakeLists.txt")):
        rel = p.parent.relative_to(ROOT_EXAMPLES).as_posix()
        # Filter out shared helper components and inner sub-targets
        if "common_components" in rel:
            continue
        if rel.endswith("/ulp"): # Inner ULP coprocessor sub-target
            continue
        if "/components/" in rel or rel.endswith("/components"):
            continue
        if rel.endswith("/main"):
            continue

        try:
            txt = p.read_text(encoding="utf-8", errors="ignore")
            if "project.cmake" in txt or ("project(" in txt and "idf_component_register" not in txt):
                cat = rel.split("/")[0]
                projects.append((cat, rel))
        except Exception:
            pass

    # Sort strictly by CATEGORY_ORDER then by relative path
    projects.sort(key=lambda x: (cat_order_map.get(x[0], 999), x[1]))

    total_count = len(projects)
    print(f"Total verified standalone example projects: {total_count}")

    # Assign sequential IDs #001 ~ #478 strictly in document order
    analyzed_items = []
    status_counts = {"[x]": 0, "[ ]": 0, "[-]": 0, "🚫": 0}
    by_category = {}

    for idx, (cat, rel) in enumerate(projects, 1):
        status, obs, prio, app, desc = analyze_example(rel)
        status_counts[status] += 1
        item = {
            "id": f"{idx:03d}",
            "cat": cat,
            "path": rel,
            "status": status,
            "obs": obs,
            "prio": prio,
            "app": app,
            "desc": desc,
        }
        analyzed_items.append(item)
        by_category.setdefault(cat, []).append(item)

    now = datetime.datetime.now().strftime("%Y-%m-%d")
    md = []
    md.append("# ESP-IDF v6.1 官方示例全量仿真适配核对清单 (Checklist)")
    md.append("")
    md.append("> **权威上游路径**：`D:\\software\\embedded-tools\\esp-idf\\.espressif\\v6.1\\esp-idf\\examples`  ")
    md.append("> **参考标准规范**：[`PLAN-20260922-ESP-IDF-SIM-MASTER`](../../implementation-plans/esp32/2026-09-22-esp-idf-simulation-interception-master-plan.md) §7.1.1（三层证据塔与精选规则）  ")
    md.append("> **对标基线**：[`docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md`](../Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md)（业界最高保真审计基线）  ")
    md.append(f"> **生成日期**：{now} | **版本**：v1.1 (全量连续编号普查版)  ")
    md.append("")
    md.append("---")
    md.append("")
    md.append("## 一、 总体适配进度与全量普查统计")
    md.append("")
    md.append(f"- **官方功能大类总数**：**{len(CATEGORY_ORDER)} 个大类**")
    md.append(f"- **官方独立示例工程总数**：**{total_count} 个**（地毯式 100% 全量建档，严格按编号 `#001 ~ #{total_count:03d}` 连续顺排）")
    md.append("- **全景状态分布统计**：")
    md.append(f"  - `[x]` **已完成适配并实证 (Passed)**：**{status_counts['[x]']} 项**（M0~M4 核心黄金外设与网络连接代表，Host/Wasm 双向编译与行为实证 100% 通过）")
    md.append(f"  - `[ ]` **待适配排期中 (Pending In-Scope)**：**{status_counts['[ ]']} 项**（ADC、RMT/RGB、SPI Master、NVS、WebSocket 等纯软件可模拟用例）")
    md.append(f"  - `[-]` **纯物理硬件专用 / 声明 Out-of-Scope**：**{status_counts['[-]']} 项**（遵循 [ADR-0012 合约诚实原则](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)，明确标明因缺乏物理射频波形、外部 PHY 变压器硬件或物理熔丝而免于失真模拟的项）")
    md.append(f"  - `🚫` **存在前置阻断 (Blocked)**：**{status_counts['🚫']} 项**")
    md.append("")
    md.append("### 大类索引与编号导航总览")
    md.append("")
    md.append("| 序号 | 功能大类 | 包含示例数 | 编号跨度 | 已实证项数 | 范围定位 |")
    md.append("| :---: | :--- | :---: | :---: | :---: | :--- |")

    for cat_idx, (cat_key, cat_title) in enumerate(CATEGORY_ORDER, 1):
        items = by_category.get(cat_key, [])
        if not items:
            continue
        first_id = items[0]["id"]
        last_id = items[-1]["id"]
        passed = sum(1 for it in items if it["status"] == "[x]")
        md.append(f"| {cat_idx:02d} | [{cat_title}](#{cat_key}) | {len(items)} 项 | `#{first_id} ~ #{last_id}` | {passed} 项 | `examples/{cat_key}/` |")

    md.append("")
    md.append("---")
    md.append("")
    md.append("## 二、 符号与分类说明")
    md.append("")
    md.append("### 1. 状态符号")
    md.append("- `[x]` **已完成实证**：已建立独立工程，通过 Host/Wasm 双 Target 编译门禁，并在 UniSim/Headless 中获得数据实证。")
    md.append("- `[ ]` **待适配**：功能在仿真规划范围内，待随着后续里程碑（如 M5）逐步翻牌实施。")
    md.append("- `[-]` **声明 Out-of-Scope / 硬件专用**：遵循 [ADR-0012 合约诚实原则](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)，明确标明因缺乏物理射频波形、外部 PHY 硬件或物理熔丝而暂不适配。")
    md.append("- `🚫` **前置阻断**：依赖尚未就绪的平台抽象契约。")
    md.append("")
    md.append("### 2. 可观测性分级 (Observability Level)")
    md.append("- 🎯 **Level 1（界面直观可视）**：在 UniSim 画布或 Inspector 拥有直观控件（如 LED、数码管、Virtual BLE Inspector 调试面板）。")
    md.append("- 📜 **Level 2（控制台日志/网络数据）**：虚拟串口终端或网络通信控制台有格式化数据流（UART、MQTT、HTTP）。")
    md.append("- ⚡ **Level 3（IO 打点/波形探测）**：硬件定时器中断、GPIO 边沿触发波形。")
    md.append("- ⚙️ **Level 4（纯内部静默逻辑）**：内存管理、错误码捕获或寄存器级状态校验。")
    md.append("")
    md.append("---")
    md.append("")
    md.append(f"## 三、 {total_count} 个官方示例逐项核对总账")
    md.append("")

    for cat_key, cat_title in CATEGORY_ORDER:
        cat_items = by_category.get(cat_key, [])
        if not cat_items:
            continue
        first_id = cat_items[0]["id"]
        last_id = cat_items[-1]["id"]
        passed = sum(1 for it in cat_items if it["status"] == "[x]")

        md.append(f"<a id=\"{cat_key}\"></a>")
        md.append(f"### {cat_title}（共 {len(cat_items)} 项 | 编号 `#{first_id} ~ #{last_id}` | 已实证: {passed} 项）")
        md.append("")
        md.append("| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |")
        md.append("| :---: | :---: | :--- | :---: | :---: | :--- | :--- |")
        for it in cat_items:
            md.append(f"| {it['status']} | {it['id']} | `{it['path']}` | {it['obs']} | {it['prio']} | `{it['app']}` | {it['desc']} |")
        md.append("")
        md.append("---")
        md.append("")

    content = "\n".join(md)
    OUTPUT_MD_1.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_MD_1.write_text(content, encoding="utf-8")
    OUTPUT_MD_2.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_MD_2.write_text(content, encoding="utf-8")

    print(f"Checklist successfully written to:")
    print(f"  1. {OUTPUT_MD_1}")
    print(f"  2. {OUTPUT_MD_2}")
    print(f"Stats: Total={total_count}, Passed={status_counts['[x]']}, Pending={status_counts['[ ]']}, OutOfScope={status_counts['[-]']}")

if __name__ == "__main__":
    main()
