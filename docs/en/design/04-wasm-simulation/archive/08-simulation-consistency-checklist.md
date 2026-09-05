# 4.8 Simulation Consistency Checklist

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/archive/08-simulation-consistency-checklist.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Creation Date | 2026-07-31 |
| Document Tier | ① Design Specification (`04-wasm-simulation/`) |
| Status | **Active** |
| Related Docs | [05-simulation-consistency-and-fidelity-spec.md](./05-simulation-consistency-and-fidelity-spec.md), [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md), [ADR-0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md), [ADR-0013](../../../decisions/unisim/0013-sim-cooperative-scheduler.md), [ADR-0014](../../../decisions/unisim/0014-sim-single-virtual-core.md), [ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md), [ADR-0042](../../../decisions/unisim/0042-sim-execution-modes.md), [ADR-0045](../../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md), [07-scheduler-model.md](./07-scheduler-model.md) |

> **Positioning**: This checklist serves as the **scenario verification index** for [05 Simulation Consistency & Fidelity Specification](./05-simulation-consistency-and-fidelity-spec.md)—answering "can this be tested right now / what are the gaps" across categories and sub-scenarios.
>
> - **To check if a scenario is testable** $\rightarrow$ Read this checklist.
> - **For problem definitions, solutions, acceptance oracles, and boundaries** $\rightarrow$ Consult [05](./05-simulation-consistency-and-fidelity-spec.md) (**Principles & Sub-Scenario Contracts SSOT**).
> - **For product boundaries** $\rightarrow$ [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md).
>
> "Planned Solutions" contain only a **one-line summary + link**; duplicating technical narratives from 05 is prohibited.

---

## 0. Reading Conventions & SSOT Division of Responsibilities

### 0.1 Support Level

| Marker | Meaning |
|---|---|
| ✅ **Supported** | Core correctness verifiable; known limitations documented in "Residual Gaps" |
| 🟡 **Partially Supported** | Subset covered or approximated only; physical hardware escapes remain possible |
| ❌ **Unsupported** | Currently untestable; depends on future Phases or physical hardware testing |
| 🚫 **Explicit Non-Goal** | Outside product boundaries; approximate or unmodeled |
| — **N/A** | API or execution path not exposed in current product releases |

### 0.2 Solution Types

| Type | Meaning |
|---|---|
| **A** | Constraints (linter rules / compile-time flags / JSON gates) |
| **B** | Engine Modeling (virtual clocks, delays, homologous drivers, chaotic scheduling, etc.) |
| **C** | Observation Gates (Sanitizers, soft WDT, shadow memory, overflow Faults) |
| **Hardware** | HIL / Physical hardware test sign-off |

### 0.3 Document Pointers

