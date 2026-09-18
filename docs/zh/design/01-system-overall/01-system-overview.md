# 01. 通用低代码 AI 嵌入式开发平台：平台系统级总体架构设计

> **核心愿景**：Wink-AI 是一个面向 AI 生成嵌入式应用的低代码开发、行为级高保真仿真与真机部署平台。通过将嵌入式研发全流程 Workflow 与物理环境全息数字化，构建高保真数字实验室（Digital Laboratory / Harness），使嵌入式软硬件研发流程接近 100% 脱离物理硬件环境。用户与 AI Agent 在浏览器中基于 WebAssembly 进行确定性沙箱验证、故障注入测试与 Golden Trace 一致性比对；验证通过后，通过云端隔离编译与 WebSerial/WebUSB 用户授权烧录到真实硬件，并通过虚实差异反馈迭代不断消除 Sim-to-Real 鸿沟，实现真正闭环的 AI 自主嵌入式研发。

---

## 1. 技术哲学与系统愿景：从垂直领域数字化到 AI 闭环研发

### 1.1 AI 工业落地的本质与物理世界数字化 Harness

当前人工智能与大模型技术正在经历从“信息交互”向“实体生产力”的历史性跃迁：
* **AI (LLM) 的本质**：是对人类**智力与认知能力（IQ/EQ）**的数字化、参数化与函数化。
* **具身智能 (Embodied AI) 的本质**：是对人类**五感感知输入与身体物理动作输出**完整工作流（Workflow）的数字化、参数化与函数化。
* **千行百业的数字化演进**：未来千行百业都必然经历各自领域研发 Workflow 与作业环境的全栈数字化。**因为只有将垂直领域的物理环境与作业流程彻底数字化，AI 才能真正成为生产力，而不是停留在聊天框里的玩具**。通过 AI Agent 与专用测试执行工程（Harness Engineering）的紧密咬合，垂直领域的工作流才能形成可自愈、可验证的生产力闭环。

**嵌入式研发的核心困境与突破口**：
* 纯软件（如 Web、后端）领域 AI 之所以能快速自闭环编程，是因为拥有完备的数字化沙箱（编译器报错、单元测试容器、CI 管道、运行时日志）。
* 传统嵌入式研发强依赖物理硬件：硬件不可逆、易损坏、缺少微观可观测性、无法高并发与快进运行（Fast-Forward），且物理激励获取成本极高。AI 生成的代码如同“盲人摸象”，无法得到确定性的物理因果反馈。
* **Wink-AI 的破局解法**：为嵌入式系统研发构建一个**全参数化、高保真、可快进、可注入异常的数字实验室（Digital Harness）**。让 AI 生成的固件在此虚拟沙箱中经历百万次迭代演进，最终让仿真与研发流程接近 100% 脱离物理硬件环境。通过构建“虚拟仿真 ➔ 证据链断言 ➔ 故障注入 ➔ 真机标定 (Model Calibration)”的反向反馈闭环，不断缩小 Sim-to-Real 差异，最终赋能 AI 真正实现嵌入式软硬件的自主研发闭环。

### 1.2 传统嵌入式开发瓶颈与 Wink-AI 解法

传统嵌入式开发存在以下五大核心瓶颈：
1. **硬件依赖重，开发门槛高**：开发者或 AI 生成器必须理解寄存器、引脚复用、电气时序和平台 SDK，导致业务逻辑难复用、难验证。
2. **AI 生成代码存在安全风险**：AI 生成 C 代码可能包含死循环、空指针、越界、错误状态机或危险控制逻辑，直接烧录真机风险极高。
3. **Web 端微观仿真性能低**：逐周期模拟 GPIO、I2C、UART 等高频波形会造成海量 JS/Wasm 跨端通信开销，浏览器性能不可接受。
4. **虚实一致缺少证据链**：仅凭视觉仿真无法证明真机行为与仿真一致，需要结构化 trace、输入回放和微秒级差异对比。
5. **工具链与烧录割裂**：用户需要安装不同芯片厂商的交叉工具链（ESP-IDF、ARM GCC）、驱动程序与专用烧录器，门槛极高。

