# 仿真总体架构与联合仿真模型

| 项 | 内容 |
|---|---|
| 文档层级 | ① 设计规范（UniSim 3.0 / overview） |
| 文档状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| **落地** | **Landed**（架构总览与代码地图；不含未落地能力承诺） |
| 管辖 ADR | [0002](../../../decisions/unisim/0002-dual-target-compilation.md)、[0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)、[0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md)、[0042](../../../decisions/unisim/0042-sim-execution-modes.md)、[0064](../../../decisions/unisim/0064-chip-simulation-four-tier-taxonomy.md) |
| 关联代码 | `wink-micro-os/osal/wasm/`、`wink-micro-os/targets/wasm/`、`wink-micro-os/targets/common/`、`@wink-ai/unisim`（独立 SDK 依赖包，按导出模块契约规范） |
| 上次核对 | 2026-09-01（补齐 ADR-0064 异构仿真四层体系映射） |

> 本文件回答：仿真引擎由哪些层组成、它们如何交换数据、代码分别在哪里。具体机制（时钟/调度/中断/故障/外设）见 [`02-mechanisms/`](../02-mechanisms/00-README.md) 专题文档。

---

## 1. 分层架构总览

```text
┌────────────────────────────────────────────────────────────┐
│       Vue 3 主线程（画布 / ControlHub / ProductWorld*）     │  ← 轴 A 注入与观测
└───────────────────────────────▲────────────────────────────┘
                                │ postMessage
                                ▼
┌────────────────────────────────────────────────────────────┐
│             Web Worker（SimWorker + Plugins）              │
│  PinArbiter / I2C·SPI·UART Bus / VirtualClock / Fault     │  ← 轴 A/B/F (通用仿真底座)
│  ┌──────────────────────┐   ┌──────────────────────────┐  │
│  │   Wasm-Core (C OS)   │──►│   Wasm JS Bridge         │  │
│  │ App/BAL/DAL + PAL API│   │ Asyncify · js_pal_*      │  │  ← 轴 D/E (Tier 1 现行实现)
│  │ OSAL 协作调度        │   │ InterruptQueue (Poll)    │  │
│  └──────────────────────┘   └──────────────────────────┘  │
└────────────────────────────────────────────────────────────┘
```

关键边界与四层仿真定位（详见 [ADR-0064](../../../decisions/unisim/0064-chip-simulation-four-tier-taxonomy.md)）：

- **UniSim 与四层体系关系**：UniSim 现行主干完整承载 **Tier 1 (AI-Native 统一 OS 架构)**；其底座（`VirtualClock`、`PinArbiter`、总线与故障注入）同时具备通用性，未来演进中可无缝挂载 **Tier 2 (Arduino/C51 代理桩)**、**Tier 3 (应广 PDK / 辉芒微专有 ISA 解释模块)** 及 **Tier 4 (混合协同加速)**。
- **App / BAL / DAL 100% 同源 C 代码**（Tier 1 铁律），同一份源码编译到 wasm32 与 xtensa（ESP32）。App 层禁止 `#ifdef SIMULATION` 改业务分支（[ADR-0002](../../../decisions/unisim/0002-dual-target-compilation.md)、C1.1）。
- **PAL 是唯一合法的旁路落点**。物理量来源（引脚电平、脉宽、总线从机响应字节、ADC 原值、缓冲内容）在 PAL Wasm 实现处被替换；DAL 目标是零仿真宏（见 [`08-channel-routing`](../02-mechanisms/08-channel-routing.md)）。
- **内核零业务物理痕迹**。小车运动学、传感器退化等物理算法只存在于 JS Plugin，不进 C 内核。
- **\* ProductWorld（3D 物理/可视化）仍在规划/进行中**，非已落地组件；其余主线程画布与 ControlHub 已用于轴 A 注入观测。物理量须经引脚/ADC 注入，不得直连 DAL。

---

## 2. 控制域与物理域解耦（Co-Simulation）

```text
┌────────────────────────────────────────────────────────┐
│             应用控制域 (App Control Domain)            │  ◄── 100% C 业务代码（同源）
└───────────────────────────┬────────────────────────────┘
                            ▼
┌────────────────────────────────────────────────────────┐
│             平台仿真底座 (Platform Sim OS)             │  ◄── 领域无关
│   时钟、调度、虚拟引脚、中断 Poll、通信总线、配额/Fault  │
└───────────────────────────┬────────────────────────────┘
                            ▲ (管脚/总线双向交换)
                            ▼
┌────────────────────────────────────────────────────────┐
│           外部环境/器件模型 (Simulation Plugins)       │  ◄── 物理算法插件
│    运动学/动力学、传感器退化状态机、显示帧解析           │
└────────────────────────────────────────────────────────┘
```

