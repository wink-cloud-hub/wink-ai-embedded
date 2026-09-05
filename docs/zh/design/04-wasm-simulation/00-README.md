# Wasm 仿真与前端运行时引擎（UniSim）— 3.0 SSOT

| 项 | 内容 |
|---|---|
| 文档层级 | ① 设计规范（物理路径 `docs/design/04-wasm-simulation/`） |
| **文档状态** | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT 阅读入口；2026-08-11 Amend 修订补强） |
| 前身 | `04-wasm-simulation-2.0/`（2.0，曾为 Active；2026-08-02 归档后删除，迁移溯源见各文件文首「迁自」与本节 §6 映射表）；[04-wasm-simulation/](../04-wasm-simulation/)（1.0，**Archived**，历史对照，不再演进） |
| 关联 ADR | 0002、0003、0009、0013、0014、0019、0025、0040、0042、0045、0047 |
| 关联代码（总览） | `wink-micro-os/osal/wasm/`、`wink-micro-os/targets/{wasm,common}/`、`@wink-ai/unisim`（**UniSim 仿真引擎底座**；独立 TS SDK 包契约，以模块级定义为准。逐模块规则见 §4.1） |
| 上次核对 | 2026-08-11 Amend（基于 `review.md` 嵌入式架构评审修补：PWM 通道 1b 重分类、降级不丢信息量硬铁律、控制面 IRQ/DMA/Timer 补齐） |

> **现行入口**：3.0 已于 2026-08-02 通过 §7 门禁切换为 **Active**，是 Wasm 仿真设计的现行 SSOT 阅读入口。2026-08-11 依据 `review.md` 评审补充 **Amend**：修正 PWM 通道分类（通道 1b）、声明“Behavioral 降级不得破坏脉宽/测距信息语义”、补充控制面三线（IRQ/DMA/Timer）。TS 侧配套设计见 `wink-ai/packages/unisim/docs/design/unified-peripheral-channel-architecture.md`。

---

## 0. 为什么是 3.0 四层，而不是扁平 01–15

2.0 用单一编号序列承载了三套正交索引（机制 / A~F / C 场景），读者不知道「主轴」是什么；轴 C 埋进通道文、轴 F 散落多篇，分类承诺与文件边界不一致。

3.0 用**物理目录**固定四种职责，杜绝双写：

| 层 | 目录 | 回答的问题 | 厚度 | 可写正文？ |
|---|---|---|---|---|
| **Ⅰ 宏观** | [`01-overview/`](./01-overview/) | 概念、架构、方法论、生产口径、术语 | 中 | 是（口径与定义） |
| **Ⅱa 机制** | [`02-mechanisms/`](./02-mechanisms/) | 引擎子系统怎么实现 | **厚** | **是（实现 SSOT）** |
| **Ⅱb 保真轴** | [`03-axes/`](./03-axes/) | A~F 各轴保障什么、上限在哪 | **薄** | 仅索引+上限；**禁止**复制机制细节 |
| **Ⅲ 验收治理** | [`04-assurance/`](./04-assurance/) | 验什么、能不能验、怎么演进 | 中 | 是（场景/状态/规程） |

```text
宣称「高一致」
    │
    ├─ 读 Ⅰ  → 口径与边界（完备 ≠ 恒等）
    ├─ 按 A~F 查 Ⅱb → 该轴上限 + 链到机制/场景
    ├─ 改引擎读 Ⅱa → 唯一实现正文
    └─ 验收读 Ⅲ   → C 契约 / 状态矩阵 / 成熟度与 CI
```

---

## 1. 目录树（本阶段交付物）