Wink-AI 的系统级破局之道：
* **App/BAL/DAL/PAL 四层解耦**：用户业务逻辑、可复用算法库、器件语义和平台能力分离。
* **Device Model Registry 单一事实源**：统一外设模型、属性、引脚、DAL API、仿真策略、真机约束和代码生成。
* **数据面五通道旁路 (Channel-routed Bypass)**：Pin-level (通道1)、PWM Modulation (通道1b)、Protocol Bus (通道2)、Analog Signal (通道3)、Buffer Payload (通道4) 按场景分流；旁路全沉至 PAL 平台层，DAL/App 维持 100% 虚实同源代码。
* **安全沙箱链路**：App Safe Codegen、静态检查、Wasm Worker watchdog、隔离编译容器和固件 manifest。
* **Golden Trace 一致性验证与真机标定**：记录仿真与真机关键语义事件，支持回放、对比、CI 回归与模型参数校准。

> **术语澄清**：
> - ✅ **App 层**：用户代码/AI 生成的一次性业务逻辑（`app_init/app_loop/app_on_fault`）
> - ✅ **BAL 层**：Business Abstraction Layer（业务抽象层），包含 **物理增强**（`input` / `output` / `sensor` / `actuator` / `display` / `comm`）、**`math` 纯算法**、**`control` 闭环编排** 三大域，为 `wink-micro-os` 内核的核心组件库 (`wink-micro-os/bal/`)
> - ⚠️ **历史用法**：早期文档中的 "DAL Bypass / DAL 直通" 指早期整层 `#ifdef SIMULATION` 替换 DAL 驱动的行为；现已淘汰。现行 UniSim 3.0 规定 **旁路必须下沉至 PAL 平台层 (PAL Physical Source Bypass)**，DAL/App 100% 虚实同源。

---

## 2. 宏观四层系统数字化模型 (The 4-Layer Digital Twin Hierarchy)

为了让嵌入式研发完整脱离硬件依赖，平台将真实物理世界的机电与时序因果链全息映射为**四大核心数字化模型**：

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                              系统宏观四层数字化模型 (Digital Twin Hierarchy)             │
├────────────────────────────────────────────────────────────────────────────────────────┤
│  1. 芯片模型 (Chip & Core Model)                                                       │
│     - 数字化算力与时序基准：虚拟微秒时钟 (VirtualClock)、指令/沙箱执行、寄存器与中断 NVIC   │
├────────────────────────────────────────────────────────────────────────────────────────┤
│  2. 外设通道模型 (Peripheral Channel & Interconnect Model)                             │
│     - 数字化信号传输与电气时序：4 值逻辑仲裁 (PinArbiter 0/1/Z/X)、数据面五通道旁路分流    │
├────────────────────────────────────────────────────────────────────────────────────────┤
│  3. 外设模型 (Peripheral Device Model)                                                 │
│     - 数字化元器件机电转换规律：传感器采样与电气限位、执行器驱动特性、故障注入状态机   │
├────────────────────────────────────────────────────────────────────────────────────────┤
│  4. 外设与环境交互的物理模型 (Plant & Environment Physics Model)                       │
│     - 数字化物理世界法则与几何拓扑：运动学/动力学、声光热空间传播、碰撞检测、物理退化噪声  │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.1 四层数字化模型的内涵与因果链

1. **芯片模型 (Chip & Core Model)**：
   * **职责**：解决**算力、时钟基准与内核时序**的确定性数字化。
   * **实现**：包括 UniSim 微秒级虚拟时钟（`VirtualClock`）、Wasm 沙箱/Asyncify 挂起机制、中断分发器与指令级虚拟机（ADR-0064 Tier 1~4）。它保障在仿真世界中，代码的执行推进完全受虚拟时间轴绝对控制，消除跨宿主机的墙钟抖动。
2. **外设通道模型 (Peripheral Channel & Interconnect Model)**：
   * **职责**：解决**信号传输介质、总线协议与电气逻辑仲裁**的数字化。
   * **实现**：由 `PinArbiter` 提供支持 0/1/Z/X（高阻/未知）的电气逻辑模拟，以及数据面五通道旁路（Pin 电平、PWM 调制占空比、I2C/SPI/UART 协议总线、ADC 模拟量、Buffer 图像帧）。将高频硬件波形解析为轻量级语义事件，是连接虚拟芯片与虚拟外设的高速神经。
