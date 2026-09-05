# 01. 通用低代码 AI 嵌入式开发平台：平台系统级总体架构设计

> **核心愿景**：Wink-AI 是一个面向 AI 生成嵌入式应用的低代码开发、行为级仿真与真机部署平台。用户通过拖拽可视化组件或 AI 生成业务逻辑，在浏览器中基于 WebAssembly 进行安全沙箱验证、故障注入测试和 Golden Trace 一致性追踪；验证通过后，再通过云端隔离编译与 WebSerial/WebUSB 用户授权烧录到真实开发板。

---

## 1. 平台核心痛点与解决之道

传统嵌入式开发存在以下瓶颈：

1. **硬件依赖重，开发门槛高**：开发者或 AI 生成器必须理解寄存器、引脚复用、电气时序和平台 SDK，导致业务逻辑难复用、难验证。
2. **AI 生成代码存在安全风险**：AI 生成 C 代码可能包含死循环、空指针、越界、错误状态机或危险控制逻辑，直接烧录真机风险极高。
3. **Web 端微观仿真性能低**：逐周期模拟 GPIO、I2C、UART 等波形会造成高频 JS/Wasm 通信，浏览器性能不可接受。
4. **虚实一致缺少证据链**：仅凭视觉仿真无法证明真机行为与仿真一致，需要结构化 trace、回放和对比。
5. **工具链与烧录割裂**：用户需要安装 ESP-IDF、ARM GCC、驱动、烧录工具，阻碍低门槛使用。

Wink-AI 的解法：

* **App/BAL/DAL/PAL 四层解耦**：用户业务逻辑、可复用算法库、器件语义和平台能力分离。
* **Device Model Registry 单一事实源**：统一外设模型、属性、引脚、DAL API、仿真策略、真机约束和代码生成。
* **数据面五通道旁路 (Channel-routed Bypass)**：Pin-level (通道1)、PWM Modulation (通道1b)、Protocol Bus (通道2)、Analog Signal (通道3)、Buffer Payload (通道4) 按场景分流；旁路全沉至 PAL 平台层，DAL/App 维持 100% 虚实同源代码。
* **安全沙箱链路**：App Safe Codegen、静态检查、Wasm Worker watchdog、隔离编译容器和固件 manifest。

> **术语澄清**：
> - ✅ **App 层**：用户代码/AI 生成的一次性业务逻辑（`app_init/app_loop/app_on_fault`）
> - ✅ **BAL 层**：Business Abstraction Layer（业务抽象层），包含 **物理增强**（`input` / `output` / `sensor` / `actuator` / `display` / `comm`）、**`math` 纯算法**、**`control` 闭环编排** 三大域，为 `wink-micro-os` 内核的核心组件库 (`wink-micro-os/bal/`)
> - ⚠️ **历史用法**：早期文档中的 "DAL Bypass / DAL 直通" 指早期整层 `#ifdef SIMULATION` 替换 DAL 驱动的行为；现已淘汰。现行 UniSim 3.0 规定 **旁路必须下沉至 PAL 平台层 (PAL Physical Source Bypass)**，DAL/App 100% 虚实同源。
* **Golden Trace 一致性验证**：记录仿真与真机关键语义事件，支持回放、对比和 CI 回归。

---

## 2. 系统总体分层架构

```mermaid
graph TD
    Input[AI / Low-Code 输入] --> SafeCodegen[wink CLI Codegen / 静态检查]
    SafeCodegen --> App[应用逻辑层 App]

    Registry[Device Model Registry] --> SafeCodegen
    Registry --> DeviceTree[device_tree 生成]
    Registry --> WebSchema[SchemaForm / 画布校验]
    Registry --> SimModel[仿真模型 / 故障模型]

    App -->|调用业务抽象| BAL[业务抽象层 BAL]
    BAL -->|器件语义 API| DAL[器件抽象层 DAL]
    DeviceTree --> DAL

    subgraph WinkMicroOS[WinkMicroOS Runtime (虚实同源 C 代码)]
        BAL
        DAL -->|总线与系统 API| PAL[平台抽象层 PAL]
        Trace[Golden Trace Runtime]
    end

    PAL -.->|PAL Wasm Target / Channel Bypass| WasmBridge[Wasm-JS Bridge]
    PAL -.->|真机静态绑定| Target[Target PAL: ESP32 / STM32]

    subgraph Monorepo[Wink-AI Monorepo Frontend & Sim]
        WasmBridge --> Worker[@wink-ai/unisim Worker]
        Worker --> Watchdog[Watchdog / Resource Limit]
        Worker --> UniSim[UniSim Virtual Peripherals]
        UniSim --> UI[@wink-ai/embedded-frontend Canvas]
    end

    subgraph CloudBuild[wink CLI / Cloud Build]
        BuildContainer[Isolated Build Environment] --> Firmware[Firmware + Manifest + sha256]
    end

    Target --> Hardware[Physical MCU]
    Firmware --> Flash[WebSerial / WebUSB Flash]
    Flash --> Hardware
    Trace --> Compare[Trace Replay / Compare]
```

