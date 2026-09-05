# 演进路线、CI 分层与文档治理

| 项 | 内容 |
|---|---|
| 文档层级 | ① 设计规范（UniSim 3.0 / assurance） |
| 文档状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| SSOT 职责 | Phase 里程碑、CI 三层、**机制成熟度总表**、维护规程、**代码→文档联动**、**ADR 回写索引**、**golden 向量治理** 唯一归属 |
| 关联代码 | `wink-tools/wink.py`；`.github/workflows/clang-tidy.yml`（完整 PR/nightly 仿真矩阵入口待核对） |
| 上次核对 | 2026-08-02 |
| 管辖 ADR | 0003、0009、0013、0014、0042、0045、0047、[0053](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)、[0054](../../../decisions/unisim/0054-sim-uart-async-rx-model-boundary.md)、[0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md) |
| 迁自 | `04-wasm-simulation-2.0/13-roadmap-and-governance.md` |

> 机制成熟度词表定义见根 [`00-README.md` §3.2](../00-README.md)；文档生命周期词表见根 §3.1。场景能不能验仍只查 [`02-consistency-checklist.md`](./02-consistency-checklist.md)。

---

## 0. 已冻结治理条款（迁入前生效）

### 0.1 代码变更 → 必须刷新文档

当某 `02-mechanisms/*`（或本目录相关文）**关联代码**路径下发生 API 签名、行为语义或 ABI 变更时：

1. **同一 PR** 更新对应文档正文，并刷新文首 **上次核对**；
2. Reviewer 检查：`wink-micro-os/osal/wasm/`、`targets/wasm/`、`@wink-ai/unisim` 的 ABI/契约 diff 是否伴有文档 diff；
3. 目标自动化：改了关联路径代码却未碰对应 `.md` → CI / `wink lint` **warning**（实现待迁入后落地）。

细则与操作清单亦见下文 §5；根摘要见 [`../00-README.md` §4.4](../00-README.md)。

### 0.2 ADR Accepted → 回写设计规范（可审计）

docs-adr 要求 Accepted 后立即回写 Layer ①。操作清单：

1. 按各文件文首 **管辖 ADR** 字段反查受影响文档（该字段即回写检查索引）；
2. 更新正文与（若需要）**管辖 ADR** 列表本身；
3. 刷新 **上次核对**；
4. 若影响成熟度，同步本文件成熟度总表与 mechanisms 文首「落地」行。

---

## 1. 核心失真维度（扩展）

| 维度 | 仿真现状 | 真机 | 逃逸风险 | 主要场景 |
|---|---|---|---|---|
| 并发模型 | 单虚拟核、协作 yield | 抢占 + 可能双核 | 隐藏无锁竞态 | C3、C9、C16 |
| 中断时序 | tick/yield Poll | 任意刺入、嵌套 | 临界区外错误修改 | C4、C15 |
| 总线与 I/O | 部分同步 0µs；丢包有，故障态机弱 | 异步 DMA + 丰富故障 | 窗口竞态、挂死恢复未测 | C7、C8、C18、C19 |
| 时间推进 | 虚拟时钟快进 | 连续墙钟 + 晶振误差 | 漏边沿、双步进、回绕 | C2、C14、C21 |
| 宿主边界 | Asyncify/JS plant | 无此层 | 仿真自欺、丢中断 | C14、C15 |
| 生命周期 | Worker 热复用 | 上电默认 | 残留状态当冷启动 | C13、C23 |
| 资源拓扑 | 易忽略互斥 | 硬件资源独占 | 引脚/定时器冲突板级爆炸 | C17 |
| 内存/ABI | 堆大、对齐宽松 | 小 SRAM、严格对齐 | OOM/padding 逃逸 | C6、C12、C24、C25 |

### 1.1 机制能力成熟度总表（截至 2026-08-02）

词表：根 [`00-README.md` §3.2](../00-README.md)。本表是**机制落地**索引；场景能不能验仍只查 [`02-consistency-checklist.md`](./02-consistency-checklist.md)。**整体落地**列与对应 `02-mechanisms/*`（或 overview）文首「落地」对齐；子项备注不得抬高整体标签。