```text
04-wasm-simulation/
├── 00-README.md                          ← 本文件：总入口 + SSOT 铁律 + 词表
├── 01-overview/                          ← Ⅰ 宏观整体
│   ├── 00-README.md
│   ├── 01-architecture.md
│   ├── 02-axes-af.md                     ← A~F 定义唯一出处
│   ├── 03-production-contract.md
│   ├── 04-methodology.md
│   └── 05-glossary.md                    ← 术语权威释义
├── 02-mechanisms/                        ← Ⅱa 引擎机制（实现 SSOT）
│   ├── 00-README.md
│   ├── 01-sandbox-and-execution.md
│   ├── 02-virtual-clock.md
│   ├── 03-scheduler-and-concurrency.md
│   ├── 04-interrupt-model.md
│   ├── 05-memory-and-faults.md
│   ├── 06-physical-degradation.md
│   ├── 07-peripheral-registry.md
│   ├── 08-channel-routing.md             ← 轴 A 数据面（含 PWM 作为通道 1b 的路由）
│   ├── 09-timer-and-pwm-semantics.md     ← 轴 C 主机制（软步进 / duty / capture / pal_hwtimer）
│   ├── 10-wasm-js-bridge-abi.md
│   └── 11-accuracy-observation-lifecycle.md
├── 03-axes/                              ← Ⅱb 保真轴薄索引（禁止双写）
│   ├── 00-README.md
│   ├── A-peripheral-source.md
│   ├── B-timebase.md
│   ├── C-timer-semantics.md
│   ├── D-interrupt-model.md
│   ├── E-scheduler-concurrency.md
│   └── F-fault-and-observation.md
└── 04-assurance/                         ← Ⅲ 验收与治理
    ├── 00-README.md
    ├── 01-consistency-spec.md
    ├── 02-consistency-checklist.md
    └── 03-roadmap-and-governance.md
```

---

## 2. SSOT 铁律（长期可维护的硬约束）

1. **一义一处**：同一技术事实只在一个文件写正文；其它文件只链过去。
2. **Ⅱa 与 Ⅱb 不对称**：机制可厚写；轴页只含固定模板字段。发现轴页粘贴算法/状态机/ABI 表 → 立刻搬回 `02-mechanisms/`。
3. **每轴恰好一个 primary home**：每个轴指向**恰好一个**主机制文件；允许 secondary。一个机制至多作为 **0 或 1** 个轴的 primary（横切文件如 sandbox/bridge 可为 0）。**禁止**「唯一主链 = 他轴主文件的子锚点」；**禁止**给同一机制挂两个 primary。
4. **overview ↔ 轴页非对称**：
   - [`01-overview/02-axes-af.md`](./01-overview/02-axes-af.md)：字母**定义** + 跨轴对照表（上限为缩略）；
   - `03-axes/X`：问题句可逐字回声一行；上限为**展开版**；**禁止改写** overview 中的定义措辞。
5. **A~F 定义只在** `02-axes-af.md`；术语释义权威处为 [`01-overview/05-glossary.md`](./01-overview/05-glossary.md)。
6. **场景状态只在** [`04-assurance/02-consistency-checklist.md`](./04-assurance/02-consistency-checklist.md)；spec 不写 ✅/🟡/❌/🚫。
7. **机制成熟度标签**只用下文 §3 词表；总表只在 [`04-assurance/03-roadmap-and-governance.md`](./04-assurance/03-roadmap-and-governance.md)。文首「落地」行必须能用 **关联代码** 核验。
8. **正文已迁齐（Active）**：3.0 已于 2026-08-02 经 §7 门禁切换为 **Active**，各文件正文为现行 SSOT；1.0 已 **Archived**，2.0 归档后删除。新增/修改仍须遵守本节铁律（一义一处、Ⅱa/Ⅱb 不对称、状态只在 checklist 等），**不得**从 1.0 回灌旧正文造成双写。
9. **STRICT_NONBLOCKING**：纪律与「为什么」在 methodology；构建落地在 sandbox 与/或 scheduler，**双向链接**。
10. **代码变更必须刷文档**：见 §4.4；细则与 ADR 回写索引见 [`04-assurance/03-roadmap-and-governance.md`](./04-assurance/03-roadmap-and-governance.md)。

---

## 3. 两套正交词表

### 3.1 文档生命周期状态（本目录 / 本文件级）

> **本词表仅此一处定义**。描述的是**文档包**状态，不是机制落地。

| 标签 | 含义 |
|---|---|
| **Scaffold** | 仅目录、铁律、文首与结构模板；无迁入正文 |
| **Migrating** | 正在从旧版迁入正文；**不得**作为对外现行阅读入口 |
| **Active** | 现行 SSOT；对外入口指向本目录 |
| **Archived** | 只读历史对照，不再演进 |

迁移约定：开工迁入时 3.0 → **Migrating**，2.0 保持 **Active**；§7 门禁全过后再 3.0 → **Active**，2.0（及确认后的 1.0）→ **Archived**。**本切换已于 2026-08-02 完成**：3.0 为现行 Active SSOT，1.0 保留 Archived；2.0 于归档后删除（迁源文件名以纯文本保留于各文「迁自」行与 §6 映射表）。