3. **外设模型 (Peripheral Device Model)**：
   * **职责**：解决**传感器、执行器等元器件内部机电转换特性**的参数化。
   * **实现**：统一定义于 [Device Model Registry](../07-platform-governance/01-device-model-registry.md)。数字化每个外设的电气约束、采样周期、舵机脉宽/转角转换方程、超声波发声盲区、按键机械抖动，以及断线/超时/漂移等可编程故障注入状态机。
4. **外设与环境交互的物理模型 (Plant & Environment Physics Model)**：
   * **职责**：解决**元器件与外部物理世界法则（力、热、光、电、声、几何空间）交互**的数学函数化。
   * **实现**：集成于前端 3D 产品世界（ProductWorld）与物理仿真插件（Simulation Plugins）。包括避障小车的两轮差速运动学方程、超声波发射在三维障碍物空间的飞行时间（ToF）射线检测、摩擦力与接触阻抗。这一层赋予了嵌入式控制代码真实的“物理环境反馈闭环”。
   * **设计规范归口**：规划归口于专属模块 `05-plant-and-environment/`（实施计划详见 [PLAN-20260919-PLANT-ENV-DOCS](../../implementation-plans/core/2026-09-19-plant-and-environment-design-docs-plan.md)）。

**物理世界因果驱动链**：
$$\text{芯片时钟与控制指令} \xrightarrow{\text{通道信号传输}} \text{外设机电动作} \xrightarrow{\text{物理法则作用}} \text{环境状态演化} \xrightarrow{\text{物理感知反馈}} \text{传感器采样} \xrightarrow{\text{通道数据回传}} \text{芯片中断与控制决策}$$

---

## 3. 系统总体分层与对偶双轮架构 (Dual-Wheel Architecture)

平台架构由**“嵌入式固件栈（运行于 MCU / Wasm 内部）”**与**“数字实验台栈（外部物理世界孪生）”**构成镜像对偶的双轮驱动体系：

```text
┌──────────────────────────────────────────────┐        ┌──────────────────────────────────────────────┐
│       嵌入式固件栈 (Embedded Firmware Stack)    │        │   数字实验台栈 (Harness Stack - 物理孪生沙箱)   │
│          运行于 Wasm / ESP32 内部             │        │            运行于 UniSim / 前端引擎           │
├──────────────────────────────────────────────┤        ├──────────────────────────────────────────────┤
│  App 层  (业务状态机、意图编排、决策回调)         │ ◄────► │  4. 物理环境交互模型 (运动学、空间几何、ToF)  │
│  BAL 层  (纯算法 math、闭环控制 control)     │ ◄────► │  3. 外设模型 (传感器/执行器机电特性、故障机)   │
│  DAL 层  (器件语义 API: dal_ultrasonic_read) │ ◄────► │  2. 外设通道模型 (数据面 5 通道、Pin 仲裁器)  │
│  PAL 层  (平台 HAL / OSAL: pal_gpio/timer)   │ ◄────► │  1. 芯片模型 (虚拟时钟、Wasm沙箱、中断/调度)  │
└──────────────────────────────────────────────┘        └──────────────────────────────────────────────┘
                        ▲                                                       ▲
                        └─────────────────── 虚实同源桥梁 ───────────────────────┘
                                   (Wasm-Bridge ABI / Device Registry)
```

### 3.1 跨栈协同与数据闭环
* **嵌入式固件栈（代码态）**：由 `App -> BAL -> DAL -> PAL` 构成，严格遵循虚实同源 C 源码规范。App 仅面向业务语义编排，芯片寄存器和总线细节下沉屏蔽。
* **实验台栈（孪生沙箱态）**：由 `芯片模型 -> 外设通道模型 -> 外设模型 -> 物理环境交互模型` 构成，为控制栈提供微秒级高保真虚拟环境与确定性激励。
* **握手与切换边界**：两栈在 **PAL (平台抽象层) 与 Wasm Bridge / Channel Arbiter** 处握手。
  - **仿真运行**：PAL 路由至 Wasm 平台层旁路，直接驱动外设通道模型与物理环境，生成微秒级 Golden Trace。
  - **真机运行**：PAL 静态绑定到物理芯片硬件驱动（ESP-IDF / STM32 HAL），控制真实物理世界，并通过 UART 吐出相同格式的 Trace。
  - **Sim-to-Real 模型标定 (Model Calibration)**：通过比对真机与仿真的 Trace 差异，反向微调外设模型与环境模型的参数（如传感器延迟漂移、电机真实死区），使数字实验室持续逼近物理真相。