| 机制 | 文档 | 整体落地 | 备注 / 子项 |
|---|---|---|---|
| 分层架构 / 代码地图 | [`../01-overview/01-architecture.md`](../01-overview/01-architecture.md) | Landed | 描述性；不含未落地能力承诺 |
| Worker / Asyncify / 执行模式 | [`../02-mechanisms/01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md) | Landed | INTERACTIVE + HEADLESS |
| 虚拟时钟单 Gate / 快进 | [`../02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) | Landed | 超声波零 Yield 目标路径 → Partial（见 08） |
| 协作调度 / Fiber / WCET | [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) | Landed | 混沌 PRNG → Planned；SMP → 拒绝/需新 ADR |
| IRQ Poll / 临界区补发 | [`../02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) | Landed | 溢出 Fail-Loud 强化 → Partial；优先级嵌套不可验（边界） |
| Fault 锁存 / safe-off / safeWrap | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | Partial | 子项 Fault/safe-off **Landed**；文首整体 Partial |
| 固定堆配额（ADR-0045） | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | Planned | CMake 三标志未检出 |
| 故障域 / 功耗模型 | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | Stub | Wave3 ABI 冻结；亦见 06 |
| 物理退化（抖动/RC/预热/I2C 丢包） | [`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) | Partial | 子项抖动/RC/预热/I2C/PRNG **Landed**；故障域/功耗 **Stub**；晶振 ppm **非目标** |
| PinArbiter / 配置源边界 | [`../02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md) | Partial | PinArbiter / TS↔ABI **Landed**；SchemaForm·画布 **Partial** |
| 电源域生命周期 | [`../02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md) | Planned | 设计面 |
| 通道 1 Pin / 1b PWM / 2 Bus 路由 | [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) | Partial | 通道 1/1b/2 **Landed**；超声波沿 **Partial**（`distanceCm` 捷径 **Deprecated**）；3/4 **Planned** |
| 轴 C 定时器 / PWM 语义 / FOC 软步进 | [`../02-mechanisms/09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) | Partial | duty **Landed**；`pal_hwtimer`/FOC 软步进 **Partial～Planned**（树内无符号）；通用 capture **Planned** |
| Wasm↔JS ABI 契约与防漂移 | [`../02-mechanisms/10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md) | Landed | 个别导出能力本身可能 Stub（如功耗） |
| Accuracy Mode（behavioral/timing/cycle） | [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) | Partial | SSOT 已立；CI 强制证据链待补强 |
| 观测平面（VCD/Recorder/BusAnalyzer） | [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) | Partial | 证据效力规则已文档化 |
| 生命周期 / 复位 / 多 Wasm 边界 | [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) | Partial | 推荐复位序已写；冷启动强制门禁仍弱 |

变更本表时：同步改对应 mechanisms / overview 文首「落地」行，并在 §6 修订记录登记。

---

## 2. 演进阶段里程碑

> 状态 SSOT 是 [`02-consistency-checklist.md`](./02-consistency-checklist.md)。下表给「建设什么、达到什么保真级」。**无退出标准不得自称 Phase 完成。**

| Phase | 核心能力 | 保真级 | 主要 C 类 |
|---|---|---|---|
| **Phase 1（MVP+）** | 虚拟时钟、物理注入、快进契约加固 | 虚拟时序保真（Virtual-Time Fidelity） | C1、C2、C13、C14、C15 |
| **Phase 2（Wave A）** | 堆配额、Sanitizer、栈 watermark | 资源对等（Resource Parity） | C6、C25 |
| **Phase 3（Wave B）** | 驱动同源、异步 DMA、总线故障态机 | 协议对等（Protocol Parity） | C7、C8、C18、C19 |
| **Phase 4（Wave C）** | 混沌调度、TSan、软 WDT、OS 语义钉死 | 时序对等（Temporal Parity） | C3、C4、C5、C9、C16、C20 |
| **Phase 5（Wave D）** | 指令/ABI 双轨 | 指令对等（Instruction Parity） | C12、C24 |
| **持续/按需** | 资源冲突门禁、NVS 撕裂、低功耗声明 | — | C17、C21、C22、C23 |
| **真机/HIL** | 双核、硬实时、SPICE、cache/DMA | — | C9、C10、C11、C22、C24 |

### 2.1 Phase 退出标准（Exit Criteria）

每条须可判定（有验证入口或明确交付物）。未满足则 Phase 状态保持「进行中」。

