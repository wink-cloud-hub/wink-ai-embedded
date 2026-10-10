# ⚡ 5 分钟快速上手指南 (Getting Started Guide)

> 本指南面向嵌入式开发者、应用工程师与 AI Agent。通过“循序渐进的四阶漏斗”，帮助你在 5 分钟内建立对 **WinkMicroOS 虚实同源嵌入式平台** 的直观认知并跑通首个闭环。

---

## 🧭 体验路径指引（30 秒速览）

WinkMicroOS 彻底打破了传统嵌入式开发“依赖物理硬件、联调反馈需人在环、调试难复现”的开环困境，通过 **硬件即代码 (Hardware-as-Code)** 与 **微秒级确定性虚拟时钟**，将整个开发验证流程收敛为可自动化、可断言的数字化实验室。

根据你当前的环境与诉求，选择最适合你的上手路线：

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ 🚀 路径 A：零安装 · 浏览器 1 分钟在线试玩（效果直观 · 交互验收首选，实时波形与虚拟器件）      │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 💻 路径 B：本地实战 · 3 分钟无头仿真与门禁（日常开发首选 · 确定性微秒级证据链支撑 AI 自闭环） │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 🔌 路径 C：虚实同源 · 5 分钟真机部署 ESP32（出厂终验 · 同一份源码直接烧录真实物理芯片）      │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 🚀 路径 A：零安装 · 浏览器 1 分钟在线试玩（效果直观 · 交互验收首选）

推荐所有初次接触 WinkMicroOS 的开发者首选此路径。免除安装编译工具链的繁琐步骤，在浏览器沙盒中立即感受外设因果交互。