### 3.2 总体系统交互架构全景

```mermaid
graph TD
    Input[AI / Low-Code 输入] --> SafeCodegen[wink CLI Codegen / 静态检查]
    SafeCodegen --> App[应用逻辑层 App]

    Registry[Device Model Registry<br>统一器件元数据] --> SafeCodegen
    Registry --> DeviceTree[device_tree 生成]
    Registry --> WebSchema[SchemaForm / 画布校验]
    Registry --> SimModel[外设仿真与故障模型]

    App -->|调用业务抽象| BAL[业务抽象层 BAL]
    BAL -->|器件语义 API| DAL[器件抽象层 DAL]
    DeviceTree --> DAL

    subgraph WinkMicroOS[WinkMicroOS Runtime (虚实同源 C 代码栈)]
        BAL
        DAL -->|总线与系统 API| PAL[平台抽象层 PAL]
        Trace[Golden Trace Runtime]
    end

    PAL -.->|PAL Wasm Target / Channel Bypass| WasmBridge[Wasm-JS Bridge]
    PAL -.->|真机静态绑定| Target[Target PAL: ESP32 / STM32]

    subgraph DigitalLab[UniSim 高保真数字实验室 (数字孪生栈)]
        WasmBridge --> Worker[@wink-ai/unisim Worker]
        Worker --> ChipSim[芯片模型: VirtualClock / 沙箱]
        Worker --> ChannelSim[通道模型: PinArbiter / 5 通道]
        ChannelSim --> PeripheralSim[外设模型: 虚拟外设 / 故障注入]
        PeripheralSim --> PlantPhysics[物理环境交互模型: 运动学 / 空间几何]
        PlantPhysics --> UI[@wink-ai/embedded-frontend 画布与 3D 世界]
    end

    subgraph CloudBuild[wink CLI / Cloud Build 隔离构建]
        BuildContainer[Isolated Build Environment] --> Firmware[Firmware + Manifest + sha256]
    end

    Target --> Hardware[物理 MCU 硬件]
    Firmware --> Flash[WebSerial / WebUSB Flash]
    Flash --> Hardware
    Trace --> Compare[Trace Replay / Sim-to-Real 差异比对与模型标定]
    Hardware -.->|真机 Trace 回灌| Compare
```

---

### 3.3 异构芯片仿真四层兼容体系 (Heterogeneous MCU Simulation Matrix)

为了同时兼顾 **AI 代码跨芯片生成**、**既有开源生态（Arduino/C51）零修改迁移** 与 **国产工业级极致低成本 8 位 OTP 芯片（义乌玩具、余慈小家电）** 的深度落地，平台将嵌入式仿真原理划分为四大演进层级（架构决议详见 [ADR-0064](../../decisions/unisim/0064-chip-simulation-four-tier-taxonomy.md)）：

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                              Tier 1: AI-Native 统一 OS 架构                            │
│           仿真与真机同源运行 wink-micro-os，用户层采用 Role-Action 语义 API (ESP32/STM32)        │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                       Tier 2: 源码零侵入 API / HAL 拦截代理架构                         │
│       仿真端 Wasm 拦截代理，真机端运行原生标准源码 (2-1: C51 C++ Proxy / 2-2: Arduino/HAL 拦截)   │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                         Tier 3: 1:1 指令级微内核解释仿真架构                           │
│             仿真端 1:1 实现专有 ISA 虚拟机，真机运行原生二进制 (应广 PDK、辉芒微 FMD)            │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                          Tier 4: 混合异构协同仿真架构 (Hybrid)                          │
│               CPU/时序 ➔ Tier 3 指令级虚拟机 ； 外设/大吞吐 ➔ Tier 2 C++ Proxy 高速通道          │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

#### 四类仿真模式对比与选型速查