| Phase | 退出标准（须同时满足） |
|---|---|
| **Phase 1** | (1) C2 / C14 核心子项验证入口已绑定且 PR 门禁覆盖单一时钟 Gate；(2) C13.1 冷启动向量可复现（两次 INIT 无业务残留）；(3) C15 Fail-Loud 路径有回归入口 |
| **Phase 2** | (1) ADR-0045 固定堆三标志已入 CMake 且 C6.1 入口非 `待补`；(2) Tier1 ASan Pass 稳定绿；(3) C25.3 / ADR-0055：至少 1 个物理 golden 标注 `fp_mode` |
| **Phase 3** | (1) C8 异步传输窗口有可复现用例（不再仅同步返回冒充完成）；(2) C18 至少一类总线故障注入套件进 Tier2；(3) C7.4 Bypass 审计有可重复命令 |
| **Phase 4** | (1) 已知竞态套件在混沌 + TSan（或影子等价物）下进入 Fault；(2) 临界区内零逃逸（套件文档化）；(3) C5.2 软 WDT 可检出挂死；(4) C16 原语对照表有测试钉死的子集 |
| **Phase 5** | (1) 夜间 ABI/双轨抽样套件可跑；(2) 至少 1 个 padding/对齐敏感用例在夜间轨失败或告警；(3) 日间轨不宣称指令级 |

**真机/HIL** 与 **持续/按需** 不设「完成」标签；以发布清单勾选为准。

---

## 3. CI 测试集分层（Test Tiers）

C1~C25 共 **102** 个子场景（Wave 4C 迁入时 98；本波增补 C1.5 / C2.5 / C13.5 / C21.4）。分层目标是让快测试挡 PR、慢测试挡 nightly、最贵的按需/发布前。

> **证据状态**：下表「核心覆盖」在对应子项验证入口仍为 `待补` 前视为 **aspirational（意向映射）**，不得解读为「已在 CI 挡住」。闭合进度以 [`02`](./02-consistency-checklist.md) 验证入口列为准。

| Tier | 响应目标 | 触发 | 核心覆盖 | 验证手段 |
|---|---|---|---|---|
| **Tier 1（PR 门禁）** | <15s | 每次 push/PR | C1、C2、C5.1、C6（ASan/堆配额）、C13.1（冷启动）、C14.1（单一写入源）、C15（Fail-Loud） | 静态编译断言、快速 HEADLESS 单测、ASan Pass 1 |
| **Tier 2（夜间混沌）** | <15min | 每夜 CI | C3（混沌+TSan 影子）、C4（中断抢占）、C7、C8（异步 DMA）、C16（OS 语义）、C18、C19 | 多种子混沌回归、TSan 并发、总线故障注入 |
| **Tier 3（深度/HIL）** | 按需/每周 | 发布前/硬件流水线 | C10（FOC 电机 HIL）、C12（RISC-V32 解释器二进制比对）、C22（低功耗）、C24（DMA RAM） | 指令级解释器双轨 trace 比对、真机 HIL 板卡自动化 |

**Sanitizer Pass**（ADR-0045）：`python wink-tools/wink.py test` 矩阵含 Pass 3（ASan Pass）；Windows MinGW 无 `libasan` 降级 `-fsanitize-undefined-trap-on-error`，完整 ASan 在 Clang/Emscripten。FP / golden **容差策略**见 [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md)；golden **文件治理**见下文 §4.1。

---

## 4. 子场景维护规程

1. **新增子场景**：先在 [`01-consistency-spec.md`](./01-consistency-spec.md) 按 §0.1 写齐五字段（🚫 / 真机·HIL 独占可走字段豁免），再在 [`02-consistency-checklist.md`](./02-consistency-checklist.md) 加一行状态（默认未验/弱验口径、验证入口 `待补`）并锚链回 `01`。
2. **引擎交付**：只更新实现/工作项跟踪与 `02` 的状态列；**禁止**只改 `02` 状态而不更新 `01` 的方案。
3. **刻意不覆盖**：必须**同时**出现在 `01` 该子场景的「边界」字段 **与** `02`「明确不在承诺内」清单。
4. **禁止双写**：`01` 的子场景正文不在 `02` 复述；`02` 只给状态 + 一句话缺口 + 验证入口指针。**禁止**在本 roadmap 粘贴 C 场景保障方案正文。
5. **冲突修正流程**：若发现 `01`（方案）与 `02`（状态）或代码三者不一致，以**代码**为最终事实来源，先改 `01` → 再改 `02` → 本文件 §6 修订记录登记。
6. **机制成熟度变更**：能力标签变更时，必须同步更新 **§1.1 总表** + 对应 mechanisms 文首「落地」行；标签只能用根 [`00-README.md` §3.2](../00-README.md) 词表。
7. **✅ 翻转门禁**：将 `02` 现状改为 ✅ 的 PR 必须：(a) 验证入口非 `待补`/`N/A`；(b) PR 描述引用对应 `01` 验收预言（oracle）；(c) Reviewer 能按入口复现。存量 ✅ + `待补` 仅「声称已支持」，补入口时不视为新功能，但须在 PR 说明绑定了哪条预言。
8. **现状列一符**：禁止双符；副语义写「残余缺口」（见 `02` §0.1）。

### 4.1 Golden 向量治理