三层各自职责：

1. **应用控制域**：App/BAL/DAL 的状态机、控制律、协议打包解析。这是同源测试的主要资产。
2. **平台仿真底座**：提供 `s_virtual_us` 虚拟时钟、协作调度器、虚拟引脚/总线、IRQ Poll、内存配额与 Fault 隔离。它不知道"超声波""小车"为何物。
3. **插件域**：把控制信号（PWM/GPIO 输出）经 Δt 物理更新换算为传感器量，再写回底座（距离、电压、寄存器响应字节）。

### 2.1 平台数据面（Data Plane）

C 侧通过 `EMSCRIPTEN_KEEPALIVE` 导出给 JS 的观测/注入 API（完整清单见 [`10-wasm-js-bridge-abi`](../02-mechanisms/10-wasm-js-bridge-abi.md)）：

| 方向 | 代表 API | 用途 |
|---|---|---|
| 导出（C→JS 观测） | `pal_wasm_get_gpio_output`、`pal_wasm_get_pwm_duty_percent`、`pal_wasm_get_servo_angle` | 读取固件产生的控制信号 |
| 导入（JS→C 注入） | `pal_wasm_set_gpio_input`、`pal_wasm_set_ultrasonic_distance`、`pal_wasm_push_pin_event` | 注入物理量/边沿 |
| 控制 | `pal_wasm_advance_virtual_clock`、`pal_wasm_reset_physical`、`pal_wasm_set_sim_mode` | 时钟步进、复位、执行模式 |

数据面保持"内核无业务物理"：例如 3D 碰撞检测算出的距离，必须由插件转成 ECHO 边沿或 ADC 原值注入，**不得**直接作为 DAL 返回值（C1.2、[08-channel-routing §4](../02-mechanisms/08-channel-routing.md)）。

### 2.2 联合仿真步进契约（Step-Lock Pipe）

每个仿真步：

1. 插件读取控制信号（PWM/GPIO 输出）；
2. 按 `Δt`（绑定 `s_virtual_us`）更新物理状态并计算传感器量；
3. 经注入 API 写回底座。

锁步破坏（plant 读墙钟、plant 与 OS 用不同 Δt）会导致不可复现，归 C14。可视化插值允许，但**不得回写控制决策**。

### 2.3 确定性与快进（仿真自身口径）

> 本节是**仿真运行之间**的口径，与 [`03-production-contract.md`](./03-production-contract.md) 的「虚实之间 ≠ 恒等」是两回事，勿混。

- **确定性（目标承诺）**：固定输入 + 固定 unisim/C 构建版本 + 固定 PRNG seed → **事件序与逻辑结果跨运行可复现**。底座是 `s_virtual_us` 单一 Gate（[`02-virtual-clock`](../02-mechanisms/02-virtual-clock.md)）、协作调度器确定序（[`03-scheduler-and-concurrency`](../02-mechanisms/03-scheduler-and-concurrency.md)）、以及带 seed 的确定性 LCG（`wink_phys_prng_next`，噪声/丢包据此可复现）。
- **不承诺的部分**：跨浏览器/JS 引擎的**浮点 bit 级一致**不做承诺；逻辑时序靠虚拟时钟，**不靠墙钟对齐**（宿主 Asyncify 挂起点、渲染节流只改墙钟观感，不改事件序）。
- **快进**：`HEADLESS` 执行模式下 C 侧直接跳虚拟时钟、不进入 Asyncify unwind/rewind，故可快于墙钟用于 CI 批跑；`INTERACTIVE` 受渲染/交互节流，不承诺快于实时。具体预算口径见 unisim 性能预算与时间契约规范，机制见 [`01-sandbox-and-execution`](../02-mechanisms/01-sandbox-and-execution.md)。

---

## 3. 执行拓扑：Worker 与 Wasm 实例

- **Wasm 必须运行在 Web Worker 中**，主线程只做消息驱动的渲染。主线程直接加载 Wasm + Asyncify 会导致定时器饿死与 OOM（[`01-sandbox-and-execution` §1](../02-mechanisms/01-sandbox-and-execution.md)、C15.5）。
- 一个 Worker 持有一个 Wasm 实例与一套 SimWorker 编排（VirtualClock、PinArbiter、Bus、Fault 桥）。
- Node 侧烟测（`wink_sim_stub.js`）必须用 `worker_threads.Worker` 加载，不能在主线程——Emscripten 6.x Asyncify 的 unwind→rewind 与 Node 主事件循环共存会饿死定时器并 OOM。

---

## 4. 代码地图（已核对真实路径）

### 4.1 C 侧（wink-micro-os）