| 分类 | 核心技术原理 | 典型芯片与生态 | 核心价值与适用场景 | 虚实同源与侵入性 |
| :--- | :--- | :--- | :--- | :--- |
| **Tier 1: AI-Native 统一 OS** | 仿真与真机均运行 `wink-micro-os`，PAL 平台层旁路物理源 | ESP32, STM32F4, RP2040, Linux | **AI 代码生成最友好**：上层业务基于 `Role-Action` 语义物理量编排，换芯免改业务代码 | 业务代码 100% 虚实同源 |
| **Tier 2: 源码零侵入代理** | 宿主端拦截 API/HAL 并打桩转发至 UniSim 总线 | 2-1: C51 系列<br>2-2: Arduino, STM32 HAL | **开源/遗留项目兼容**：用户不改一行现有 C/C++/Arduino 代码即可在 Web 端完成仿真 | 零侵入，真机仅跑用户标准源码 |
| **Tier 3: 1:1 指令级解释** | UniSim 内置专有 8/16 位 CPU 指令集解释器/虚拟机 | 应广 (PDK), 辉芒微 (FMD), 九齐 (Nyquest), 普冉 (Puya) | **工业级低成本芯片支持**：针对义乌玩具、余慈家电等 ROM < 2KB、无标准 C 库、依赖单周期精确时序的芯片 | 零侵入，真机烧录厂商原生 Hex/Bin |
| **Tier 4: 混合协同架构** | 核心时序走 ISA 解释器 + 复杂外设走 C++ Proxy | 异构多核玩具芯片、复杂工业控制器 | **兼具周期精度与运行性能**：解决全指令仿真性能瓶颈，支持高刷新外设与硬实时协同 | 零侵入 / 极低粘合开销 |

> 📌 **架构现行口径**：平台现行 MVP (Phase 0~1) 完整落地并主打 **Tier 1** 架构，为 AI 生成低代码应用提供最高置信沙箱；**Tier 2** 作为生态兼容层在 Phase 2 演进；**Tier 3 & Tier 4** 构成了平台切入千亿级国产消费电子产线的核心技术储备。

---

## 4. 跨仓五大核心模块全景卡片 (Cross-Repository 5-Core Pillars)

根据平台商业机密隔离规范与 Monorepo 物理拆分架构，系统由 5 个核心模块协同联动。非本仓外部模块（如 `embedded-frontend` 与 `unisim`）严格遵循**黑盒契约原则 (Black-Box Contract Insulation)**：**仅描述模块功能作用、使用场景、对外 API / DTO / CLI 契约与输入输出产物，不暴露主仓私有算法与商业实现细节**。

### 4.1 跨仓五大模块速查矩阵

| 模块名称 | 物理归属与路径 | 黑盒核心作用 | 典型使用场景与调用方式 | 接口与契约形式 | 商业与代码隔离边界 |
|---|---|---|---|---|---|
| **`embedded-frontend`** | 闭源 Monorepo | 嵌入式 Web 工作台 UI：2D 电路拓扑画布 (HCTR)、3D 产品世界机械/物理渲染、Pinia 状态树、构建烧录向导 | 开发者浏览器操作，或由 Wink-AI 主项目通过 iframe / 路由挂载消费 | `wink-app.json` Manifest、`SimTraceSpecV2`、WebSocket / Wasm 消息 DTO | 黑盒契约：定义 UI 交互与 Manifest DTO，隐藏私有渲染优化与商业编辑器逻辑 |
| **`unisim`** | 闭源 Monorepo | 统一 WebAssembly 行为级高保真仿真引擎：微秒级 `VirtualClock`、4 值逻辑仲裁 (0/1/Z/X)、中断/PWM/故障注入 Worker | 被 `embedded-frontend` 在 Web Worker 中加载，或由 `wink test` CLI 以 Headless 模式运行 | `SimWorker` 通信协议、Wasm-JS Bridge C-ABI (`wasm_bridge.h`) | 黑盒契约：定义引擎运行接口与 ABI 规范，隐藏内部高效状态机与转码优化实现 |
| **`wink-tools`** | 本仓<br>`wink-ai-embedded/wink-tools/` | 统一嵌入式 CLI 与开发工具链：涵盖代码生成 (`wink gen`)、静态 Lint (`wink lint`)、Headless 仿真测试 (`wink test`)、多端构建 (`wink build`)、打包烧录 (`wink pack`/`wink esp32`) | 开发者终端执行、CI/CD 自动化流水线、Web 后端构建 Worker 管道调用 | `wink <verb>` 动词指令集、JSON Telemetry Structured Envelope | 本仓开源/核心 CLI 工具链，公开完整 Python 实现与驱动描述 YAML 根 |
| **`wink-micro-os`** | 本仓<br>`wink-ai-embedded/wink-micro-os/` | C 语言轻量级嵌入式 SDK 内核：PAL/DAL/BAL 三层抽象、协作式 runtime 调度器、`wink_status_t`、Golden Trace 运行时 | 供 `wink-micro-app` 链接，经 `wink build` 构建为 ESP32/STM32 固件或 Wasm 仿真字节码 | C 公开头文件面 (`pal.h`/`dal_*.h`/`wink_bal_opts.h`)、CMake Targets | 本仓开源/核心 C SDK 内核，公开完整底层驱动抽象与调度主循环 |
| **`wink-micro-app`** | 本仓<br>`wink-ai-embedded/wink-micro-app/` | 嵌入式应用工程规范：Manifest (`wink-app.json`)、手写/AI 生成的 App C 代码 (`app_main.c`) 与生成的设备树 (`device_tree.c`) | 应用开发者或 AI 工具创建的逻辑工程，作为编译与仿真的顶层输入 | `wink-app.json` Schema v1/v2、`app_init` / `app_loop` 回调契约 | 本仓开源/工程模板，公开 App 生命周期规范与标准 Sample 库 |