> FP / 跨 host 容差策略的 SSOT 仍是 [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md)。本节只管**向量文件怎么改**，不重写容差公式。

1. **入库**：golden 向量（含整数轨迹与标注了 `fp_mode` 的浮点向量）纳入版本控制；禁止仅存在于开发者本机。
2. **更新**：PR 必须写明「为何不是回归」（算法契约变更 / seed·消费序变更 / 宿主矩阵扩容等）；关联 C 子项编号（如 C2.5、C25.3）。
3. **防顺手改绿**：CI 对 golden diff **失败即红**；禁止在 CI 脚本里静默 rebase/覆盖 baseline；本地 `--update-golden` 类开关若存在，不得进入默认 PR 门禁路径。
4. **评审**：非琐碎 golden 更新至少一人 review；PRNG 全局序或物理参数表变更视为高风险，须同步 mechanisms `06` 与 assurance 入口。

---

## 5. 文档治理（2.0 → 3.0）

### 5.1 2.0 → 3.0 简表

| 2.0 | 3.0 |
|---|---|
| `00-README.md`（A~F / 口径 / 词表） | [`../01-overview/`](../01-overview/) + 根 [`../00-README.md`](../00-README.md) §3 |
| `01-architecture.md` | [`../01-overview/01-architecture.md`](../01-overview/01-architecture.md) |
| `02`…`10`、`15` 机制文 | [`../02-mechanisms/01`](../02-mechanisms/01-sandbox-and-execution.md)…[`11`](../02-mechanisms/11-accuracy-observation-lifecycle.md)（**09 定时器语义**从旧通道文拆出为独立 `09-timer-and-pwm-semantics`） |
| `11` / `12` / `13` | 本目录 `01` / `02` / `03`（本文件） |
| （无独立轴页） | [`../03-axes/`](../03-axes/) Ⅱb 薄索引 |

历史 1.0（`04-wasm-simulation/`）对照说明仍以 2.0 `13` §5.1 为归档；本文件不再复制 1.0→2.0 长表。

### 5.2 3.0 四层 SSOT 红线

| 信息 | 唯一归属 |
|---|---|
| A~F 轴定义与生产口径 | [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md)、[`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md) |
| **实现成熟度词表** | 根 [`../00-README.md` §3.2](../00-README.md) |
| **机制能力成熟度总表** | **本文件 §1.1** |
| 机制实现正文 | [`../02-mechanisms/`](../02-mechanisms/00-README.md) |
| 轴页上限/索引（薄） | [`../03-axes/`](../03-axes/00-README.md) |
| C 项机制/方案/预言正文 | [`01-consistency-spec.md`](./01-consistency-spec.md) |
| C 项可测性状态 + 验证入口 | [`02-consistency-checklist.md`](./02-consistency-checklist.md) |
| Phase / CI / 维护规程 / 代码→文档 / ADR 回写 / golden 文件治理 | **本文件**（§0 + §2–§4） |

跨文件引用用锚链，不复制正文。路径纪律：UniSim 侧使用 `@wink-ai/unisim` 模块级契约描述，统一按 SDK 导出规范引用，避免跨仓硬编码内部源码路径。

---

## 6. 修订记录

| 日期 | 变更 |
|---|---|
| 2026-08-02 | （2.0 历史摘要）重组为 2.0 文档包；引入成熟度词表与机制总表；P1 闭合 Accuracy/`15`；轴 C 专篇并入通道文后于 3.0 再拆为 `09-timer…`。 |
| 2026-08-02 | **Wave 4A**：迁入 3.0 `04-assurance/03`；路径重映射至 mechanisms/overview；成熟度总表与文首「落地」对齐；管辖 ADR 增补 0053/0054/0055。 |
| 2026-08-02 | **Wave 4C**：[`02-consistency-checklist.md`](./02-consistency-checklist.md) 状态矩阵迁入（98 子项）；`#cN` 锚点兑现；assurance 层闭合。 |
| 2026-08-02 | **Assurance 审阅修补**：子项 98→102（C1.5/C2.5/C13.5/C21.4）；checklist 验证入口列 + 单符纪律；Phase exit criteria；✅ 翻转门禁；Tier aspirational 标注；§4.1 golden 治理。 |
| 2026-08-02 | **§7 门禁通过，3.0 切换 Active**：C25.1/C25.2/C25.3 补齐五字段；1.0/2.0 归档（Archived）；`docs/design/README.md` 与 `CLAUDE.md` 入口指向 3.0。C 档（反测/UART async RX/禁 fast-math CI 等）为代码/CI 后续项，不阻塞文档 Active。 |