### 3.2 实现成熟度词表（机制落地状态）

> **本词表仅此一处定义**。`02-mechanisms/*` 文首「落地」行与 assurance 成熟度总表必须使用下列标签。

| 标签 | 含义 |
|---|---|
| **Landed** | 端到端可用：主路径有实现，且文首 **关联代码** 可指向源文件/测试 |
| **Partial** | 主路径可用，但有已知捷径、缺口或模式限制 |
| **Stub** | ABI/接口已冻结，行为为空或未接线 |
| **Planned** | 仅有设计/选型，无实现承诺 |
| **Deprecated** | 代码仍在，禁止新用 |

**与场景可测正交**：机制落地 ≠ 某 C 场景可验。场景标记仅用 ✅ / 🟡 / ❌ / 🚫。

---

## 4. 机制 / 轴 / 验收文首必填字段

### 4.1 `02-mechanisms/*.md`（实现文）

| 字段 | 要求 |
|---|---|
| **落地** | §3.2 词表之一 |
| **关联代码** | C 侧填本仓相对路径；TS 仿真引擎侧填写 `@wink-ai/unisim` 包级与逻辑组件名（如 `@wink-ai/unisim (PinArbiter)`），按公开模块粒度标注；Landed/Partial 不得留空 |
| **上次核对** | `YYYY-MM-DD`；文档与代码对齐日期 |
| **管辖 ADR** | 本文件实际受约束的 ADR 编号（非根上的全集拷贝） |
| **支撑轴** | primary / secondary 轴字母；横切可为空 |

### 4.2 `04-assurance/01` 与 `03`

同样填写 **关联代码**（若适用）/ **上次核对** / **管辖 ADR**。checklist 以状态矩阵为主，核对日仍要有。

### 4.3 `03-axes/*.md`

不写落地/Landed；写 **主机制（primary）** 与可选 **次机制（secondary）**。轴页 primary 链接必须与 [`03-axes/00-README.md`](./03-axes/00-README.md) 对照表一致（两处不得各说各话）。

### 4.4 代码变更 → 刷新文档（防腐烂）

当某文件 **关联代码** 所列路径发生 **API 签名、行为语义或 ABI** 变更时：

1. 同一 PR 必须更新对应 mechanism（或 assurance）**正文**并刷新 **上次核对**；
2. Reviewer 对 `wink-micro-os/osal/wasm/`、`wink-micro-os/targets/wasm/`、`@wink-ai/unisim` 导出的 ABI/契约改动，须检查是否有配套文档 diff；
3. 目标：CI/`wink lint` 对「改了关联路径下的代码却未碰对应 `.md`」发 **warning**（迁入后尽快落地；规程归属 [`04-assurance/03`](./04-assurance/03-roadmap-and-governance.md)）。

---

## 5. 薄轴页防腐（迁入后用工具强制）

| 规则 | 说明 |
|---|---|
| 禁止围栏代码块 | 算法属于 mechanisms |
| 单文件建议 ≤ **120** 行 | 超限 → 内容搬回 mechanisms / overview |
| 禁止 ✅/🟡/❌/🚫 | 状态只在 checklist |
| 禁止 Landed/Partial/Stub/Planned/Deprecated | 成熟度只在 mechanisms 文首 + roadmap 总表 |
| primary 链接 ∈ axes README 表 | 与 §4.3 一致 |

---

## 6. 与 2.0 文件映射（迁入清单）

| 3.0 目标 | 主要迁自 2.0 |
|---|---|
| `01-overview/01-architecture.md` | `01-architecture.md` |
| `01-overview/02-axes-af.md` | `00-README.md` §1 |
| `01-overview/03-production-contract.md` | `00` §2–§3；`11` 口径段 |
| `01-overview/04-methodology.md` | `00` §0/§4；`11` §0.1–§0.3（STRICT「为什么」） |
| `01-overview/05-glossary.md` | 新建（从全文抽术语；不拷机制正文） |
| `02-mechanisms/01`…`08` | `02`…`09`（通道文迁入时**剥掉**定时器语义专节） |
| `02-mechanisms/09-timer-and-pwm-semantics.md` | `09` §1.4/§5.3 等 + ADR-0047 相关 |
| `02-mechanisms/10-wasm-js-bridge-abi.md` | `10-wasm-js-bridge-abi.md` |
| `02-mechanisms/11-accuracy-…` | `15-accuracy-observation-lifecycle.md` |
| `03-axes/A`…`F` | 新建薄页（从 00 §1 展开索引） |
| `04-assurance/01`…`03` | `11` / `12` / `13`（spec 按已冻结五字段模板填；加 C 索引 TOC；去重） |

