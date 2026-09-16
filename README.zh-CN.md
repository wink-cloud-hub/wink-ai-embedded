# WinkMicroOS

**软件 AI 已经会写、会跑、会自愈；嵌入式 AI 会写代码，却没人敢让它上产线——因为固件必须烧进芯片、由人按键才能验证。**

WinkMicroOS 是闭合这个环的确定性数字实验室：同一份 C 源码在浏览器 Wasm 沙盒与真实 MCU 上运行——从 ESP32 一直到单价几毛钱的 8 位工业芯片——每次运行都留下可审查的 PASS 证据链。*为 Agent 而建，不只为人类。*

[![CI](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/pr.yml/badge.svg)](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/pr.yml)
[![Nightly](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/nightly.yml/badge.svg)](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/nightly.yml)
[![License Gate](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/license-gate.yml/badge.svg)](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/license-gate.yml)
[![Release](https://img.shields.io/github/v/release/wink-cloud-hub/wink-ai-embedded?label=release)](https://github.com/wink-cloud-hub/wink-ai-embedded/releases/latest)
[![License](https://img.shields.io/badge/license-LGPL--3.0--only%20runtime-blue)](./.github/license-map.json)
![Platform](https://img.shields.io/badge/targets-wasm%20%7C%20esp32%20%7C%208051%20%7C%20avr%20%7C%20pdk-informational)
![Docs](https://img.shields.io/badge/docs-English%20%7C%20%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87-success)

[English](./README.md) | **简体中文**
&nbsp;·&nbsp; [▶ 在线试玩](http://www.wink-cloud.com/simulator/index.html) &nbsp;·&nbsp; [5 分钟上手](./docs/zh/design/00-quick-start/01-5min-getting-started.md) &nbsp;·&nbsp; [文档中心](./docs/zh/README.md) &nbsp;·&nbsp; [路线图](./docs/zh/design/01-system-overall/02-mvp-roadmap.md)

**状态：**  已发布 · 公共 CI 绿色 · host 测试可执行（见 [`wink-micro-os/TESTING.md`](./wink-micro-os/TESTING.md)）

<!-- TODO(素材)：补充 15 秒首屏 GIF（导入仓库 → 运行 button-led 场景 → 按下虚拟按键 → LED 点亮 + 实时波形）。 -->

---

## 那个从未闭合的环

纯软件领域的 AI 编程早已闭环。嵌入式却始终闭不上——因为环路中间站着一个人：

```text
纯软件 —— 已闭环
  生成 → 运行 → 测试 → 修复 → ↻

嵌入式现状 —— 开环（人在回路中央）
  生成 ─▶ [人：烧录] ─▶ [人：按键] ─▶ [人：看示波器] ─▶ [人：抄日志喂回] ─▶ ↻

"自动化" 台架 —— 仍是开环（伪闭环）
  生成 → 自动烧录 → 逻辑分析仪 → [人：解读、决策] ─▶ ↻

WinkMicroOS —— 闭环
  生成 → 硬件即代码（设备树 + 场景 JSON）
       → 确定性运行（host / wasm）
       → 结构化 trace + PASS 证据链
       → 修复 → ↻
```

自动烧录 + 抓波形，自动化的是"工具"，不是"环路"：物理世界依然要人按键、人改线、人复现边缘工况。只有当硬件本身变成代码——可复现、可设种子、可脚本化、不受物理时间约束——环路才真正闭合。

这是 WinkMicroOS 的设计前提。

## 为什么选择 WinkMicroOS

- **真正的产业主力在低成本芯片。** Wokwi、QEMU 等方案服务的是 ARM/RISC-V 开发板；而工业出货主力是没有 SWD/JTAG、没有平价 ICE 的 8/16 位芯片（STC、中微、应广、合泰、松翰……），死机只能靠 OTP 盲调。WinkMicroOS 提供指令级仿真：断点、寄存器、堆栈，全部在浏览器里。
- **同一份源码，处处运行。** 一份 C 代码可编译到 `host`（测试）、`wasm32`（浏览器 / 无头仿真）与 `targets/esp32`；未修改的 Keil C51 源码（[ADR-0075](./docs/decisions/core/0075-mcs51-production-wasm-target-headless.md)）与 Arduino Sketch（[ADR-0035](./docs/decisions/core/0035-arduino-compat-polymorphism-sandbox.md)）无需移植层即可直接仿真。
- **确定性是设计前提。** 虚拟时钟推进、固定随机种子、自带断言的无头场景脚本。失败永远可复现——不存在"我这儿能跑"。
- **硬件即代码。** 板级拓扑存 JSON（`wink-app.json` + 板卡注册表），测试激励存场景 JSON。没有面包板、没有杜邦线，天然适配 CI。
- **可分享的行为，而不是示波器截图。** 一次运行即可生成可分享的在线仿真会话——客户与产品经理在浏览器里直接体验灯光呼吸节拍，而不是对着波形截图皱眉。
- **故意把它弄坏。** 掉电跌落、传感器断线、电机堵转、干烧过热：破坏性工况零风险注入、逐次精确重放。
- **可交付的证据链。** 每次运行都产出结构化、可回放的 PASS/FAIL 记录与确定性 trace（`SimTraceSpecV2`）——AI 产出的固件无需逐行通读即可审查，也无需凭感觉就敢合并。
- **为 AI Agent 而设计。** 研发闭环中的每一份输入输出都是文本，并提供面向机器阅读的约定（[AGENTS.md](./AGENTS.md)）——Agent 可以生成驱动、运行场景、读取结构化失败并自我修正。

## 为 Agent 而建，不只为人类

现有仿真器都是好工具——为坐在键盘前的人而设计。Agent 需要的是另一组属性：一切皆文本、默认无头、确定性、产出结构化证据。

| 工具 | 擅长 | 为什么闭不上 Agent 的环 |
|---|---|---|
| Proteus | 电路级 SPICE 仿真，教学经典 | 桌面重型、授权昂贵；芯片库停留在传统 51/AVR/PIC；无机器可读的证据输出 |
| Wokwi | Web 端 Arduino/ESP32 模拟器，体验优秀 | 面向创客教育；缺乏工业级测试框架；对年出货百亿计的低成本工业芯片空白 |
| QEMU | 开源指令级 / 系统级虚拟化 | 为操作系统级目标而建，不适配 MCU 微秒级外设时序；接入固件 CI 环路过重 |
| Renode | 多节点物联网系统仿真（Cortex-M / RISC-V） | 能力强但工作流重；面向高端 32 位场景，不覆盖几毛钱的 8 位生态 |

WinkMicroOS 不是同一用户的更好捕鼠夹，而是换了一个用户：Agent 本身。硬件即代码、确定性场景、无头证据——一切皆文本，一切可脚本化。

> **能力边界，说清楚。** 仿真不取代真机，它把研发风险左移。电气特性、EMC、热与机械行为仍必须由真机把关——我们的目标是把真机验证压缩成"最后一道确认"，而不是"第一轮迭代"。

## 看一个完整闭环

### 1 · 描述硬件

```json
// wink-micro-app/mcs51_button_led/wink-app.json
{
  "app_name": "mcs51_button_led",
  "board": "stc89c52_devboard",
  "mcu": "at89c52",
  "devices": {
    "btn": { "type": "button", "gpio_pin": 26, "active_low": true },
    "led": { "type": "led",    "gpio_pin": 8,  "active_high": false }
  }
}
```

### 2 · 编写逻辑 —— 原厂源码零修改，或使用 WinkMicroOS API

未修改的 Keil C51 源码，直接来自原厂 IDE —— `sbit`、SFR 全部原样（构建期转译生成仿真产物，原始文件从不被改动）：

```c
// wink-micro-app/mcs51_button_led/button_led.c  (SPDX: Apache-2.0)
#include <wink_mcu.h>

sbit KEY = P3^2;    /* push button on P3.2 / INT0, active-low */
sbit LED = P1^0;    /* LED on P1.0, low-drive-on */

void main(void) {
    LED = 1;
    while (1) {
        LED = (KEY == 0) ? 0 : 1;
        _nop_();    /* microstep / cooperative yield point */
    }
}
```

……或采用现代事件驱动风格，在 host、Wasm 与 ESP32 上行为完全一致：

```c
// wink-micro-app/avoidance_car/app_callbacks.c  (SPDX: Apache-2.0)
static void app_on_event(const wink_event_t *evt)
{
    if (evt->device != &front_radar || evt->type != WINK_EVENT_DISTANCE_READY) {
        return;
    }
    float cm = (float)evt->param / 10.0f;
    neck_servo_set_angle(cm < 20.0f ? 1800 : 900);  /* 0.1° units */
}
```

### 3 · 无头验证

场景是确定性的、自带断言，且无需浏览器（[SimTraceSpecV2](./docs/zh/design/04-wasm-simulation/00-README.md)）：

```json
// wink-micro-app/mcs51_button_led/unisim-scenarios/button-led.scenario.json (excerpt)
{
  "header": { "accuracyMode": "behavioral", "failurePolicy": "fail-fast",
              "determinism": { "prngSeed": 42 } },
  "steps": [
    { "type": "INPUT_PLUGIN_EVENT", "timeUs": "200ms", "targetPluginId": "btn",
      "action": "SET_PRESSED", "params": { "pressed": true } },
    { "type": "ASSERT_POINT", "timeUs": "800ms", "target": "plugin:led/on", "matcher": true },
    { "type": "ASSERT_POINT", "timeUs": "800ms", "target": "gpio:8", "matcher": 0 }
  ]
}
```

在本机用一条命令跑通等价验证——不需要硬件，也不需要浏览器：

```console
$ winkcli test                            # host 构建 + 全量测试（截至 2026-07 共 35 个可执行）
[PASS] All tests passed
```

## 支持的目标平台

板卡定义是硬件的单一事实来源（SSOT）：[`wink-tools/tools/codegen/boards/`](./wink-tools/tools/codegen/boards/README.md)。

| 芯片家族 | 注册表开发板 | 仿真分级 | 上游兼容性 |
|---|---|---|---|
| ESP32（Xtensa） | `esp32_devkitc_v4` | Tier 1 —— 全功能 Wasm 运行时，含调度器与多任务 | WinkMicroOS 原生应用（BAL / DAL） |
| MCS-51 / 8051 | `stc89c52_devboard`、`cms8s78xx_devboard` | Tier 2 —— 指令拦截 + SFR 虚拟网关 | **未修改的 Keil C51 源码**（`sbit`、`REGX52.H`、原厂 SFR） |
| AVR | `arduino_uno_r3` | Arduino 兼容层 | **未修改的 Arduino Sketch**（`Serial`、`String`、`millis`） |
| 应广 PDK | `padauk_pfs154_devboard` | Tier 3 —— 1:1 指令集虚拟机 | 原厂 PDK 源码（[ADR-0064](./docs/decisions/unisim/0064-chip-simulation-four-tier-taxonomy.md)） |

## 系统架构

```text
wink-micro-app/<app>/   你的固件（C）：L1 Role API / L2 dal_* 实例
        │
        │  device_tree.h（由 wink-app.json 生成）
        ▼
wink-micro-os 运行时 —— 静态分发 · 无 malloc · 协作式循环
        BAL  业务抽象层   事件 · 闭环控制
        DAL  器件抽象层   舵机 · 超声波 · 按键 · LED · OLED
        PAL  平台抽象层   gpio · pwm · i2c · uart · 定时器 · 中断
        runtime / trace / 故障注册表
        │
        ├── targets/host    单元测试
        ├── targets/wasm    UniSim（浏览器 / 无头 CI）
        ├── targets/esp32   ESP-IDF 固件载体（真机）
        └── frameworks/     mcs51 · avr · pdk 8/16 位 MCU 家族
```

## 仓库布局

| 路径 | 内容 | 许可 |
|---|---|---|
| [`wink-micro-os/`](./wink-micro-os/) | C 运行时：PAL / DAL / BAL、runtime、trace、targets（`host`、`wasm`、`esp32`）、MCS-51 框架、host 测试套件 | **LGPL-3.0-only**（以库形式链接进你的固件——固件可保持闭源） |
| [`wink-micro-app/`](./wink-micro-app/) | 示例与回归应用（MCS-51、PDK、Arduino、ESP32），各自包含设备树、场景与预编译 Wasm | Apache-2.0 |
| [`wink-firmware-carriers/`](./wink-firmware-carriers/) | 可烧录的固件载体工程（ESP-IDF） | LGPL-3.0-only |
| [`wink-tools/tools/codegen/boards/`](./wink-tools/tools/codegen/boards/) | 板卡注册表 —— 硬件单一事实来源（供 WinkCli 工具链消费） | Apache-2.0 |
| [`wink-plugin-peripherals/`](./wink-plugin-peripherals/) | 仿真外设插件（TypeScript）：超声波、WS2812…… | GPL-3.0-only |
| [`docs/`](./docs/README.md) | 双语 SSOT 设计规范、ADR、技术方案、计划与评审 | GPL-3.0-only |

## 设计准则

| 准则 | 原因 | 依据 |
|---|---|---|
| 负数错误码：`0` = 成功，`< 0` = 错误 | 各层统一的失败处理语义 | [ADR-0001](./docs/decisions/core/0001-error-code-sign-convention.md) |
| 编译期静态分发（POD + 命名 API，无虚表 / `container_of`） | 8 位 MCU 上可预期的代码体积与栈开销 | [ADR-0004](./docs/decisions/core/0004-static-dispatch-vs-runtime-ops.md) |
| 双 target 同源编译：wasm32 + xtensa 共用一份 C 代码 | 仿真行为必须等于真实行为 | [ADR-0002](./docs/decisions/unisim/0002-dual-target-compilation.md) |
| 禁止动态分配；协作式循环执行模型 | 无堆碎片，小芯片上内存有界 | [ADR-0007](./docs/decisions/core/0007-cooperative-loop-execution-model.md) |
| PWM 占空比使用定点（`PAL_PWM_DUTY_PCT/PERMILLE`），占空比禁浮点 | 确定、单位安全的执行器控制 | [ADR-0066](./docs/decisions/core/0066-pwm-basis-points-and-float-deprecation.md) |
| 分层与 API 形态由 YAML 规则在 CI 强制 | 保持 App/BAL/DAL/PAL 边界不腐化 | [ADR-0043](./docs/decisions/tools/0043-yaml-layer-lint.md) |

## 快速开始

### 1 · 零安装 —— 在浏览器里跑一个演示

1. 打开在线仿真器：**<http://www.wink-cloud.com/simulator/index.html>**
2. 导入本仓库（或仅导入 `wink-micro-app/mcs51_button_led/` 目录）
3. 运行 `button-led` 场景，按下虚拟按键，观察 LED 与实时引脚波形

### 2 · 安装 WinkCli（一次性）

下文所有构建 / 仿真 / 烧录命令均由 **WinkCli**（WinkMicroOS 工具链）驱动。安装一次即可：

```powershell
# 方式一 - winget（Windows 推荐）
winget install WinkAI.WinkCli

# 方式二 - GitHub Releases（离线 / 免包管理器）：
#   从 https://github.com/wink-cloud-hub/wink-ai-embedded/releases
#   下载 winkcli-v<version>-windows-x86_64.zip，解压后将 winkcli.exe 加入 PATH
```

> 完整安装与环境指引：[`wink-tools/docs/zh/00-install.md`](./wink-tools/docs/zh/00-install.md)。

### 3 · 本机构建与测试 —— 无需任何硬件

```bash
git clone https://github.com/wink-cloud-hub/wink-ai-embedded.git
cd wink-ai-embedded
winkcli test            # 日常门禁：host 构建 + 全量测试
winkcli test --clean    # 怀疑 CMake / 缓存污染时全量重建
```

> 底层即纯 CMake + CTest：`cmake -B build-host -DTARGET_PLATFORM=host && cmake --build build-host && ctest --test-dir build-host --output-on-failure`。
> 测试梯队、保真度保证与 MSVC 第二编译链：[`wink-micro-os/TESTING.md`](./wink-micro-os/TESTING.md)。

### 4 · 真机 —— 烧录 ESP32

```powershell
winkcli esp32 --app devkitc_smoke                       # 构建
winkcli esp32 --app devkitc_smoke -- -p COM3 flash monitor
```

> 需要经 Espressif IDE Manager 安装 ESP-IDF v6.x。详见 [`wink-firmware-carriers/esp32/README.zh_CN.md`](./wink-firmware-carriers/esp32/README.zh_CN.md)。

## 为 AI-in-the-loop 研发而设计

- **硬件可被机器读取**：设备树与板卡注册表是 JSON Schema，而不是原理图 PDF。
- **确定性激励**：场景脚本可注入按键弹跳、时序竞争、传感器断线与故障工况——固定种子、可回放。
- **结构化失败与证据**：故障码 + 环形 trace 缓冲（`SimTraceSpecV2`）直接指向根因，且每次运行都留下可回放的 PASS/FAIL 记录——而不是一张"烧了的板子"照片。
- **内置 Agent 指南**：[`AGENTS.md`](./AGENTS.md) 与 [`.agents/skills/`](./.agents/skills/) 描述了仓库约定、质量门禁与安全编辑规则，供 AI 编码助手遵循。

<!-- TODO: 若后续推出公开的 MCP Server 或 Agent CLI，在此补充"接入你的 Agent"代码段。不要承诺尚未交付的集成能力。 -->

## 文档导航

| 入口 | 内容 |
|---|---|
| [全局文档中心](./docs/README.md) | 文档拓扑、治理规范、CLI 查询工具 |
| [简体中文](./docs/zh/README.md) · [English](./docs/en/README.md) | 双语 SSOT（01–07 设计规范） |
| [Wasm 仿真（UniSim）](./docs/zh/design/04-wasm-simulation/00-README.md) | 仿真机制、保真度轴、保障体系 |
| [架构决策（ADR）](./docs/decisions/) | 按领域组织的决策记录 |
| [实施计划](./docs/implementation-plans/) · [评审记录](./docs/reviews/) | 执行流与验证记录 |

## 路线图

- 跨平台 winkcli 分发（Linux / macOS 二进制），让公共 CI 端到端构建固件
- 扩展 8/16 位芯片覆盖（更多应广 / 合泰 / 松翰型号）与板卡注册表
- 场景库扩充：故障注入、总线时序竞争、长时浸泡测试

当前里程碑与范围：[`docs/zh/design/01-system-overall/02-mvp-roadmap.md`](./docs/zh/design/01-system-overall/02-mvp-roadmap.md)。

## 贡献

欢迎贡献。除小修复外，请先开 Issue 对齐设计。新同学可以从 `good first issue` 标签开始，或先尝试新增一个板卡定义 / 器件驱动（位于 `wink-micro-app/`）。

使用 AI 编码助手？让它先阅读 [`AGENTS.md`](./AGENTS.md) 再动手改代码。

<!-- TODO(P2): 补充 CONTRIBUTING.md、CODE_OF_CONDUCT.md、SECURITY.md 与 Issue 模板。 -->

## 开源许可

分层许可（License Map）——运行时为 **LGPL-3.0-only**（你的固件可以保持闭源）；平台与文档默认 **GPL-3.0-only**：

| 范围 | 许可 |
|---|---|
| `wink-micro-os/**` 运行时（pal / dal / bal / runtime / trace / osal / targets / frameworks） | LGPL-3.0-only |
| `wink-micro-os/codegen/**`（驱动 / 角色描述与模板，生成物归用户） | Apache-2.0 |
| `wink-micro-app/**` 示例 · `wink-tools/tools/codegen/boards/**` | Apache-2.0 |
| `wink-firmware-carriers/**` | LGPL-3.0-only |
| `wink-tools/**`（其余） · `wink-plugin-peripherals/**` · 平台与文档 | GPL-3.0-only |
| `wink-micro-os/third_party/**`（ArduinoCore-API、Unity） | LGPL-2.1-or-later / MIT |

单一事实来源：[`.github/license-map.json`](./.github/license-map.json)，由 CI 强制校验。第三方组件归属：[`wink-micro-os/NOTICE`](./wink-micro-os/NOTICE)。