---

### 4.2 模块详细使用与集成指南

#### 1. `embedded-frontend` (前端工作台)
* **作用**：提供专业级嵌入式 IDE 体验，支持 2D 电路连线、3D 机械物理联动渲染、属性编辑、AI 助手交互及一键编译烧录向导。
* **怎么用**：
  - 独立开发模式：在闭源工作台仓检出根运行 `bun run dev` 拉起 Vite 调试服务。
  - 宿主集成模式：通过 `<iframe src="/simulator/?projectId=xxx">` 嵌入主项目，通过 `window.postMessage` 交换 Project Manifest 数据。

#### 2. `unisim` (Wasm 仿真引擎)
* **作用**：在浏览器沙箱中行为级模拟 MCU 内核与虚拟外设，支持微秒级虚拟时钟、通道仲裁、中断与故障注入。
* **怎么用**：
  - 前端渲染模式：由 `embedded-frontend` 的 `SimWorker` 实例化 Wasm 模块并建立二进制通信通道。
  - Headless CI 模式：通过 `wink test` 或 Node.js 环境调用 `headless-sim-runner` 运行自动化规范断言。

#### 3. `wink-tools` (统一开发与构建 CLI)
* **作用**：作为嵌入式开发的核心指挥官，集代码生成、静态检查、Headless 仿真测试、云端/本地构建打包与 ESP32/STM32 烧录于一体。
* **怎么用**：
  - 命令行交互：
    ```bash
    wink doctor                       # 诊断本地环境与工具链
    wink gen app                      # 由 Manifest 生成 app_main.c / device_tree.c
    wink lint --strict                # 执行 P-stack / packed 禁令 / 大括号静态检查
    wink build -t esp32               # 编译目标固件
    wink test                         # 运行 Headless 仿真测试与一致性断言
    ```

#### 4. `wink-micro-os` (C 语言 SDK 内核)
* **作用**：屏蔽芯片寄存器与总线差异，为 App 提供稳定统一的器件 API (DAL) 与业务抽象 (BAL)，保证仿真与真机同源运行。
* **怎么用**：
  - 在应用工程的 `CMakeLists.txt` 中通过 `add_subdirectory(wink-micro-os)` 引入，按需 link `libpal`、`libdal` 与 `libbal`。

#### 5. `wink-micro-app` (应用工程规范)
* **作用**：定义单次嵌入式项目的标准物理边界，包含权威清单 `wink-app.json` 及生成的业务逻辑源码。
* **怎么用**：
  - 在 `wink-micro-app/` 目录下放置 `wink-app.json`，运行 `wink build` 直接构建可执行程序。

---

## 5. 分层职责 (Layer Responsibilities)