---

## 7. 切换入口门禁（迁入「填满」≠ 可切换）

> **2026-08-02 全部通过并完成切换**：3.0 → Active；1.0 保留 Archived，2.0 归档后删除。下列 checkbox 为门禁记录；新增/重构仍须重新满足相应项。

以下全部满足后，方可：3.0 → **Active**；2.0 → **Archived**；并明确 1.0（`04-wasm-simulation/`）→ **Archived** 或删除（不得三版并存却不声明现行入口）。

### 7.1 Overview（口径与定义 SSOT）

- [x] `01-overview/02-axes-af.md` 的 A~F 定义表已填（轴页禁止改写措辞的前提）
- [x] `03-production-contract.md` 生产口径已定稿（其它文件禁止另写承诺的前提）
- [x] `05-glossary.md` 待收词目全部有定义（非 TODO；29 词条）
- [x] `04-methodology.md` 与 sandbox/scheduler 的 STRICT 双向链接齐全

### 7.2 Mechanisms / Axes

- [x] 所有 `02-mechanisms/*` 文首有 **关联代码**（跨仓路径可核验）+ **上次核对** + **管辖 ADR**
- [x] 轴 C 主链指向 `09-timer-and-pwm-semantics.md`（非 channel 子锚点）
- [x] `08-channel-routing` 与 `09-timer-…` 交叉处仅一句 + 反向链接，无双写算法
- [x] 各轴页 primary 与 `03-axes/00-README` 对照表一致
- [x] 薄轴页 lint（或等价人工清单）通过（六轴均 ≤42 行，无围栏代码/状态符/成熟度词）

### 7.3 Assurance / 仓库入口

- [x] spec 有 C 场景索引 TOC；子场景均按五字段模板（C25.1/C25.2/C25.3 已补齐）；checklist 无方案正文
- [x] 全文 `C<N>.<M>` 子项引用（如 C1.2/C5.2/C15.5）均可解析到 spec 锚点（102 子项、127 显式锚点，0 断链）
- [x] roadmap 成熟度总表与文首落地行一致；含代码→文档联动与 ADR 回写规程
- [x] 全库相对链接无死链；CLAUDE.md / 设计规范入口已更新
- [x] 1.0 / 2.0 处置已写入归档说明（1.0 文首 Archived banner；2.0 归档后删除，迁源以纯文本溯源）

---

## 8. 迁移记录与后续

> **2026-08-02 已通过 §7 门禁切换为 Active**：Wave 1–4 正文迁齐，3.0 为现行 SSOT，1.0 保留 Archived，2.0 归档后删除。以下为迁移波次记录与 Active 后仍在推进的工程项。

1. **Wave 1–4 全部完成**：overview（Wave 1）、mechanisms `01`–`11`（Wave 2A–2D，含 08/09 定时器语义原子拆分）、axes A–F（Wave 3）、assurance（Wave 4A–4C + 审阅修补，子项 98→102，C1.5/C2.5/C13.5/C21.4）均已迁入并闭合。
2. **Mechanisms 审阅闭环**：[implementation-plans/2026-08-02-unisim3-mechanisms-review-closure-plan.md](../../implementation-plans/unisim/2026-08-02-unisim3-mechanisms-review-closure-plan.md) — A 档补丁 ✅；B 档 [ADR-0053](../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)/[0054](../../decisions/unisim/0054-sim-uart-async-rx-model-boundary.md)/[0055](../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md) **Accepted 且已回写**；C 档（同刻总序反测、UART async RX 实现、禁 fast-math CI 等）作为代码/CI 项继续推进，不阻塞文档 Active。
3. **Active 后治理**：落地薄轴页 lint + 代码→文档 warning（§4.4）；按 [`04-assurance/03`](./04-assurance/03-roadmap-and-governance.md) 维护成熟度总表与 ADR 回写。
4. 文档层改动不再动产品代码或 ADR 编号（除非 Accepted 回写）。

