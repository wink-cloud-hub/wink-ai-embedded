# wink-micro-app — 嵌入式应用中心与示例集合

> 本目录作为 Wink-AI 嵌入式端（WinkMicroOS）的应用工程库，承载 AI 生成的业务逻辑、开发者手写工程、开源生态兼容示例以及各半导体原厂官方例程的回归套件。
> 系统顶层架构与仿真体系设计详见：[01-system-overview.md](../docs/zh/design/01-system-overall/01-system-overview.md)。

---

## 一、两大核心运行模式

结合平台整体架构（参见系统总览《2.1 异构芯片仿真四层兼容体系》），本目录下的应用分为**两种截然不同的运行与交付模式**：

```text
┌──────────────────────────────────────────────────────────────────────────────────┐
│ 模式 1：AI-Native 统一 OS 架构 (对应系统架构 Tier 1)                               │
│ 仿真与真机均运行 wink-micro-os 内核，上层业务通过 Role-Action 语义 API 编排          │
│ 虚实同源：【仿真端 (Wasm) ✅ 运行 OS】 ⟷ 【真机端 (ESP32/STM32) ✅ 运行 OS】           │
├──────────────────────────────────────────────────────────────────────────────────┤
│ 模式 2：源码零侵入兼容与原厂回归 (对应系统架构 Tier 2 拦截代理 / Tier 3 指令解释 / Vendor)│
│ 原厂/用户原生代码“一行不改”，无需适配 wink-micro-os                                │
│ 虚实分离：【仿真端 (Wasm) ⚡ 加载 OS 兼容拦截/虚拟机】 ⟷ 【真机端 🛡️ 纯原生标准源码/二进制】│
└──────────────────────────────────────────────────────────────────────────────────┘
```

### 1. 模式 1：AI-Native 统一 OS 应用（深度集成）
* **运行机制**：
  * **真机端**：编译链接 `wink-micro-os`，直接基于 OSAL/PAL/DAL 运行调度。
  * **仿真端**：同样编译链接 `wink-micro-os`，由底层 PAL 旁路至 UniSim 虚拟总线。
  * **虚实 100% 同源**：业务代码完全一致，换芯（如 ESP32 切 STM32）业务层零修改。
* **开发模式**：
  * 基于 `wink-app.json` 声明外设拓扑；
  * 使用生成的 `device_tree.h` 中的 **Role-Action 语义 API**（`{instance}_{verb}`）编写，支持事件队列与自动轮询（L1 级）或直接操作 `dal_*`（L2 级）。

### 2. 模式 2：源码零侵入兼容与原厂回归（外部生态平移）
* **运行机制**：
  * **真机端**：**仅运行原厂/用户原生代码**。通过 Keil、SDCC、Arduino IDE 或原厂专属编译器直接编译成 Bin/Hex 烧录，**完全不依赖、不加载 `wink-micro-os`**。
  * **仿真端**：为了在 Web 浏览器中高保真呈现外设交互，**仅在仿真编译时由 Wasm 端加载 `wink-micro-os` 兼容层**（如 `wink_mcs51_compat` 拦截 SFR 寄存器读写并打桩到 UniSim 总线，或通过 ISA 解释器模拟 CPU）。
* **开发模式**：
  * **“一行不改”**：用户原生代码保持 100% 原汁原味；
  * 仅需提供一个轻量 `wink-app.json` 声明电路外设引脚映射与上游元数据。

---

## 二、应用分类概览与命名规范

随着工程数量持续增加，本目录**不采用固定死板的单一应用清单**，而是建立清晰的**前缀命名规范**以便 CI 脚本自动化发现、分类构建与批量回归：

| 目录前缀 / 命名模式 | 所属模式 | 技术分类与定位 | 典型特征与代码形态 | 示例代表 |
| :--- | :--- | :--- | :--- | :--- |
| **`vendor_{chip}_{ver}_{demo}`** | 模式 2 | **原厂官方例程回归套件** | 芯片厂商官方源码零侵入，带 `upstream` SSOT 溯源，CI 强校验 Diff 门禁 | `vendor_cms8s78xx_v202_led_4com_8seg` |
| **`mcs51_{module}_{feature}`** | 模式 2 | **MCS-51 架构兼容生态** | 标准 C51 源码（含 SFR、中断、延时），仿真端 Wasm C++ Proxy 拦截 | `mcs51_health_pot` (养生壶), `mcs51_button_led` |
| **`arduino_{demo}`** | 模式 2 | **开源 Arduino 生态平移** | 标准 `setup()` / `loop()` 语法与库，基于 ArduinoCore-API 兼容层拦截 | `arduino_blink_demo` |
| **`pdk_{demo}`** | 模式 2 | **专有 8 位 OTP 芯片生态** | 极低成本单片机原生二进制/汇编，UniSim 内置 ISA 解释器仿真 | `pdk_button_led` |
| **`{feature_name}` (如 oled/car)** | 模式 1 | **Wink 原生 AI-Native 业务** | 深度使用 `wink-micro-os/dal`，Role API 事件驱动或 L2 专家双任务 | `oled_dashboard`, `avoidance_car`, `dual_task_demo` |
| **`*_smoke` / `*_fixture`** | 系统支撑 | **平台 CI 与确定性测试夹具** | 硬件板级 Bring-up 自检、Wasm JS 胶水测试、微秒级时钟步进确定性验证 | `devkitc_smoke`, `unisim_smoke`, `determinism_fixture` |

