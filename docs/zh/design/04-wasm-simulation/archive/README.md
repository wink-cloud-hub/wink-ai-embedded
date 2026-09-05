# WebAssembly 仿真与前端运行时引擎 (UniSim)

> **⚠️ 已归档（Archived，2026-08-02）**：本目录为 UniSim **1.0** 历史版本，已被 [**3.0（现行 Active SSOT）**](../00-README.md) 取代（中间版本 2.0 已归档后删除）。以下内容仅作只读历史对照，不再演进；现行设计请读 3.0。

本目录包含 **Wink-AI** 前端 Wasm 仿真沙箱与浏览器虚拟外设运行库（UniSim 引擎）的架构与设计规范。

仿真层允许编译为 `wasm32` 字节码的 C 语言业务代码在 Web Worker 沙箱中运行，并与前端 Vue 3 / 虚拟外设 UI（及规划中的 ProductWorld 3D）联合，实现设备行为的**行为级高保真**模拟。

> **阅读指引**：`03` 只回答「外设物理量怎么进固件」；整机保真（时间、中断、调度、可测场景）以 `05` + `08` 为准。勿用四通道路由替代整套仿真架构。

---

## 仿真多轴总览（正交维度）

仿真能力应按下表分轴理解与验收。各轴独立演进；宣称「高一致」时须指明覆盖了哪些轴。

| 轴 | 回答的问题 | 主要机制 | 主文档 | 典型上限 |
|---|---|---|---|---|
| **A. 外设物理源** | 传感器/执行器/总线数据从哪来 | 四通道 + PWM 子类：Pin / Bus / Analog / Buffer；PinArbiter、Plugin | [03](./03-multi-channel-sim-routing.md)、[02](./02-virtual-peripheral-registry.md) | 不仿真电气前端；部分通道 Planned |
| **B. 时间基** | delay / 超时 / 脉宽以谁为钟 | `s_virtual_us` SSOT；禁止 `pal_delay` 双重步进 | [05](./05-simulation-consistency-and-fidelity-spec.md) C2/C14、[06](./06-physical-degradation-engine.md) | 非墙钟实时；快进改变可观测墙钟时长 |
| **C. 定时器硬件语义** | HW timer / PWM 周期 / capture | PAL timer、软步进近似；资源独占门禁 | [05](./05-simulation-consistency-and-fidelity-spec.md) C10/C17、[ADR-0047](../../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md) | 无 10kHz+ 硬 ISR；FOC 快环行为级 |
| **D. 中断模型** | ISR 何时跑、能否抢占嵌套 | Asyncify 协作插入、IRQ 队列 poll（非真抢占） | [01](./01-wasm-sandbox-lifecycle.md)、[05](./05-simulation-consistency-and-fidelity-spec.md) C4/C15/C20 | **不可**验优先级嵌套/硬实时抢占 |
| **E. 调度与并发** | 多任务、阻塞、临界区、多核 | 协作式单虚拟核调度器 | [07](./07-scheduler-model.md)、[05](./05-simulation-consistency-and-fidelity-spec.md) C3/C5/C9/C16 | SMP / 真抢占 → 真机 |
| **F. 故障与观测** | OOM、WDT、竞态、Trace | Fault 策略、lint/门禁、场景清单 | [05](./05-simulation-consistency-and-fidelity-spec.md)、[08](./08-simulation-consistency-checklist.md)、[ADR-0045](../../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md) | 清单中 🚫 项必须 HIL |

```text
固件 C (App/BAL/DAL 同源目标)
        │
        ├─ A 外设物理源 ←── 03 四通道 / UniSim Plugin
        ├─ B/C 时间与定时器 ←── VirtualClock / PAL timer
        ├─ D 中断/回调序 ←── Asyncify + IRQ 队列
        ├─ E 调度与共享状态 ←── 07 协作调度
        └─ F 故障与门禁 ←── 05/08 + lint
                ↓
         真机 / HIL（电气、硬实时、多核…）
```

**交叉示例**：超声波「通道 1 高一致」= **A**（ECHO 沿）+ **B**（VirtualClock 脉宽）+ 建议 **`timing`** Accuracy Mode；仅 Hub 注入 `distanceCm` 捷径不足以宣称沿捕获一致。

场景「现在能不能验」→ 查 **[08](./08-simulation-consistency-checklist.md)**；机制与契约 → 查 **[05](./05-simulation-consistency-and-fidelity-spec.md)**。

### A～F 完备后的产品口径（会否与真机不一致）