### 第 1 步 · 访问在线仿真工作台并导入套件
在现代浏览器（推荐 Chrome / Edge）中直接打开：  
👉 **[Wink-AI 在线仿真设计器](http://www.wink-ai.com/simulator/index.html)**

下载本地仓库：
```bash
git clone https://github.com/wink-cloud-hub/wink-ai-embedded.git
```
在页面中点击 **打开套件根目录** 按钮，选择上面 clone 到本地的 `wink-ai-embedded` 文件夹导入。

### 第 2 步 · 选择开箱即用示例应用
在线设计器已内置来自 [`wink-micro-app`](../../../../wink-micro-app/) 的经典应用套件：
* **经典推荐（8位极简）**：`mcs51/button_led` —— 经典 8051 按键点灯程序（Keil C51 原始代码零侵入运行）。
* **进阶推荐（智能车体）**：`native/avoidance_car` —— 智能避障小车（超声波测距 + 舵机转向，事件驱动完整闭环）。
* **原厂示例（芯片原厂）**：`vendor/esp_idf_v61/` —— 芯片原厂示例（ESP-IDF 官方示例）。

### 第 3 步 · 进行虚拟交互与时序观测
应用启动后，你将在工作台视图中看到数字化硬件与实时状态：
1. **交互操作**：在画布上点击虚拟轻触微动开关（`btn`），观察虚拟发光二极管（`led`）状态实时切换为点亮。
2. **时序观测**：展开底部的 **实时逻辑分析仪 (Logic Analyzer)**，清晰看到虚拟微秒时钟推进下，GPIO 引脚电平微秒级跳变的真实波形脉冲。
3. **Trace 记录**：在虚拟 Console 控制台窗口中，实时输出符合 `SimTraceSpec` 契约的结构化因果事件流：
   ```text
   [TRACE] 00:00:00.200000 | GPIO_INPUT | pin: 26 | value: 0 (btn pressed)
   [TRACE] 00:00:00.200050 | GPIO_SET   | pin: 8  | value: 0 (led lit)
   ```

---

## 💻 路径 B：本地实战 · 3 分钟无头仿真与门禁（日常开发首选 · 证据链支撑 AI 自我验证）

如果你是代码开发者或希望接入 AI Agent 自动化研发流程，本地 CLI 工具链能为你提供完全脱离界面的“无头断言”能力（Headless Testing）。

### 第 1 步 · 获取仓库代码
```bash
git clone https://github.com/wink-cloud-hub/wink-ai-embedded.git
cd wink-ai-embedded
```

### 第 2 步 · 安装与验证统一工具链 `winkcli`
WinkMicroOS 的构建、代码生成、仿真断言与硬件烧录均收敛于统一工具链 [`winkcli`](../../../../wink-tools/docs/zh/01-cli-reference.md)：

```powershell
# 方式 1 - winget 安装（Windows 推荐）
winget install WinkAI.WinkCli

# 方式 2 - GitHub Releases 离线包下载
# 从 https://github.com/wink-cloud-hub/wink-ai-embedded/releases 下载解压并加入 PATH
```

> 💡 **源码开发提示**：若暂未全局安装 `winkcli`，在本仓库根目录下运行 `python wink.py` 可获得完全等价的命令体验。

安装后运行环境诊断，核实系统依赖状态：
```bash
winkcli doctor
```

### 第 3 步 · 运行无头场景断言与全量测试
无需连接单片机，直接执行确定性测试集：
```bash
# 执行本地 host 仿真编译与全量单测矩阵
winkcli test
```

亦可针对特定应用运行微秒级场景断言（以 [`mcs51/button_led`](../../../../wink-micro-app/mcs51/button_led/) 为例）：
```bash
winkcli sim run --app mcs51_button_led --mode headless
```

**控制台预期输出**：
```text
[INFO] Loaded scenario: button-led.scenario.json (Seed: 42, Mode: behavioral)
[PASS] 00:00:00.100000 | ASSERT_POINT: target "plugin:led/on" == false
[PASS] 00:00:00.800000 | ASSERT_POINT: target "plugin:led/on" == true
[PASS] 00:00:00.800000 | ASSERT_POINT: target "gpio:8" == 0
[PASS] 00:00:01.500000 | ASSERT_POINT: target "plugin:led/on" == false
[RESULT] All 5 assertions passed. Trace verified. (virtual time: 1500000 us)
```

---

## 🔌 路径 C：虚实同源 · 5 分钟真机部署 ESP32（出厂终验 · 同源代码落地物理硬件）

当你完成了在线或本地仿真验证后，同一套业务逻辑可以直接部署到物理芯片上。

### 第 1 步 · 准备硬件
使用 Type-C 或 Micro-USB 数据线，将 ESP32 开发板（如 `esp32_devkitc_v4`）接入电脑 USB 接口。

### 第 2 步 · 一键编译与烧录
以系统官方冒烟验证工程 [`fixtures/devkitc_smoke`](../../../../wink-micro-app/fixtures/devkitc_smoke/) 为例，通过透传参数直接完成编译、烧录并启动监视器：

```powershell
winkcli esp32 --app devkitc_smoke -- -p COM3 flash monitor
```
*(注：请根据设备管理器中的实际端口将 `COM3` 替换为你电脑对应的串口号)*

### 第 3 步 · 观察真机因果响应
* 按下板载 `BOOT` 键，板载 LED 指示灯立即响应点亮；
* 串口终端打印出的 UART 日志与 Wasm 仿真器中捕获的事件完全一致；
* 这证明了固件在虚拟仿真世界与真实物理世界中的**行为同源性**。

---

## 🧩 核心机制速览：硬件即代码与双运行模式

### 1. 硬件即代码 (Hardware-as-Code)：硬件电路外设拓扑由 JSON 表示
在 WinkMicroOS 中，**硬件电路与外设拓扑完全由 JSON 文件统一表示**（单一事实源 [`wink-app.json`](../../../../wink-micro-app/mcs51/button_led/wink-app.json)）。引脚连线、电气极性与器件属性不再散落于散碎的代码宏定义中：

```json
{
  "app_name": "button_led",
  "board": "stc89c52_devboard",
  "mcu": "at89c52",
  "devices": {
    "btn": { "type": "button", "gpio_pin": 26, "active_low": true },
    "led": { "type": "led",    "gpio_pin": 8,  "active_high": false }
  }
}
```

### 2. 两大核心应用模式
系统支持两种截然不同但各取所长的开发模式（详见 [`wink-micro-app` 架构规范](../../../../wink-micro-app/README.md)）：

| 维度 | 模式 1：AI-Native 统一 OS (类似Arduino) | 模式 2：源码零侵入兼容 (Tier 2/3) |
|---|---|---|
| **适用场景** | 跨芯片0修改代码,API语义化AI友好 | 已有项目不限制库和框架 |
| **代码形态** | 使用 WinkMicroOS DAL/BAL 语义 API（Role-Action） | **100% 原始 Keil C51、 Arduino 源码、ESP-IDF，一行不改** |
| **虚实关系** | 虚实完全同源：仿真 Wasm 与真机均运行 OS 内核 | 虚实正交解耦：真机仅烧录纯原生固件，仿真端由 Wasm 拦截 |
| **代表工程** | `native/avoidance_car`, `native/oled_dashboard` | `vendor/esp_idfv61/`, `vendor/cms8s78xx` |

---

## 🛠️ 创建并运行你的第一个专属应用

你可以借助 `winkcli` 脚手架在 30 秒内搭建一个全新的嵌入式工程：

```bash
# 1. 创建应用骨架
winkcli create app my_first_demo

# 2. 生成对应设备树与仿真资产
winkcli gen app-schema --app my_first_demo

# 3. 本地编译验证
winkcli build host --app my_first_demo

# 4. 执行场景仿真
winkcli sim run --app my_first_demo --mode headless
```

---

## ❓ 常见问题与排障指南 (FAQ)

### Q1: `winkcli doctor` 提示找不到 gcc / cmake 等构建工具？
* **解答**：Windows 平台推荐下载 [WinLibs (GCC + MinGW-w64)](https://winlibs.com/) 并将 `bin/` 目录加入环境变量 `PATH`；或使用 `winkcli setup --set gcc="D:/path/to/gcc.exe"` 明确指定绑定路径。详见 [工具链配置指南](../../../../wink-tools/docs/zh/02-toolchain-setup.md)。

### Q2: 仿真器中点击按键，LED 为什么没有点亮？
* **解答**：请检查 `wink-app.json` 中配置的引脚极性：
  * 若按键按下时引脚电平为低（接 GND），需设置 `"active_low": true`；
  * 若 LED 为低电平导通驱动，需确认驱动电平与代码逻辑匹配。

### Q3: 烧录 ESP32 时报串口访问拒绝（Access Denied）？
* **解答**：通常是因为串口正被其他串口监视助手（如串口调试助手、VS Code 串口插件）占用。请关闭外部连接后重新执行烧录命令。

---

## 📖 进阶技术导航与设计规范

完成快速上手后，建议按需深入阅读核心设计规范：

* 🏛️ **平台系统总体架构** ➔ [01-system-overview.md](../01-system-overall/01-system-overview.md)
* ⚙️ **C 内核分层与静态分发 (PAL/DAL/BAL)** ➔ [02-wink-micro-os 设计规范](../02-wink-micro-os/README.md)
* 🌐 **UniSim 仿真机制与 Bridge ABI 契约** ➔ [04-wasm-simulation 设计规范](../04-wasm-simulation/00-README.md)
* 🔧 **WinkCli 工具链全量参考手册** ➔ [wink-tools CLI 参考](../../../../wink-tools/docs/zh/01-cli-reference.md)
* 🤖 **面向 AI Agent 的设计与检索约定** ➔ [docs/AGENTS.md](../../../AGENTS.md)