| 分层 | 核心职责 | 主要产物 | 受众 |
|---|---|---|---|
| AI/Low-Code | 生成业务意图、状态机、外设拓扑 | DSL、Blockly、App 草稿 | 普通用户、AI 助手 |
| App | 描述业务状态机和控制策略，不接触硬件总线 | `app_init/app_loop/app_on_fault` | 应用开发者 |
| BAL | 封装物理增强、算法与闭环控制（物理增强/math/control三域） | `wink_bal_opts.h`、`wink_xxx_*` | 算法/组件开发者 |
| DAL | 提供器件语义 API，屏蔽寄存器、总线和时序 | `dal_xxx_read/set` | 驱动维护者 |
| PAL | 抽象 GPIO/PWM/I2C/SPI/ADC/OSAL | `pal_hal.h`, `pal_osal.h` | 平台适配者 |
| runtime | 协作式主循环、App 生命周期调度（回调注入） | `wink_runtime_run`、`wink_app_callbacks_t` | 平台适配者/应用开发者 |
| trace | Golden Trace 故障/事件记录（横切基础服务） | `wink_trace_fault` | 测试工程师 |
| Device Model Registry | 统一外设、板卡、仿真、故障、代码生成元数据 | JSON Schema、模型库 | 架构师、生态开发者 |
| UniSim | 浏览器虚拟外设、画布、协议解析、故障注入 | TS 运行库、SchemaForm | 前端工程师 |
| Cloud Build | 隔离编译、缓存、产物签名、manifest | `.bin/.hex`, build log | DevOps/平台工程师 |
| Trace System | 记录、回放、对比仿真和真机行为 | Golden Trace | 测试工程师 |

### 5.1 与经典嵌入式四层架构（ops 表多态）的映射

> 本节澄清 App/BAL/DAL/PAL 四层与 `embedded-best-practice` 经典四层架构（应用层 / 抽象层 ops 表 / 实现层 / 注册层 + Platform 层）的关系。本平台是**范式重构而非一一对应**，特此声明，避免实现者按"全新四层 ops 架构"误解。
>
> 详见 [2026-06-22 评审报告 §2.1](../../reviews/core/2026-06-22-architecture-review.md) 与 [`02-wink-micro-os/01-dal-device-abstraction.md §2.1`](../02-wink-micro-os/01-dal-device-abstraction.md)。

PAL 采用 CMake 静态直调（符合 Platform 层惯例）；DAL 放弃运行期 ops 表多态、改用命名式扁平 API + 编译期路由（换取 AI 可生成性与仿真性能）；注册层职责由 `device_tree` 代码生成承担。

| 经典四层架构机制 | 本平台落地 | 关系 |
|---|---|---|
| 应用层（只拿句柄、不知子类） | App（只 include `device_tree.h`、只调 `bal_xxx` / `dal_xxx`） | ✅ 契约一致，换硬件 App 零修改 |
| 抽象层 ops 表多态（`me->ops->on(me)`） | DAL 命名式扁平 API | ⚠️ 范式重构 |
| `container_of` 反推子类 | 无（DAL 无父子结构） | ⚠️ 主动放弃 |
| 实现层填 ops 表 | 每器件独立 `.c` + 静态分发 | ⚠️ 编译期路由替代运行期分发 |
| 注册层（`board_init` / `MODULE_INIT`） | `device_tree.c` 代码生成 | 🔄 替换为生成式 |
| Platform 层静态直调 | PAL CMake 静态绑定 | ✅ 一致（对齐同工作区 HAL 静态直调方针） |

**取舍理由**：MVP 范围内同器件类型通常单硬件实现，无需运行时多态；命名式 API 对 AI 生成更友好、更可静态校验；静态分发零运行期开销，利于 Wasm 仿真性能与代码体积。**代价**是放弃"统一 device 模型"的可扩展性——加新器件需加整套独立 API，而非只填一张 ops 表。

---

## 6. 虚实双模运行核心机制

### 6.1 网页端仿真模式