---

### 2.1 异构芯片仿真四层兼容体系 (Heterogeneous MCU Simulation Matrix)

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

## 3. 跨仓五大核心模块全景卡片 (Cross-Repository 5-Core Pillars)

根据平台商业机密隔离规范与 Monorepo 物理拆分架构，系统由 5 个核心模块协同联动。非本仓外部模块（如 `embedded-frontend` 与 `unisim`）严格遵循**黑盒契约原则 (Black-Box Contract Insulation)**：**仅描述模块功能作用、使用场景、对外 API / DTO / CLI 契约与输入输出产物，不暴露主仓私有算法与商业实现细节**。

### 3.1 跨仓五大模块速查矩阵

| 模块名称 | 物理归属与路径 | 黑盒核心作用 | 典型使用场景与调用方式 | 接口与契约形式 | 商业与代码隔离边界 |
|---|---|---|---|---|---|
| **`embedded-frontend`** | Monorepo<br>`wink-ai/packages/embedded-frontend/` | 嵌入式 Web 工作台 UI：2D 电路拓扑画布 (HCTR)、3D 产品世界机械/物理渲染、Pinia 状态树、构建烧录向导 | 开发者浏览器操作，或由 Wink-AI 主项目通过 iframe / 路由挂载消费 | `wink-app.json` Manifest、`SimTraceSpecV2`、WebSocket / Wasm 消息 DTO | 黑盒契约：定义 UI 交互与 Manifest DTO，隐藏私有渲染优化与商业编辑器逻辑 |
| **`unisim`** | Monorepo<br>`wink-ai/packages/unisim/` | 统一 WebAssembly 行为级高保真仿真引擎：微秒级 `VirtualClock`、4 值逻辑仲裁 (0/1/Z/X)、中断/PWM/故障注入 Worker | 被 `embedded-frontend` 在 Web Worker 中加载，或由 `wink test` CLI 以 Headless 模式运行 | `SimWorker` 通信协议、Wasm-JS Bridge C-ABI (`wasm_bridge.h`) | 黑盒契约：定义引擎运行接口与 ABI 规范，隐藏内部高效状态机与转码优化实现 |
| **`wink-tools`** | 本仓<br>`wink-ai-embedded/wink-tools/` | 统一嵌入式 CLI 与开发工具链：涵盖代码生成 (`wink gen`)、静态 Lint (`wink lint`)、Headless 仿真测试 (`wink test`)、多端构建 (`wink build`)、打包烧录 (`wink pack`/`wink esp32`) | 开发者终端执行、CI/CD 自动化流水线、Web 后端构建 Worker 管道调用 | `wink <verb>` 动词指令集、JSON Telemetry Structured Envelope | 本仓开源/核心 CLI 工具链，公开完整 Python 实现与驱动描述 YAML 根 |
| **`wink-micro-os`** | 本仓<br>`wink-ai-embedded/wink-micro-os/` | C 语言轻量级嵌入式 SDK 内核：PAL/DAL/BAL 三层抽象、协作式 runtime 调度器、`wink_status_t`、Golden Trace 运行时 | 供 `wink-micro-app` 链接，经 `wink build` 构建为 ESP32/STM32 固件或 Wasm 仿真字节码 | C 公开头文件面 (`pal.h`/`dal_*.h`/`wink_bal_opts.h`)、CMake Targets | 本仓开源/核心 C SDK 内核，公开完整底层驱动抽象与调度主循环 |
| **`wink-micro-app`** | 本仓<br>`wink-ai-embedded/wink-micro-app/` | 嵌入式应用工程规范：Manifest (`wink-app.json`)、手写/AI 生成的 App C 代码 (`app_main.c`) 与生成的设备树 (`device_tree.c`) | 应用开发者或 AI 工具创建的逻辑工程，作为编译与仿真的顶层输入 | `wink-app.json` Schema v1/v2、`app_init` / `app_loop` 回调契约 | 本仓开源/工程模板，公开 App 生命周期规范与标准 Sample 库 |

---

### 3.2 模块详细使用与集成指南