落实轴 A～F **不等于**「仿真 ≡ 真机」或「可免真机发版」。正式口径如下（原理展开见 [05 §0.4](./05-simulation-consistency-and-fidelity-spec.md#04-af-完备后的生产口径与残余不一致)）：

| 「生产水准」含义 | A～F 全落地后 |
|---|---|
| 仿真作为 CI / 低代码主路径的**高置信行为级预检** | **可以接近**（漏检可分类、假绿可解释、🚫 强制真机） |
| 仿真结果可**替代**真机 / HIL 作为放行依据 | **否** |
| 仿真与真机 **bit / μs 级恒等** | **永不承诺** |

**仍必然存在的不一致**（模型上限，非「没做完」）：电气/模拟前端、硬实时与抢占中断嵌套、芯片级 HW timer / FOC 硬 ISR、多核 SMP、微架构与 silicon 细节、宿主 Asyncify/Worker 墙钟观感等。

**会显著减少的不一致**：DAL/App 同源路径上的换算/超时/协议打包、虚拟时钟下的逻辑时序、资源冲突与非法阻塞门禁、协作调度下的饿死/软 WDT 等——从「随机翻车」变为「[08](./08-simulation-consistency-checklist.md) 已知缺口 + 真机必跑项」。

```text
A～F 完备
  → 生产级「行为/协议/资源预检流水线」✅
  → 生产级「虚实结果恒等 / 免真机放行」❌

残余不一致 → 用 08 清单 + HIL/真机门禁 + ADR-0003 口径管理，而不是消灭
```

---

## 模块设计文档

*   **[01-wasm-sandbox-lifecycle.md](./01-wasm-sandbox-lifecycle.md)** — Wasm 沙箱生命周期与 Asyncify（**轴 D/E 入口**）
    *   Web Worker 隔离、Asyncify 挂起/恢复、Wasm Table 与 GPIO ISR 协作插入。
*   **[02-virtual-peripheral-registry.md](./02-virtual-peripheral-registry.md)** — 虚拟电路 / DeviceTree / SchemaForm（**轴 A 配置面**）
    *   电路拓扑、外设属性表单、连线与实例注册。
*   **[03-multi-channel-sim-routing.md](./03-multi-channel-sim-routing.md)** — 四通道 PAL 外设路由与选型（**仅轴 A**）
    *   Pin / Bus / PWM Duty / Analog / Buffer；分层同源；Plugin Channel 红线；**不是**整机仿真总纲。
*   **[04-velxio-migration-analysis.md](./04-velxio-migration-analysis.md)** — Velxio 对比与迁移分析
    *   可继承点（Wokwi Elements、导线、TS 器件模型）与放弃点（指令级模拟器）。
*   **[05-simulation-consistency-and-fidelity-spec.md](./05-simulation-consistency-and-fidelity-spec.md)** — 一致性与保真原理 SSOT（**轴 B～F + 场景契约**）
    *   虚拟时钟 / Co-Sim；C1～C25 问题·方案·预言·边界；Phase 里程碑。
*   **[08-simulation-consistency-checklist.md](./08-simulation-consistency-checklist.md)** — 场景可测性索引 SSOT
    *   「这个场景现在能不能验」；✅/🟡/❌/🚫；链回 05。
*   **[06-physical-degradation-engine.md](./06-physical-degradation-engine.md)** — 物理退化与故障注入（ADR-0009）
    *   虚拟时钟架构、退化引擎、故障分层、Worker 协议。
*   **[07-scheduler-model.md](./07-scheduler-model.md)** — 协作式调度器模型（ADR-0013/0014）（**轴 E**）
    *   任务状态机、pick_next、host vs wasm、WCET 墙钟兜底。

---

## 已知仿真限制（行为级边界）

Wink-AI 仿真为**行为级（causal）高保真**：保证业务逻辑的因果顺序与逻辑正确性，**不保证** cycle/tick 级时序保真与电气保真。详见 [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)。

- ✅ **可**验证（在对应轴落地且场景为 ✅/🟡 时）：业务状态机、经 PAL 同源路径的传感器/执行器语义、I2C/UART **payload** 级协议、超时/断线等异常处理、虚拟时钟下的逻辑时序子集。
- ❌ **不可**（或仅弱近似）验证：硬实时微秒响应、**中断抢占与优先级嵌套**、芯片级 HW timer/FOC 硬 ISR、多核 SMP 真并发、模拟电路（ADC 量化/阻抗/电源）、指令/微架构级行为。
- ⚠️ **外设轴注意**：旁路必须落在 PAL（物理量来源替换）；**禁止** DAL 业务直通 `#ifdef`。超声波等若仍走 C 侧 `distanceCm`→μs 捷径，不得宣称通道 1 沿捕获已对齐——见 [03 §5.1](./03-multi-channel-sim-routing.md)。

时序/并发/电气级交付以真机或 HIL 为准。演进方案见 [05](./05-simulation-consistency-and-fidelity-spec.md)；物理与故障注入见 [ADR-0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md)。

---

## 仿真核心层关系图

```text
 ┌────────────────────────────────────────────────────────┐
 │     Vue 3 主线程 (画布 / ControlHub / ProductWorld)    │  轴 A 注入与观测
 └───────────────────────────▲────────────────────────────┘
                             │ postMessage
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │             Web Worker (SimWorker + Plugins)           │
 │  PinArbiter / I2C·SPI·UART / VirtualClock / PluginHost │  轴 A/B
 │  ┌───────────────────────┐    ┌─────────────────────┐  │
 │  │    Wasm-Core (C OS)   ├───►│   Wasm JS Bridge    │  │
 │  │ App/BAL/DAL + PAL API │    │ Asyncify · js_pal_* │  │  轴 D/E
 │  │ OSAL 协作调度         │    │ IRQ 队列 poll       │  │
 │  └───────────────────────┘    └─────────────────────┘  │
 └────────────────────────────────────────────────────────┘
```

---

## STRICT_NONBLOCKING 与 Bringup 隔离（ADR-0025）

为在仿真阶段尽早暴露非法阻塞并防范真机 WDT 饿死：

1. **仿真默认严格**：CMake 开启 `-DWINK_STRICT_NONBLOCKING=1`；`WINK_BLOCKING` API（如阻塞式 `dal_ultrasonic_read`）在头文件中隐藏，误用 → 链接期 fail-fast。
2. **Bringup / Selftest 隔离**：阻塞辅助工具置于 `runtime/selftest/`，用 `#ifndef WINK_STRICT_NONBLOCKING` 包裹；严格模式仅留 stub，防止阻塞代码进入仿真沙箱。