1. 用户生成 App 和拓扑后，平台先执行 App 静态安全检查。
2. Device Model Registry 生成 `device_tree.c/h`、SchemaForm 属性、仿真注册信息和故障模型。
3. App、BAL、DAL、PAL Wasm target 编译为 `wasm32`。
4. 前端启动 Web Worker 运行 Wasm，主线程保持 UI 渲染。
5. Asyncify 处理 `pal_delay_ms` 等阻塞延时，watchdog 防止死循环和资源失控。
6. UniSim 通过数据面五通道（Pin / PWM / Protocol Bus / Analog / Buffer）及 PAL 平台层旁路与 Wasm 交互。
7. 仿真过程写入 Golden Trace，可用于回放、故障测试和 CI 回归。

### 6.2 真机部署模式

1. 仿真和必要故障测试通过后，用户选择目标板卡。
2. 云端编译服务在隔离容器中拉起对应 toolchain。
3. 编译器链接 App、BAL、DAL、PAL target 和设备树，生成固件。
4. 返回 firmware、sha256、build manifest 和编译日志。
5. 浏览器通过 WebSerial/WebUSB 请求用户授权，执行烧录。
6. 真机运行 WinkMicroOS，并可通过 UART 输出 trace 与仿真 trace 对比。

---

## 7. 仿真精度边界

Wink-AI 的主目标是**行为级高保真仿真**，不是全电气级仿真。

| 仿真级别 | 是否 MVP 主打 | 说明 |
|---|---|---|
| 行为级仿真 | 是 | 验证业务状态机、传感器语义值、执行器命令 |
| 协议级仿真 | 是 | 验证 I2C/UART/SPI payload 级交互 |
| 电平级仿真 | 部分支持 | 支持 LED、Button、简单 GPIO |
| 电气级/SPICE 仿真 | 否 | 不模拟电流、阻抗、噪声、电源完整性 |
| 指令级 MCU 仿真 | 否 | 不运行 QEMU/AVR/RP2040 指令模拟器作为主路径 |

产品表述应避免承诺“100% 替代真实硬件验证”。推荐表述：

> Wink-AI 提供行为级高保真仿真，帮助用户在真实烧录前验证业务逻辑、外设交互和异常处理。

---

## 8. 安全与可信链路

```text
S0 未检查代码
 ↓ 静态规则通过
S1 可仿真
 ↓ Wasm 沙箱 + watchdog 通过
S2 可编译
 ↓ 隔离容器编译 + manifest 完整
S3 可烧录
 ↓ 真机 trace 正常
S4 已验证配置
```

关键约束：

1. AI 生成代码默认不可信。
2. BAL 禁止直接调用 PAL。
3. 忽略 `wink_status_t` 返回值属于阻断错误。
4. Wasm Worker 必须有 heartbeat 和强制 terminate 能力。
5. 云端编译容器不得挂载密钥，不得默认访问外网。
6. 烧录必须由用户通过浏览器授权。

---

## 9. 核心商业与技术价值

1. **降低嵌入式原型试错成本**：用户先在浏览器中验证控制逻辑和外设交互，再进入硬件阶段。
2. **让 AI 生成代码可控可审计**：静态检查、沙箱、故障注入和 trace 让 AI 代码从“能生成”升级为“可验证”。
3. **统一虚拟和真实执行路径**：App 同源运行，BAL/DAL/PAL 分层隔离，减少平台迁移成本。
4. **性能优先的浏览器仿真**：通过协议旁路与 PAL 物理源旁路绕开微观波形开销。
5. **产品闭环完整**：从需求、画布、仿真、编译、烧录到真机 trace 对比，形成端到端体验。

---

## 10. MVP 聚焦范围

第一阶段建议聚焦：

| 范围 | 内容 |
|---|---|
| 目标板 | ESP32 DevKit V1 / STM32F4 |
| 外设 | LED、Button、Servo、HC-SR04、SSD1306 OLED |
| 总线 | GPIO、PWM、I2C |
| 仿真 | Wasm Worker、Asyncify、PAL 物理源旁路、Protocol Bypass |
| 安全 | App 静态检查、Worker watchdog、错误状态码 |
| 部署 | 云端 ESP-IDF 编译、Chrome/Edge WebSerial 烧录 |
| 验证 | Golden Trace 基础事件、故障注入 timeout/disconnect |

暂缓 STM32/RP2040、多板通信、复杂 3D 机械臂、ngspice 电气仿真和完整 WebUSB DFU。