---

## 三、模式 1：AI-Native 角色 API 与事件模型

对于模式 1 的原生低代码/AI 编排应用，推荐优先采用 **Role API**（面向对象语义层），业务层代码不出现任何物理引脚号与底层中断配置。

### 1. 语义化调用（`{instance}_{verb}`）
```c
/* 由 wink-app.json 代码生成器产出的统一业务接口 */
user_button_enable_events();   /* 开启按键事件队列 */
status_led_activate();         /* 点亮状态灯 */
status_oled_draw_text(0, 0, "HELLO");
status_oled_flush();
```

### 2. 事件驱动模式（软轮询 vs 硬件中断）
在 `wink-app.json` 中，通过声明式配置底层的事件采集方式：
* **`soft_poll`（默认）**：跨平台通用（Host / Wasm / ESP32 / STM32 行为完全一致），由底层周期定时器轮询检测边缘并消抖；
* **`gpio_irq`**：物理芯片硬件边沿中断（适用于 ESP32 等需要低功耗唤醒场景；在 Wasm 仿真端会自动安全降级为 soft_poll 并记录警告）。

```json
{
  "type": "button",
  "pin": 10,
  "active_low": true,
  "event_drive": "soft_poll",
  "auto_poll_ms": 10,
  "debounce_ms": 20
}
```

业务层接收统一事件回调：
```c
void app_on_event(const wink_event_t *event) {
    if (event->type == WINK_EVENT_BUTTON_PRESSED) {
        status_led_toggle();
    }
}
```

### 3. L2 专家逃生通道 (Expert Mode)
若需直接操作底层或编写复杂的硬实时状态机，代码仍可直接调用 `dal_*` 命名接口或手写 `device_tree`（如 `dual_task_demo`），兼具灵活性。

---

## 四、模式 2：厂商原厂例程规范 (Vendor Specification)

面向半导体厂商（中微、乐鑫、意法、沁恒等）的原厂参考代码，遵循以下接入规范：

### 1. 目录命名（全小写蛇形命名）
格式：`vendor_{chip}_{version}_{module_name}`
* **全小写 + 下划线**：避免跨操作系统（Windows 与 Linux CI）大小写敏感冲突；
* **版本号去点号**：如 `v202`（代表 V2.0.2），避免路径与 CMake Target 命名解析异常；
* **`vendor_` 前缀**：便于 CI 脚本一键通过 `glob("vendor_*")` 批量发现与一键回归。

### 2. 上游元数据标准（`wink-app.json`）
所有原厂示例必须在 `wink-app.json` 中声明 `upstream` 作为单一事实来源（SSOT）：
```json
{
  "app_name": "vendor_cms8s78xx_v202_led_4com_8seg",
  "display_name": "CMS8S78xx V2.0.2 - LED 4COM_8SEG_LED",
  "board": "cms8s78xx_devboard",
  "category": "vendor_example",
  "mcu": "cms8s78xx",
  "tick_ms": 1,
  "upstream": {
    "vendor": "Cmsemicon",
    "version": "V2.0.2",
    "source_dir": "docs/vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2/CMS8S78xx_Example/Example/LED/4COM_8SEG_LED/code"
  },
  "devices": {
    "display": {
      "type": "seg_display",
      "variant": "direct_gpio_4d",
      "com0_pin": "$board.headers.P30",
      "a_pin": "$board.headers.P10"
    }
  }
}
```

### 3. 双端保障门禁
* **“一行不改” 源码 Diff 门禁**：CI 自动校验原厂源文件与 app 内文件哈希一致性；
* **双端回归验证**：支持通过 `wink.py` 一键生成 Wasm 并在仿真中回归验证数码管/外设逻辑。

---

## 五、统一构建与验证

所有应用均由根目录工具链统一驱动构建：

### 1. Wasm 仿真构建
```bash
# 构建指定的 App 并在 build/wasm/<app_name> 产出仿真二进制
python packages/wink-tools/wink.py build wasm --app wink-micro-app/vendor_cms8s78xx_v202_led_4com_8seg

# 构建原生 AI-Native 应用
python packages/wink-tools/wink.py build wasm --app wink-micro-app/oled_dashboard
```

### 2. ESP32 物理真机编译与烧录（模式 1 应用）
```bash
# 自动通过 idf.py 构建并烧录至 ESP32
python packages/wink-tools/wink.py esp32 oled_dashboard
```

---

## 关联设计文档
* 平台顶层架构：[docs/zh/design/01-system-overall/01-system-overview.md](../docs/zh/design/01-system-overall/01-system-overview.md)
* 业务逻辑与代码生成设计：[docs/zh/design/03-app-codegen/01-app-business-logic.md](../docs/zh/design/03-app-codegen/01-app-business-logic.md)
* 按键事件驱动架构决策：[docs/decisions/core/0031-button-event-drive-backends.md](../docs/decisions/core/0031-button-event-drive-backends.md)