| 路径 | 职责 |
|---|---|
| `wink-micro-os/osal/wasm/pal_osal_wasm.c` | Wasm OSAL：`s_virtual_us` 时钟、任务/睡眠对接调度器、`wink_vclock_advance_internal` 单 Gate、HEADLESS 时钟跳跃 |
| `wink-micro-os/osal/host/pal_osal_host.c` | Host 原生 OSAL（Win32 Fibers + host 虚拟时钟），与 wasm 语义对齐 |
| `wink-micro-os/targets/wasm/` | Wasm target 适配：`pal_hal_wasm.c`（GPIO/PWM/I2C/SPI/UART）、`pal_irq_wasm.c`（IRQ Poll/FIFO）、`pal_wasm_physical.c`（退化引擎）、`pal_wasm_fault.c`（Fault 锁存/safe-off）、`pal_wasm_fault_domain.c`（故障域/功耗 stub）、`pal_log_wasm.c`、`pal_storage_wasm.c` |
| `wink-micro-os/targets/wasm/devices/` | `wasm_dev_ultrasonic.c`、`wasm_dev_servo.c`、`wasm_sim_registry.c`（**注意**：超声波含 deprecated cm→µs 捷径，见 [`08-channel-routing` §5.1](../02-mechanisms/08-channel-routing.md)） |
| `wink-micro-os/targets/wasm/wasm_bridge.h` | **Wasm↔JS ABI SSOT**，所有 `js_pal_*`/`pal_wasm_*` 声明与 6 条 ABI 契约集中于此 |
| `wink-micro-os/targets/wasm/wink_sim_js.js` | `--js-library` 默认桩（wrapper 模式 + `__async:'auto'`） |
| `wink-micro-os/targets/wasm/exported_runtime_functions.json` | 链接期导出/Asyncify 配置 SSOT（`EXPORT_NAME=WasmSandbox` 等） |
| `wink-micro-os/targets/common/include/`、`src/` | target 无关算法：`wink_sim_scheduler.*`（协作调度器）、`wink_sim_physical.*`（退化算法：去抖/RC/丢包/PRNG）、`sim_ctx.h`（Fiber 抽象）、`pal_resource.c`（资源声明） |

### 4.2 TypeScript 侧（@wink-ai/unisim 仿真引擎底座）

> 契约包：`@wink-ai/unisim`（前端仿真 SDK 运行库，提供确定性虚拟时钟、电气仲裁、Worker 编排与物理退化通道）。

| 逻辑模块 | 职责与能力契约 |
|---|---|
| `VirtualClock (虚拟时钟引擎)` | μs 精度 bigint 确定性虚拟时钟（JS 镜像） |
| `PinArbiter (电气仲裁器)` | 4 值逻辑 + 3 级驱动强度引脚仲裁（电气 SSOT） |
| `PeripheralRegistry (外设注册表)` | 虚拟外设类型注册与生命周期管理 |
| `Observability Suite (可观测性组件)` | 引脚追踪 (Tracer)、VCD 导出 (Exporter)、会话录制 (Recorder)、断点控制 (Debugger) |
| `Wasm Bridge (Wasm 桥接与总线模块)` | `js_pal_*` 导入工厂、I2C/SPI/UART 总线驱动、中断队列及导出转换器 |
| `SimWorker (Worker 调度编排器)` | Web Worker 编排器（前端 Worker 唯一入口） |
| `Physical & Fault Bridge (物理与故障桥)` | 物理退化桥与故障注入（对接 C 侧 `pal_wasm_set_*`） |
| `Fault Composer (故障合成器)` | 噪声 (Noise)、延迟 (Delay)、丢包 (Drop) 故障注入模块 |
| `Bus Analyzer (总线分析器)` | I2C / SPI / UART 总线事务抓包分析 |
| `Types & ABI Spec (类型与 ABI 契约)` | 引脚逻辑类型、WASM 导入/导出契约、运行时状态与时间契约定义 |

公开 API 由 `@wink-ai/unisim` 统一导出。

---

## 5. 同源编译与双 target

- C 业务代码同时编译到 `wasm32-unknown-emscripten` 与 `xtensa`（ESP-IDF）。Host 原生编译用于 CI/调试（[ADR-0002](../../../decisions/unisim/0002-dual-target-compilation.md)）。
- 平台差异隐藏在 `sim_ctx_*`（Fiber）、`pal_osal_*`、`pal_hal_*` 之后；上层 API 签名一致。
- 零编译污染：退化/仿真专用代码（`wink_sim_physical`、`pal_wasm_*`）不进 esp32/baremetal 构建，由各 target CMake 显式枚举源文件（非 glob）保证。

