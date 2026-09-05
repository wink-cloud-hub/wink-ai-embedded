# ADR-0007：协作式执行模型的适用边界、跨核隔离与多系统适配方案

| 项 | 内容 |
|---|---|
| 状态 | **Accepted（已采纳）** |
| 日期 | 2026-06-27 |
| 触发 | 用户追问：`wink_runtime_run` 硬编码执行顺序是否会成为硬件创意瓶颈；多核芯片及裸机与不同操作系统的适配与运行逻辑 |
| 影响范围 | runtime / DAL / BAL·App / OSAL / AI Codegen / Wasm 仿真 / 跨平台编译链 |
| 决策者 | 项目负责人与核心架构师团队 |

---

## 背景（Context）

### 1. 现状执行骨架
现有的 runtime 主循环 `wink_runtime_run(callbacks, max_ticks)`（位于 [wink_runtime.c](../../../wink-micro-os/runtime/src/wink_runtime.c#L19-L66)）将执行骨架定义为：
1. **Boot safe-lock**：检测复位原因是否为 WDT/Panic，并安全关断全部执行器。
2. **`callbacks->init()`**：执行一次性初始化。
3. **`while` 周期 Tick 循环**：每 Tick 执行一次 `callbacks->loop()`，监测 WCET（执行耗时是否超过 Tick 周期的一半，即 5ms），随后执行 Tick 周期延迟 `wink_app_delay_ms(10ms)`。
4. **故障处理**：若触发故障，执行 `wink_runtime_fault` 关断所有执行器并调用故障回调。

### 2. 协作式特征与设计张力
该模型采用单线程、顺序、固定 Tick（10ms）、非抢占的**协作式事件循环（Cooperative Event Loop）**。它是 Arduino `setup()/loop()` 范式的安全性进化版。

这种“单一协作循环”与“未来用户硬件创意的多样性”之间存在着设计张力。用户担忧单一 `loop` 难以优雅表达并发、多速率、长 I/O 阻塞等复杂业务。此外，面对**多核处理器（如 ESP32）**、**裸机裸跑（Bare-metal）**、**不同嵌入式操作系统（如 FreeRTOS/RT-Thread）** 以及 **WebAssembly (Wasm) 浏览器仿真**这四种完全不同的底层运行环境，整个系统的任务调度、时序一致性及跨平台适配的架构决策需要进行系统性的融合与正式化。

---

## 方案比选与架构权衡（Options & Trade-offs）

### 方案 A：纯协作式单循环（现状，Arduino 范式）
*   **优点**：高度确定性（利于同源仿真对齐）、AI 代码生成极友好（避免多线程锁错误）、零内存同步开销。
*   **缺点**：当出现多速率任务（例如 100Hz 闭环控制与 1Hz 刷屏并发）时需要手写分频计数器，代码繁琐；不解决长 I/O 阻塞与高速硬实时场景。

### 方案 B：原生抢占式 RTOS 多任务（每业务一个 OS 任务）
*   **优点**：支持抢占、阻塞操作无碍、多速率表达自然。
*   **缺点（对本项目致命）**：
    1.  **破坏仿真一致性**：抢占调度的非确定性使得 Web 仿真器与真机的 Golden Trace 无法对齐。
    2.  **AI Codegen 灾难**：大语言模型（LLM）编写互斥锁、信号量等同步原语时极易产生死锁与竞争幻觉。
    3.  **Wasm 仿真缺位**：标准 Wasm 单线程栈难以模拟抢占式多任务内核。

### 方案 C：混合增强模型（采纳方案：底座定时引擎 + 表层协作式无栈协程）
*   在底层维护一个**无栈协作式软定时/事件调度器（类似 TaskScheduler 机制）**，在上层为应用代码提供**纯 C 无栈协作式协程（类似 Dunkels Protothreads 机制）**的宏语法封装。
*   **优点**：
    *   **AI 线性生成友好**：应用代码看起来是线性的顺序结构（支持等待和延迟），LLM 生成错漏率降至最低。
    *   **执行高效且低功耗**：当协程等待状态或延时（如 `WINK_PT_DELAY_MS(100)`）时，底层调度器将该任务标记为非活跃，主循环直接跳过它（消除忙轮询），空闲时允许 MCU 进入物理休眠。
    *   **完美的跨平台一致性**：无任何汇编切栈，在 Wasm 仿真器和物理 MCU 上均是纯粹的 C `switch-case` 跳转，代码完全同源。

---

## 决策结论（Decision）

采纳 **方案 C（底座软定时器调度器 + 表层无栈协作协程）** 作为主执行模型，并基于此设计**多核隔离拓扑**与**多系统适配层（OSAL）**。

```
                    Wink-AI 跨平台三层执行架构
+-------------------------------------------------------------+
|    1. App 业务层应用（AI 生成）： 线性顺序协作式协程 (WINK_PT_*)   |
+-------------------------------------------------------------+
                               | (只调用统一 API 契约)
                               v
+-------------------------------------------------------------+
|    2. 平台抽象层（OSAL / Runtime）： 软定时/事件调度与任务抽象  |
+-------------------------------------------------------------+
                               | (根据编译目标自适应适配)
                               v
+-------------------------------------------------------------+
|    3. 底层执行引擎层：适应裸机、不同 OS 及 Web 仿真            |
|                                                             |
|  [ESP32 / 高性能板]      | [Arduino Uno / 裸机] | [Wasm 浏览器]      |
|   - 依托 FreeRTOS 调度    | - 硬件定时器与忙等    | - JS 事件循环      |
|   - Core 0/1 物理隔离     | - 降级不支持多并发    | - Asyncify 栈挂起  |
+-------------------------------------------------------------+
```

### 1. 多核架构设计：非对称物理隔离
针对多核处理器（如 ESP32 双核），**严禁将应用协程/定时器自动分发到多核上并行运行**，避免引入多线程锁竞态。我们采取**系统级非对称物理隔离**策略：

*   **Core 0（系统通信核）**：固化运行 Wi-Fi 协议栈、蓝牙协议栈、TCP/IP (lwIP) 协议栈以及系统后台。将频繁的网络数据包处理中断隔离在 Core 0，防止其抖动干扰控制算法。
*   **Core 1（应用控制核）**：独占运行单线程的 `wink_runtime_run` 协作式主循环（包含所有业务协程）。这保证了控制回路具有极高的周期确定性与超低抖动。
*   **跨核逃生舱通信**：若确实需要在 Core 0 上运行重型背景计算（如 AI 视觉推理），必须通过 OSAL 创建特定的后台任务，并且**仅允许使用 OSAL 提供的非阻塞、线程安全环形缓冲区（RingBuffer/Queue）**进行数据交换。Core 1 仅在 Tick 边界无锁轮询该队列，禁止任何跨核阻塞同步。

### 2. 裸机与多嵌入式系统适配设计（OSAL）
通过 OSAL（操作系统抽象层），Wink-AI 隔离了具体开发板是否运行操作系统的技术差异：

*   **裸机（Bare-metal）适配**：在不支持操作系统的低端 MCU（如 AVR/Arduino Uno）上，OSAL 的延时直接映射到硬件定时器和忙等；`pal_task_create` 显式创建后台任务接口直接返回 `WINK_ERR_NOT_SUPPORTED`。应用层依托单线程协作主循环完美运行，实现**优雅降级**。
*   **主流 RTOS（FreeRTOS / RT-Thread / Zephyr 等）适配**：在运行 RTOS 的中高端开发板上，Wink-AI 运行时整体作为**一个高优先级的 OS 线程**挂载运行。OSAL 接口直接映射到原生 OS API（如 `vTaskDelay` 或 `rt_thread_delay`），在延迟期间主动释放 CPU，让底层 OS 调度其他系统服务。
*   **Web 仿真适配**：在浏览器 Wasm 目标下，OSAL 桥接到 JS 的定时器和 Asyncify 技术，实现不卡死浏览器事件循环的虚拟时序调度。

---

## 后果与约束（Consequences & Constraints）

### 硬约束（Conformance）
1.  **安全锁（Boot Safe-Lock）物理强截断**：一旦检测到上次重启积压原因为 WDT/Panic，系统必须强行截断，直接置位安全标志进入 `wink_runtime_fault`，**严禁执行用户的 `init()` 与 `loop()`**，防止异常状态被用户初始化覆盖引发无限重启环。
2.  **多并发禁止直调原生 OS API**：App 业务层严禁直接调用 `xTaskCreate` 等原生 OS 接口。所有后台并发任务必须通过 OSAL `pal_task_create` 建立，以保障代码在 Wasm 仿真平台中能由 Asyncify 微任务引擎等效模拟。
3.  **WCET 细粒度监控**：WCET 耗时检测必须由针对全局 `loop()` 的宏观统计，重构为**基于单个协程/任务回调的微观耗时监控**。防止软定时器层引入后，多个多速率任务在同一 Tick 槽重合执行造成 8002 虚警。
4.  **Tick 周期配置 SSOT**：`WINK_RUNTIME_TICK_MS` 必须由应用全局描述文件 `wink_app.json` 统一定义，并自动生成两端一致的头文件，绝不允许编译时真机与仿真侧参数分叉。

### 适用边界（Applicability Boundary）

| 业务场景 | 协作式够吗 | 处置策略 | 跨核与系统依赖 |
|---|---|---|---|
| **状态机逻辑控制** | ✅ 够 | 标准 `loop` 状态转移 | 单核/裸机/OS皆可 |
| **多速率轮询/定时** | ✅ 够 | 软定时器与协作式无栈协程 | 单核/裸机/OS皆可，零 CPU 忙轮询 |
| **慢物理量转换（如 DS18B20 转换）** | ✅ 够 | 驱动层必须非阻塞三段式（Request/Poll/Cached） | 单核/裸机/OS皆可 |
| **秒级网络异步 I/O (MQTT/WiFi)** | ⚠️ 吃力 | OSAL 后台任务逃生舱 | 必须在多核 OS (Core 0) 或仿真 JS 侧处理 |
| **高速电机闭环控制 (FOC / Fast PID)** | ⚠️ 不够 | 调小 `wink_app.json` 周期或任务硬实时化 | 需在支持 RTOS 高优线程上处理 |

### 真实风险点（Risk Register）
1.  **AI 编写无栈协程的语法规范限制**：Protothreads 机制要求协程内跨挂起点的局部变量必须声明为 `static`。策略：通过静态代码检查器及 Prompt 规则强制 AI 在协程函数中限制普通局部变量的使用。
2.  **跨核数据同步死锁**：若第三方库在后台 Core 0 占用了某些硬件总线（如 I2C），而 Core 1 的主循环同时轮询，可能会导致硬件锁死。必须在 DAL 驱动层做静态总线独占限制。

---

## 遵循与后续（Compliance & Follow-up）

1.  **Backlog（待办事项）**：
    *   [x] 软定时器调度器 API 接口（`wink_soft_timer.h/.c`：ONESHOT/PERIODIC、静态分配、per-callback WCET、Tick 对齐）——2026-06-28 ESP32 真机闭环验证通过。
    *   [ ] `WINK_PT_*` 无栈协程宏（Protothreads 语法封装）——footgun 检查器 `wink-micro-os/tools/lint/check_pt_variables.py` 已就位，宏本身仍待实现 -> `docs/tech-designs/`（**唯一剩余 follow-up**）
    *   [x] `wink_runtime.c` 的 WDT/Panic 安全锁，切断后续 `init` 与 `loop` 执行流程（Boot safe-lock，见 [04-runtime-and-trace.md](../../zh/design/02-wink-micro-os/04-runtime-and-trace.md) §3.4）。
    *   [x] WCET（8002）监控细粒度化：软定时器回调已实现 per-callback 独立耗时计时（`wink_soft_timer.c`）。
    *   [x] 从 `wink_app.json` 自动解析并生成 `WINK_RUNTIME_TICK_MS` 编译头文件（`wink-micro-os/tools/codegen/config_h.py`，host/wasm 与 ESP32 IDF 组件均已接入）。
    *   [x] OSAL 接口定义文件 `pal_osal.h`（含 `pal_task_create` 钉核、`pal_ringbuf_*` 跨核逃生舱）。

---

*变更记录：*
- 2026-06-27：Proposed（用户追问协作式硬编码执行顺序是否成瓶颈，触发边界正式化）
- 2026-06-27：Amended（架构评审优化：融入“底层 Task-driven 定时器 + 表层无栈协程”混合架构，确立双核物理隔离与 OSAL 裸机/多操作系统适配策略，补全安全锁截断、OSAL并发、WCET细化与配置SSOT规范）
- 2026-06-28：Accepted（软定时器调度器 + 双核物理隔离钉核 + 跨核 ringbuf 逃生舱在 ESP32 DevKitC（ESP-IDF v6.0.1）真机闭环验证通过：soft timer 经 `wink_runtime_run` tick 循环调度、Core 1 控制环物理隔离、`pal_ringbuf` 跨核通信均确认工作。评审记录见 [2026-06-28-cooperative-loop-esp32-hardware-verification.md](../../reviews/core/2026-06-28-cooperative-loop-esp32-hardware-verification.md)。决策已回写至 [04-runtime-and-trace.md](../../zh/design/02-wink-micro-os/04-runtime-and-trace.md)。`WINK_PT_*` 协程宏仍为 follow-up。）

