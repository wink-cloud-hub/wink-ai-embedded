#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""
Generate docs/vendors/Espressif/ESP32_IDFV61_EXAMPLE_CHECKLIST.md
Exhaustive, 100% census of all 476 official examples in ESP-IDF v6.1.
"""

from pathlib import Path
import datetime

ROOT_EXAMPLES = Path(r"D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf\examples")
OUTPUT_MD = Path("wink-micro-os/frameworks/esp_idf/docs/05-esp32-idfv61-example-checklist.md")

# Known implemented in M0~M4
IMPLEMENTED_MAP = {
    "get-started/blink": ("esp_idfv61_blink_gpio", "🎯 Level 1", "M0 标杆已落地。GPIO 输出 + FreeRTOS 延时驱动板载 LED 呼吸闪烁。"),
    "peripherals/ledc/ledc_basic": ("esp_idfv61_ledc_basic", "🎯 Level 1", "M2 已落地。LEDC 定时器与通道配置，定点 PWM 占空比平滑渐变，无浮点。"),
    "peripherals/i2c/i2c_basic": ("esp_idfv61_i2c_basic", "📜 Level 2", "M2 已落地。现代对象式 I2C Master 驱动，总线与器件二级句柄，读写传感器寄存器。"),
    "peripherals/uart/uart_echo": ("esp_idfv61_uart_echo", "📜 Level 2", "M2 已落地。UART 阻塞读/写回环，环形缓冲区与多任务调度。"),
    "peripherals/timer_group/gptimer": ("esp_idfv61_gptimer_alarm", "⚡ Level 3", "M2 已落地。高精度通用硬件定时器 Alarm 回调与自动重载机制。"),
    "wifi/getting_started/station": ("esp_idfv61_wifi_sta", "📜 Level 2", "M4-1 已落地。虚拟 AP 状态机与 DHCP 虚拟 IP 分配，esp_event 事件循环派发。"),
    "protocols/esp_http_client": ("esp_idfv61_http_client", "📜 Level 2", "M4-2 已落地。HTTP 客户端请求隧道桥接，支持 GET/POST 响应流式解析。"),
    "protocols/mqtt": ("esp_idfv61_mqtt_tcp", "📜 Level 2", "M4-2 已落地。ESP-MQTT 客户端连接内存虚拟轻量 Broker，Pub/Sub 实时闭环。"),
    "bluetooth/nimble/bleprph": ("esp_idfv61_bleprph", "🎯 Level 1", "M4-3/M4-4 已落地。NimBLE GATT 静态属性池、特征值读写回调与 Virtual BLE Inspector 交互面板。"),
}

# Category metadata
CATEGORY_TITLES = {
    "get-started": ("基础快速起步 (Get-Started)", 0),
    "peripherals": ("片上与总线外设 (Peripherals)", 1),
    "system": ("操作系统与核心系统调用 (System & OS)", 2),
    "protocols": ("网络与应用层通信协议 (Protocols)", 3),
    "wifi": ("Wi-Fi 无线局域网 (Wi-Fi)", 4),
    "bluetooth": ("蓝牙协议栈 (Bluetooth)", 5),
    "storage": ("片上存储与文件系统 (Storage)", 6),
    "network": ("底层网络与接口 (Network)", 7),
    "cxx": ("C++ 运行时与语言特性 (C++)", 8),
    "build_system": ("构建系统与组件组织 (Build System)", 9),
    "security": ("硬件加密与芯片安全特性 (Security - 硬件物理特性)", 10),
    "custom_bootloader": ("定制引导加载程序 (Custom Bootloader)", 11),
    "ethernet": ("有线以太网 (Ethernet - 外部 PHY 硬件)", 12),
    "lowpower": ("超低功耗与 ULP 协处理器 (Low Power & ULP)", 13),
    "mesh": ("Wi-Fi 空间自组网 (Mesh)", 14),
    "openthread": ("OpenThread 802.15.4 线程网络 (OpenThread)", 15),
    "zigbee": ("Zigbee 2.4G 射频网络 (Zigbee)", 16),
    "ieee802154": ("IEEE 802.15.4 原始射频 (IEEE 802.15.4)", 17),
    "phy": ("射频物理层与工厂校准 (PHY & Calibration)", 18),
}

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
    examples = []
    for p in sorted(ROOT_EXAMPLES.rglob("CMakeLists.txt")):
        if (p.parent / "main").is_dir():
            rel = p.parent.relative_to(ROOT_EXAMPLES).as_posix()
            cat = rel.split("/")[0]
            examples.append((cat, rel))

    print(f"Total discovered examples: {len(examples)}")
    
    # Categorize
    by_category = {}
    for cat, rel in examples:
        by_category.setdefault(cat, []).append(rel)

    # Count stats
    total_count = len(examples)
    status_counts = {"[x]": 0, "[ ]": 0, "[-]": 0, "🚫": 0}
    
    analyzed_items = []
    for idx, (cat, rel) in enumerate(examples, 1):
        status, obs, prio, app, desc = analyze_example(rel)
        status_counts[status] += 1
        analyzed_items.append({
            "id": f"{idx:03d}",
            "cat": cat,
            "path": rel,
            "status": status,
            "obs": obs,
            "prio": prio,
            "app": app,
            "desc": desc,
        })

    # Build Markdown Content
    now = datetime.datetime.now().strftime("%Y-%m-%d")
    md = []
    md.append("# ESP-IDF v6.1 官方示例全量仿真适配核对清单 (Checklist)")
    md.append("")
    md.append("> **权威上游路径**：`D:\\software\\embedded-tools\\esp-idf\\.espressif\\v6.1\\esp-idf\\examples`  ")
    md.append("> **参考标准规范**：[`PLAN-20260922-ESP-IDF-SIM-MASTER`](../../implementation-plans/esp32/2026-09-22-esp-idf-simulation-interception-master-plan.md) §7.1.1（三层证据塔与精选规则）  ")
    md.append("> **对标基线**：[`docs/vendors/Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md`](../Cmsemicon/CMS8S78XX_EXAMPLE_CHECKLIST.md)（业界最高保真审计基线）  ")
    md.append(f"> **生成日期**：{now} | **版本**：v1.0 (全量普查版)  ")
    md.append("")
    md.append("---")
    md.append("")
    md.append("## 一、 总体适配进度与全量普查统计")
    md.append("")
    md.append(f"- **官方分类总数**：{len(by_category)} 个功能大类")
    md.append(f"- **官方独立子示例总数**：**{total_count} 个**（地毯式 100% 全量建档，无任何遗漏）")
    md.append("- **全景状态分布统计**：")
    md.append(f"  - `[x]` **已完成适配并实证 (Passed)**：**{status_counts['[x]']} 项**（M0~M4 核心黄金外设与网络连接代表）")
    md.append(f"  - `[ ]` **待适配排期中 (Pending In-Scope)**：**{status_counts['[ ]']} 项**（ADC、RMT/RGB、NVS、WebSocket 等纯软件可模拟用例）")
    md.append(f"  - `[-]` **纯物理硬件专用 / 声明 Out-of-Scope**：**{status_counts['[-]']} 项**（Zigbee、Thread、以太网变压器、eFuse 硬件不可逆特性等诚实排除项）")
    md.append(f"  - `🚫` **存在前置阻断 (Blocked)**：**{status_counts['🚫']} 项**")
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
    md.append("## 三、 476 个官方示例逐项核对总账")
    md.append("")

    # Output tables by category in prioritized order
    sorted_cats = sorted(by_category.keys(), key=lambda c: CATEGORY_TITLES.get(c, (c, 99))[1])
    
    for cat in sorted_cats:
        cat_title = CATEGORY_TITLES.get(cat, (cat.title(), 99))[0]
        cat_items = [it for it in analyzed_items if it["cat"] == cat]
        passed_in_cat = sum(1 for it in cat_items if it["status"] == "[x]")
        
        md.append(f"### {cat_title}（共 {len(cat_items)} 项 | 已实证: {passed_in_cat} 项）")
        md.append("")
        md.append("| 状态 | 编号 | 官方子示例相对路径 | 可观测等级 | 优先级 | 对应 wink-micro-app | 验收标准与架构说明 |")
        md.append("| :---: | :---: | :--- | :---: | :---: | :--- | :--- |")
        for it in cat_items:
            md.append(f"| {it['status']} | {it['id']} | `{it['path']}` | {it['obs']} | {it['prio']} | `{it['app']}` | {it['desc']} |")
        md.append("")
        md.append("---")
        md.append("")

    OUTPUT_MD.write_text("\n".join(md), encoding="utf-8")
    print(f"Checklist successfully written to {OUTPUT_MD}")
    print(f"Stats: Total={total_count}, Passed={status_counts['[x]']}, Pending={status_counts['[ ]']}, OutOfScope={status_counts['[-]']}")

if __name__ == "__main__":
    main()
