# Ⅱb 保真轴薄索引（axes）

| 项 | 内容 |
|---|---|
| 层 | Ⅱb A~F 保障视角 |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| 职责 | 按轴回答「保障什么、上限在哪、链到哪」；**禁止**成为第二套实现 SSOT |

## 为何单独成目录

产品叙事按 A~F 宣称一致性；工程实现按机制子系统演进。物理分开后：

- 改时钟算法 → 只动 `02-mechanisms/02-virtual-clock.md`
- 改「轴 B 我们承诺什么」→ 只动 `B-timebase.md` + 必要时 overview 口径

## 轴 ↔ 机制基数（写死）

| 关系 | 基数 | 说明 |
|---|---|---|
| 轴 → primary 机制 | **恰好 1** | 每个轴必须有且仅有一个 primary home |
| 机制 → 作为 primary 的轴 | **0 或 1** | 禁止一机制挂两个 primary |
| 轴 → secondary 机制 | 0..N | 可选 |
| 横切机制 | primary = 0 | 如 `01-sandbox`、`10-bridge`：不归属任何轴为 primary |

例：`06-physical-degradation` 可以是 A/F 的 **secondary**，但不得同时当两个轴的 primary。

## 与 overview 的非对称（铁律）

| 位置 | 写什么 | 不写什么 |
|---|---|---|
| [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md) | 字母**定义** + 跨轴对照（上限**缩略**） | 机制算法、展开上限长文 |
| 本目录 `X-*.md` | 问题句可**逐字回声**一行；上限**展开**；primary/secondary；相关 C | **改写定义措辞**；粘贴算法；状态符；成熟度标签 |

## 每轴页固定模板（迁入时填充）

1. **回答的问题**（可与 overview 表逐字回声）
2. **主机制（primary）** — 必须与下表一致；禁止「他文件 `#子锚点`」作为唯一主链
3. **次机制（secondary）** — 可选；**仅** Ⅱa 机制（或契约 ADR），禁止把 overview / assurance 目录塞进 secondary
4. **典型上限 / 不可验边界**（展开版）
5. **相关 C 场景**（→ assurance spec；状态只查 checklist）

> **Wave 4 链接**：各轴 §5 的 C 编号链向 3.0 [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)；可测状态只查 [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)。

## 防腐（见根 00 §5）

- 禁止围栏代码块；建议 ≤120 行
- 禁止场景状态符与机制成熟度标签
- 轴页 primary 链接必须与下表一致（两处不得各说各话）
- secondary ≠ 第 Ⅲ 层 assurance；定义出处 ≠ secondary
## Primary home 对照表（轴页必须与此一致）

| 文件 | 轴 | Primary home |
|---|---|---|
| [A-peripheral-source.md](./A-peripheral-source.md) | A | `08-channel-routing` |
| [B-timebase.md](./B-timebase.md) | B | `02-virtual-clock` |
| [C-timer-semantics.md](./C-timer-semantics.md) | C | `09-timer-and-pwm-semantics` |
| [D-interrupt-model.md](./D-interrupt-model.md) | D | `04-interrupt-model` |
| [E-scheduler-concurrency.md](./E-scheduler-concurrency.md) | E | `03-scheduler-and-concurrency` |
| [F-fault-and-observation.md](./F-fault-and-observation.md) | F | `05-memory-and-faults` |

轴字母定义见 [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md)。