* Sub-scenario 5-field templates, C1–C25 full specifications $\rightarrow$ [05 §0.1 / §2](./05-simulation-consistency-and-fidelity-spec.md#01-场景文档模板每个子场景必须写齐)
* Principles (Virtual Clock / Co-Sim / Zero-Yield) $\rightarrow$ [05 §1](./05-simulation-consistency-and-fidelity-spec.md#1-仿真一致性底层原理概述)
* Phase Milestones $\rightarrow$ [05 §3.2](./05-simulation-consistency-and-fidelity-spec.md#32-演进阶段里程碑)

---

## 1. Category Overview Matrix (C1～C25)

| ID | Consistency Category | Status | Primary Solution | Phase / Scope | Priority |
|---|---|---|---|---|---|
| C1 | Business Causality / State Machines | ✅ | B (+A) | Baseline | High |
| C2 | Virtual Microsecond Logic Timing | ✅ | B | Phase 1 | High |
| C3 | Shared State Race Conditions | ❌ | B+C | Phase 4 | **Highest** |
| C4 | Critical Sections & Interrupt Preemption | 🟡 | B+C | Phase 4 | **Highest** |
| C5 | Blocking / Starvation / WDT | 🟡 | A+B+C | Phase 4 Precursor | High |
| C6 | Stack / Heap / Memory Safety | ✅ | A+C | Phase 2 | High |
| C7 | Bus Protocols / CRC | 🟡 | B+A | Phase 3 | Medium-High |
| C8 | DMA / Asynchronous Windows | ❌ | B | Phase 3 | Medium (Drivers: High) |
| C9 | Multi-Core SMP | ❌ | B Approx. / Hardware | ADR-0014 | High |
| C10 | Fast-Loop ISR / FOC | 🟡 | B Approx. / HIL | ADR-0047 | Medium-High |
| C11 | Electrical / Analog | 🚫 | B Tabular Approx. | ADR-0003 | Product-Dependent |
| C12 | CPU / ABI Instruction Level | ❌ | C Dual-Track | Phase 5 | Low (Hard to debug) |
| C13 | Lifecycle / Reset | 🟡 | B+C | Phase 1 Enhancement | **Highest** |
| C14 | Fast-Forward / Co-Sim Stepping | 🟡 | B+C | Phase 1+ | **Highest** |
| C15 | Host↔Wasm Boundary | 🟡 | A+C | Baseline | **Highest** |
| C16 | OS Synchronization Primitive Semantics | 🟡 | B+A | Phase 4 Precursor | **Highest** |
| C17 | Peripheral Resource Conflicts / Clocks | 🟡 | A+C | Continuous | High |
| C18 | Bus Fault State Machines | ❌ | B | Phase 3 Extension | Medium-High |
| C19 | DMA / Buffer Lifecycles | ❌ | B+C | Phase 3 Extension | Medium (Drivers: High) |
| C20 | Callback Reentrancy / Bottom-Halves | 🟡 | A+C | Phase 4 | High |
| C21 | Time & Counter Wrap-Around | 🟡 | A+C | Continuous | High |
| C22 | Power / Low-Power / Clock Domains | 🚫 | Hardware | Mostly Non-Goal | Medium |
| C23 | Persistence / NVS | 🟡 | B | On-Demand | Medium |
| C24 | Caches / DMA RAM | 🚫 | Hardware / C12 | Mostly Non-Goal | Medium |
| C25 | Floating-Point / Numerics UB | 🟡 | C | Phase 2 | Medium |

---

## 2. Key Sub-Scenario Lookup (Status · Gaps · Links to 05)

> Full sub-scenario specifications reside in 05; this section enumerates **high-escape / high-priority** sub-items to avoid checklist bloat. Sub-items not listed inherit category-level status.

### C1 — Business Causality
| Sub-Item | Status | Residual Gap (Summary) | Link to 05 |
|---|---|---|---|
| C1.1 Homologous State Transitions | ✅ | — | [C1.1](./05-simulation-consistency-and-fidelity-spec.md#c11-同源-appbal-状态迁移) |
| C1.2 Bypass Narrowing | 🟡 | Residual full-layer `#ifdef` | [C1.2](./05-simulation-consistency-and-fidelity-spec.md#c12-dal-bypass--ifdef-simulation-收窄) |
| C1.3 Fault / Timeout Paths | ✅ | Expanding with peripherals | [C1.3](./05-simulation-consistency-and-fidelity-spec.md#c13-故障--超时--断线异常路径) |
| C1.4 Retry Storms | 🟡 | Lacks unified command count assertions | [C1.4](./05-simulation-consistency-and-fidelity-spec.md#c14-幂等恢复与重试风暴) |

### C2 — Virtual Timing
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C2.1 sleep Fast-Forward | ✅ | Crystal drift not modeled by default | [C2.1](./05-simulation-consistency-and-fidelity-spec.md#c21-sleep--定时唤醒快进) |
| C2.2 Pulse Width Zero-Yield | ✅ | Overlaps C14.2 | [C2.2](./05-simulation-consistency-and-fidelity-spec.md#c22-脉宽测量零-yield-环回) |
| C2.3 Debounce / RC | ✅ | Noise amplitude is injection parameter | [C2.3](./05-simulation-consistency-and-fidelity-spec.md#c23-去抖--rc-低通锚定虚拟时钟) |
| C2.4 Overly Ideal Periodic Tasks | 🟡 | Controlled jitter not enabled by default | [C2.4](./05-simulation-consistency-and-fidelity-spec.md#c24-单中断友好采样周期) |

### C3 — Race Conditions
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C3.1 Task↔Task Lockless | ❌ | Awaiting chaotic scheduler + TSan | [C3.1](./05-simulation-consistency-and-fidelity-spec.md#c31-无锁共享读写tasktask) |
| C3.2 Task↔ISR | ❌ | Awaiting multi-point ISR injection | [C3.2](./05-simulation-consistency-and-fidelity-spec.md#c32-taskisr-无锁交叉) |
| C3.3 Struct Tearing | ❌ | Awaiting shadow memory per-field tracking | [C3.3](./05-simulation-consistency-and-fidelity-spec.md#c33-多字段结构体撕裂) |
| C3.4 Publish Ordering / Flags | ❌ | Weak memory models not on daily track | [C3.4](./05-simulation-consistency-and-fidelity-spec.md#c34-发布-订阅顺序假设) |

### C4 — Critical Sections / Interrupts
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C4.1 Critical Section Gating | 🟡 | Deepen state assertions | [C4.1](./05-simulation-consistency-and-fidelity-spec.md#c41-临界区门禁enterexit) |
| C4.2 Polled Delivery | 🟡 | Not arbitrary instruction preemption | [C4.2](./05-simulation-consistency-and-fidelity-spec.md#c42-调度点-isr-投递poll-模型) |
| C4.3 Priority Nesting | ❌ | Phase 4+ / Hardware | [C4.3](./05-simulation-consistency-and-fidelity-spec.md#c43-优先级嵌套) |
| C4.4 FromISR Misuse | ❌ | Awaiting context gating | [C4.4](./05-simulation-consistency-and-fidelity-spec.md#c44-fromisr--非-isr-safe-api-误用) |
| C4.5 IRQ Queue Overflow | 🟡 | Requires Fail-Loud hardening | [C4.5](./05-simulation-consistency-and-fidelity-spec.md#c45-pending-中断队列溢出) |

### C5 — Blocking / WDT
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C5.1 STRICT_NONBLOCKING | ✅ | Indirect calls see C5.4 | [C5.1](./05-simulation-consistency-and-fidelity-spec.md#c51-strict_nonblocking-编译期隐藏阻塞-api) |
| C5.2 Soft WDT | ❌ | Pending implementation | [C5.2](./05-simulation-consistency-and-fidelity-spec.md#c52-软-wdt虚拟时间未喂狗) |
| C5.3 Starvation Accounting | ❌ | Pending implementation | [C5.3](./05-simulation-consistency-and-fidelity-spec.md#c53-就绪任务饿死) |
| C5.4 Indirect Blocking | 🟡 | Runtime probes incomplete | [C5.4](./05-simulation-consistency-and-fidelity-spec.md#c54-动态间接阻塞) |
| C5.5 Priority Inversion | ❌ | Inheritance not promised | [C5.5](./05-simulation-consistency-and-fidelity-spec.md#c55-优先级反转) |

### C6 — Memory Safety
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C6.1 Heap Quota | ✅ | Fragmentation geometry $\neq$ Hardware | [C6.1](./05-simulation-consistency-and-fidelity-spec.md#c61-静态堆配额耗尽) |
| C6.2 Fragmentation | 🟡 | Stress mode optional | [C6.2](./05-simulation-consistency-and-fidelity-spec.md#c62-堆碎片化) |
| C6.3 ASan / UBSan | ✅ | Not all Wasm daily builds run ASan | [C6.3](./05-simulation-consistency-and-fidelity-spec.md#c63-asan--ubsanuaf越界未对齐溢出) |
| C6.4 NO-MALLOC-APP | ✅ | — | [C6.4](./05-simulation-consistency-and-fidelity-spec.md#c64-app-禁裸-malloc) |
| C6.5 Per-Task Stack | 🟡 | Diverges from FreeRTOS multi-stack layout | [C6.5](./05-simulation-consistency-and-fidelity-spec.md#c65-per-task-栈溢出) |

### C7 — Protocols
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C7.1 Homologous CRC | 🟡 | Residual bypasses | [C7.1](./05-simulation-consistency-and-fidelity-spec.md#c71-同源协议帧与-crc) |
| C7.2 ACK / Retries | 🟡 | — | [C7.2](./05-simulation-consistency-and-fidelity-spec.md#c72-ack-超时与重试) |
| C7.3 JSON Gating | ✅ | — | [C7.3](./05-simulation-consistency-and-fidelity-spec.md#c73-json-语义仿真门禁) |
| C7.4 Bypass Audits | 🟡 | Ongoing zero-bypass drive | [C7.4](./05-simulation-consistency-and-fidelity-spec.md#c74-残留-bypass-清零审计) |

### C8 — DMA Windows
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C8.1 Transfer Yields | ❌ | Residual synchronous APIs | [C8.1](./05-simulation-consistency-and-fidelity-spec.md#c81-粗粒度传输耗时挂起) |
| C8.2 Completion IRQ Order | ❌ | Depends on C8.1 | [C8.2](./05-simulation-consistency-and-fidelity-spec.md#c82-完成中断与任务唤醒序) |
| C8.3 Sync Remnants | 🟡 | Requires migration tags | [C8.3](./05-simulation-consistency-and-fidelity-spec.md#c83-同步-api-残留) |

### C9～C12 (Clear Boundaries)
| Sub-Item | Status | Notes | Link to 05 |
|---|---|---|---|
| C9.1 Unicore Boundary | 🚫/❌ | ADR-0014 | [C9](./05-simulation-consistency-and-fidelity-spec.md#c9--多核-smp-真实并发) |
| C9.2 Chaotic Approximation | ❌ | Phase 4 | Same as above |
| C10.1 Soft-Stepping | 🟡 | Not hard real-time | [C10](./05-simulation-consistency-and-fidelity-spec.md#c10--快环-isrfoc--硬定时器) |
| C10.2 PWM-ADC Sync | 🚫 | HIL exclusive | Same as above |
| C11.* Electrical | 🚫 | Except tabular approximations | [C11](./05-simulation-consistency-and-fidelity-spec.md#c11--电气--模拟电路特性) |
| C12.1 Fast-Track | ✅ Functional | Not instruction-level | [C12](./05-simulation-consistency-and-fidelity-spec.md#c12--cpu--abi-指令级一致性) |
| C12.2 Nightly Dual-Track | ❌ | Phase 5 | Same as above |

### C13 — Lifecycles (P0)
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C13.1 Cold Boot Reset List | 🟡 | List must be locked and tested | [C13.1](./05-simulation-consistency-and-fidelity-spec.md#c131-冷启动bss--静态初值--外设默认电平) |
| C13.2 Reset Reason Injection | ❌ | Model pending | [C13.2](./05-simulation-consistency-and-fidelity-spec.md#c132-热重启--软复位原因) |
| C13.3 Re-initialization | 🟡 | Idempotency contract standardization | [C13.3](./05-simulation-consistency-and-fidelity-spec.md#c133-外设-deinit--再-init) |
| C13.4 Zombie / UAF | 🟡 | Relies on sanitizers | [C13.4](./05-simulation-consistency-and-fidelity-spec.md#c134-任务对象生命周期与-zombie-gc) |

### C14 — Fast-Forward / Step-Lock (P0)
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C14.1 Ban Dual Stepping | 🟡 | Hardened CI gate checks | [C14.1](./05-simulation-consistency-and-fidelity-spec.md#c141-时钟单一写入--禁止双重步进) |
| C14.2 No Dropped Edges | 🟡 | Global minimum timestamp stepping | [C14.2](./05-simulation-consistency-and-fidelity-spec.md#c142-快进跨越边沿--半窗-debounce) |
| C14.3 Plant Step-Lock | 🟡 | Ban wall-clock plants | [C14.3](./05-simulation-consistency-and-fidelity-spec.md#c143-plantos-锁步漂移) |
| C14.4 Pin Queue Overflow | 🟡 | Fail-Loud assertions | [C14.4](./05-simulation-consistency-and-fidelity-spec.md#c144-pin-event-queue-溢出--丢失) |
| C14.5 Step Sequence | 🟡 | Sequence contract validation | [C14.5](./05-simulation-consistency-and-fidelity-spec.md#c145-观测与注入竞态) |

### C15 — Host↔Wasm (P0)
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C15.1 Asyncify Contracts | 🟡 | Comprehensive test coverage | [C15.1](./05-simulation-consistency-and-fidelity-spec.md#c151-asyncify-挂起契约) |
| C15.2 Poll Model | ✅ | — | [C15.2](./05-simulation-consistency-and-fidelity-spec.md#c152-中断-pushpoll--重入) |
| C15.3 BigInt ABI | ✅ | Third-party plugin type risks | [C15.3](./05-simulation-consistency-and-fidelity-spec.md#c153-bigint--指针-abi) |
| C15.4 Bypass Leakage | 🟡 | Runtime probes | [C15.4](./05-simulation-consistency-and-fidelity-spec.md#c154-语义-bypass-泄露) |
| C15.5 Worker Isolation | ✅ | — | [C15.5](./05-simulation-consistency-and-fidelity-spec.md#c155-worker-隔离与主线程-starve) |

### C16 — OS Semantics (P0)
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C16.1 Mutex / Timeouts | 🟡 | Compatibility table + unit tests | [C16.1](./05-simulation-consistency-and-fidelity-spec.md#c161-mutex-锁--超时--递归) |
| C16.2 Queue Full Policy | 🟡 | Per-item parity with FreeRTOS | [C16.2](./05-simulation-consistency-and-fidelity-spec.md#c162-queue--ringbuf-满与覆盖策略) |
| C16.3 Event vs Timeout Order | 🟡 | Defined priority resolution | [C16.3](./05-simulation-consistency-and-fidelity-spec.md#c163-阻塞等待与超时唤醒序) |
| C16.4 Event Groups | — | N/A if unexposed | [C16.4](./05-simulation-consistency-and-fidelity-spec.md#c164-任务通知--事件组若暴露) |
| C16.5 Deadlock Detection | ❌ | Optional gate | [C16.5](./05-simulation-consistency-and-fidelity-spec.md#c165-死锁检测) |

### C17 — Resource Conflicts
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C17.1 Pin Conflicts | 🟡 | Codegen gate hardening | [C17.1](./05-simulation-consistency-and-fidelity-spec.md#c171-引脚复用冲突) |
| C17.2 Timer Exclusivity | 🟡 | Resource map coverage | [C17.2](./05-simulation-consistency-and-fidelity-spec.md#c172-定时器--pwm-通道独占) |
| C17.3 APB Clock Effects | 🚫 | Hardware | [C17.3](./05-simulation-consistency-and-fidelity-spec.md#c173-apb--外设时钟变更副作用) |
| C17.4 PWM Glitches | 🟡 | Optional models | [C17.4](./05-simulation-consistency-and-fidelity-spec.md#c174-pwm-占空比更新毛刺--相位连续性) |
| C17.5 Bus Contention | 🟡 | Overlapping window detection | [C17.5](./05-simulation-consistency-and-fidelity-spec.md#c175-共享总线所有权多驱动争用) |

### C18～C21
| Sub-Item | Status | Residual Gap | Link to 05 |
|---|---|---|---|
| C18.1 I2C NACK / Stretch / Hang | ❌ | Phase 3 Extension | [C18](./05-simulation-consistency-and-fidelity-spec.md#c18--总线故障态机超越-crc丢包) |
| C18.2 UART Framing / FIFO | ❌ | Same as above | Same as above |
| C18.3 SPI Modes / CS | ❌ | Same as above | Same as above |
| C19.1 Half-Transfer / Ping-Pong | ❌ | Depends on C8 | [C19](./05-simulation-consistency-and-fidelity-spec.md#c19--dma--缓冲生命周期) |
| C19.2 Buffer Reuse in Transfer | ❌ | Same as above | Same as above |
| C20.1 Yielding inside Callback | 🟡 | Context gating | [C20](./05-simulation-consistency-and-fidelity-spec.md#c20--回调重入--延迟下半部) |
| C21.1 uint32 Wrap-Around | 🟡 | Dedicated fast-forward tests | [C21](./05-simulation-consistency-and-fidelity-spec.md#c21--时间与计数回绕) |

### C22～C25 (Boundaries / On-Demand)
| Sub-Item | Status | Notes | Link to 05 |
|---|---|---|---|
| C22.* Low-Power / Clocks | 🚫 | Minimal stubs optional | [C22](./05-simulation-consistency-and-fidelity-spec.md#c22--电源--低功耗--时钟域) |
| C23.1 NVS Tearing | 🟡 | On-demand injection | [C23](./05-simulation-consistency-and-fidelity-spec.md#c23--持久化--nvs--磨损) |
| C24.* Cache / DMA RAM | 🚫 | Hardware | [C24](./05-simulation-consistency-and-fidelity-spec.md#c24--缓存--内存属性--dma-ram) |
| C25.1 UBSan Overflows | 🟡 | Host CI | [C25](./05-simulation-consistency-and-fidelity-spec.md#c25--浮点--数值与编译器-ub) |

---

## 3. Scenario Reading Order (Non-Scheduling SSOT)

Embedded engineering priority order:

| Sequence | Category | Recommended Action |
|---|---|---|
| 1 | C14 + C15 | Fast-forward/step-lock contracts and host boundary Fail-Loud mechanics |
| 2 | C13 + C16 | Cold boot manifests and OS semantic compatibility tables |
| 3 | C5.2 / C5.3 | Soft WDT + starvation statistics |
| 4 | C8 + C18/C19 | Asynchronous DMA + bus fault state machines + buffer lifecycles |
| 5 | C3 + C4 | Chaotic scheduling + TSan + FromISR context gates |
| 6 | C17 | Pin/timer resource conflict gates |
| 7 | C7.4 | Continuous zero-bypass audits |
| 8 | C12 | Nightly ABI dual-track testing |
| — | C9/C10/C11/C22/C24 | Hardware / HIL / 🚫 boundaries |

---

## 4. Explicit Non-Goals in Simulation Consistency

Even upon full completion of future Phases and total implementation of axes **A through F**, claims that "simulation is identical to hardware" or "eliminates physical board sign-off" are **strictly forbidden**. See [05 §0.4](./05-simulation-consistency-and-fidelity-spec.md#04-af-完备后的生产口径与残余不一致) and directory [README](./README.md).

Items explicitly unpromised:

1. Circuit-level SPICE / full power integrity simulation (C11)
2. Cycle-accurate Xtensa/RISC-V microarchitecture simulation in browser (daytime track) (C12)
3. ESP32 dual-core SMP + hardware cache coherency replication (C9/C24)
4. Full clock tree / APB frequency shift side effects (C17.3/C22)
5. Deep sleep current consumption and analog wakeup latency waveforms (C22)
6. Flash memory physical wear-leveling algorithms (C23)
7. Substituting simulation results for certification-grade EMC or functional safety assessments

Governed by [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md): **Behavioral and virtual timing high fidelity; timing concurrency and electrical dynamics governed by physical hardware; simulation catches escapes early, never proves zero escape.** Axes A–F complete = production-grade **pre-check pipeline**, $\neq$ virtual-physical identity.

---

## 5. Maintenance Governance

1. **Scenario Support Status Changes**: Modify "Status" columns in this document only.
2. **Problem Definition / Solution / Oracle Changes**: Modify [05](./05-simulation-consistency-and-fidelity-spec.md).
3. **Adding New Sub-Scenarios**: Write 5-field template in 05 first, then add index row in this document.
4. **Prohibited**: Duplicating technical narratives in this checklist.

---

## Revision History

| Date | Description |
|---|---|
| 2026-07-31 | Initial draft: C1–C12 |
| 2026-07-31 | SSOT deduplication: Technical solutions transferred to 05; this document established as index |
| 2026-07-31 | Aligned with 05: Expanded to C1–C25; key sub-scenario quick-reference tables; fixed dead links |
| 2026-08-02 | §4 cross-linked to 05 §0.4 / README: Completing A–F $\neq$ zero-hardware sign-off |