#### 1. `embedded-frontend` (前端工作台)
* **作用**：提供专业级嵌入式 IDE 体验，支持 2D 电路连线、3D 机械物理联动渲染、属性编辑、AI 助手交互及一键编译烧录向导。
* **怎么用**：
  - 独立开发模式：在 `wink-ai/packages/embedded-frontend/` 下运行 `bun run dev` 拉起 Vite 调试服务。
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

## 4. 分层职责

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

### 3.1 与经典嵌入式四层架构（ops 表多态）的映射

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

## 5. 虚实双模运行核心机制

### 4.1 网页端仿真模式

1. 用户生成 App 和拓扑后，平台先执行 App 静态安全检查。
2. Device Model Registry 生成 `device_tree.c/h`、SchemaForm 属性、仿真注册信息和故障模型。
3. App、BAL、DAL、PAL Wasm target 编译为 `wasm32`。
4. 前端启动 Web Worker 运行 Wasm，主线程保持 UI 渲染。
5. Asyncify 处理 `pal_delay_ms` 等阻塞延时，watchdog 防止死循环和资源失控。
6. UniSim 通过数据面五通道（Pin / PWM / Protocol Bus / Analog / Buffer）及 PAL 平台层旁路与 Wasm 交互。
7. 仿真过程写入 Golden Trace，可用于回放、故障测试和 CI 回归。

### 4.2 真机部署模式

1. 仿真和必要故障测试通过后，用户选择目标板卡。
2. 云端编译服务在隔离容器中拉起对应 toolchain。
3. 编译器链接 App、BAL、DAL、PAL target 和设备树，生成固件。
4. 返回 firmware、sha256、build manifest 和编译日志。
5. 浏览器通过 WebSerial/WebUSB 请求用户授权，执行烧录。
6. 真机运行 WinkMicroOS，并可通过 UART 输出 trace 与仿真 trace 对比。

---

## 6. 仿真精度边界

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

## 7. 安全与可信链路

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

## 8. 核心商业与技术价值

1. **降低嵌入式原型试错成本**：用户先在浏览器中验证控制逻辑和外设交互，再进入硬件阶段。
2. **让 AI 生成代码可控可审计**：静态检查、沙箱、故障注入和 trace 让 AI 代码从“能生成”升级为“可验证”。
3. **统一虚拟和真实执行路径**：App 同源运行，BAL/DAL/PAL 分层隔离，减少平台迁移成本。
4. **性能优先的浏览器仿真**：通过协议旁路与 PAL 物理源旁路绕开微观波形开销。
5. **产品闭环完整**：从需求、画布、仿真、编译、烧录到真机 trace 对比，形成端到端体验。

---

## 9. MVP 聚焦范围

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

---

## 10. 复杂智能硬件下的 AI-MCU 协同架构 (大脑与小脑模式)

为了支持复杂的智能嵌入式产品，平台将“上层 AI 决策算法”与“底层实时控制逻辑”进行物理与逻辑的双重解耦，采取分布式异构协同架构。

### 10.1 大脑与小脑的分工原则

- **上层 AI 决策层 (大脑 - Cerebrum)**：运行于高性能边缘计算芯片 (如 Linux/Cortex-A/NPU) 或云端，负责视觉识别、自然语言处理、大模型 Agent 规划、SLAM 路径构建等高算力、非确定性、高延迟的任务。
- **底层控制层 (小脑 - Cerebellum / WinkMicroOS)**：运行于实时 MCU (Cortex-M/ESP32)，负责电机驱动闭环 (PID)、实时传感器更新、本地安全策略以及看门狗。它保证微秒/毫秒级的硬实时确定性与物理安全。

### 10.2 虚实融合四层架构 (Virtual-Physical Hybrid Four-Layer Architecture)

在分布式异构协同的基础上，系统通过“虚实融合四层架构”实现应用逻辑与硬件、平台以及仿真环境的彻底解耦：

```text
+-----------------------------------------------------------------------------------+
|  1. 智能决策与 AI 算法层 (AI & Decision Layer - 大脑)                               |
|     - 运行位置：主控芯片 (Linux/Cortex-A/NPU) 或 云端                                  |
|     - 职责：视觉目标检测 (YOLO)、语音识别、路径规划 (SLAM)、LLM Agent 状态决策              |
+-----------------------------------------------------------------------------------+
                                       │
                                       │ 协议通信 (Protobuf / JSON over UART/SPI/WiFi)
                                       ▼
+-----------------------------------------------------------------------------------+
|  2. 应用业务控制层 (WinkOS App Layer - 小脑)                                        |
|     - 运行位置：实时控制 MCU (WinkOS Core)                                          |
|     - 职责：接收 AI 层的决策指令，根据本地状态机执行物理安全逻辑（如：避障降速、安全防夹） |
|     - 特点：对 AI Agent 友好，双端同源编译 (Wasm/MCU)                                |
+-----------------------------------------------------------------------------------+
                                       │
                                       ▼
+-----------------------------------------------------------------------------------+
|  3. 业务算法层 (WinkOS BAL Layer - 胶水/协处理器)                                  |
|     - 职责：协调 DAL 硬件与 Runtime 任务，如自动采样、数据滤波、状态广播、AI 桥接      |
+-----------------------------------------------------------------------------------+
                                       │
                                       ▼
+-----------------------------------------------------------------------------------+
|  4. 设备与平台抽象层 (WinkOS DAL / PAL Layer - 底盘)                                |
|     - 职责：屏蔽具体芯片和外设差异，提供标准物理 API (如：电机转速、超声波距离值)     |
+-----------------------------------------------------------------------------------+
```

1.  **AI & Decision Layer (大脑)**：
    *   独立于实时 MCU 运行。通过非阻塞协议与底层的 WinkMicroOS 进行通信。
    *   在网页仿真中，此层可以通过 Mock 的 AI 服务节点或本地 JS 模型进行仿真。
2.  **WinkOS App Layer (小脑)**：
    *   这是 WinkMicroOS 直接调度的业务层，提供 `app_init`、`app_loop`、`app_on_fault` 等生命周期回调。
    *   在真机上编译为二进制裸跑，在 Web 上通过 Wasm 沙箱运行，确保两端行为 100% 一致。
3.  **WinkOS BAL Layer**：
    *   作为 App 层与 DAL/PAL 的桥梁，封装诸如 PID 闭环控制、传感器滤波等不需要上层 AI 介入的硬实时边缘计算。
4.  **WinkOS DAL/PAL Layer**：
    *   底层的驱动和系统服务抽象。在仿真时通过 Wasm-JS Bridge 直通旁路输出给前端 3D 物理引擎；在真机时通过静态绑定与 ESP32/STM32 物理外设交互。

### 10.3 接口与边界 (BAL 的定位)

> **现行细则 SSOT**：[02-wink-micro-os/06-bal-layer.md](../02-wink-micro-os/06-bal-layer.md)（三域、命名、CI）。决策：[ADR-0037](../../decisions/core/0037-bal-domain-partition-and-closed-loop-motor.md)、[ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md)。

- **业务算法层 (BAL) 保持轻量**：BAL 仅包含与底层硬件直接相关的实时算法 (如卡尔曼滤波、滑动平均、PID 控制) 与器件增强/闭环组件，不得集成重型神经网络或复杂的宏观决策，以防止低端 MCU 的运行时过载与代码膨胀。
- **三域**：物理增强（单 DAL）· `math/`（纯算法）· `control/`（跨器件闭环/编排）。详见 06-bal-layer。
- **通信桥梁**：BAL 中可引入通信协议适配器 (如遥测 default 服务)，通过标准协议向上层 AI 广播实时状态数据，并接收上层下发的控制指令。
- **Fail-Safe 安全降级机制**：一旦上层 AI 出现内存溢出、死机或网络异常，底层 WinkMicroOS App 层状态机在检测到通信超时后，能够自主接管，执行本地安全刹车、回航或报警，实现系统级容错。闭环控制另须反馈失效脱扣（ADR-0037）。

### 10.4 意图控制与数据契约 (Intent Command & Data Contract)

在整个异构协同设计中，App 层最重要的职责是**对智能决策层提供“意图指令”的封装与隔离**，避免直接物理引脚读写的反模式（Anti-Pattern）：

1.  **意图控制指令封装 (Control Commands)**：
    *   App 层明确定义控制语义（如“设置底盘目标速度 `0.5m/s`”、“机械臂移至坐标 `(X,Y,Z)`”）。
    *   指令入参必须采用标准的物理单位（如米/秒、角度、毫米），绝对不向决策层暴露物理引脚号、PWM 占空比等底层时序细节。
    *   这为 AI Agent 提供了天然的 Function-Calling Schema，极易生成与验证。
2.  **语义遥测数据输出 (Semantic Telemetry)**：
    *   底层传感器数据经 BAL 处理（数据滤波、状态融合）后，由 App 层以语义化形式推送给决策层（如“当前电量百分比”、“前方障碍物距离”），过滤掉底层的 ADC 原始读数和引脚波动。
3.  **本地硬实时闭环与执行否决权 (Local Real-Time Override)**：
    *   WinkMicroOS App 层根据当前本地物理约束（如：底盘超声波防撞、限位开关触发），对上层决策指令拥有最终执行否决权。即使上层 AI 产生决策幻觉或发送了危险指令，App 层亦能根据安全边界本地拦截，保障物理实体安全。



