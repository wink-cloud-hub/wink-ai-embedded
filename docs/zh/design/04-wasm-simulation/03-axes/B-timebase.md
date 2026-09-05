# 轴 B — 时间基

| 项 | 内容 |
|---|---|
| 层 | Ⅱb 薄索引 |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| 定义出处 | [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md)（禁止改写定义措辞） |

## 1. 回答的问题

delay / 超时 / 脉宽以谁为钟

定义见 [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md) 轴 B 行。

## 2. 主机制（primary）

- [`../02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) — `s_virtual_us` SSOT、单一写入 Gate、HEADLESS 快进

## 3. 次机制（secondary）

- 执行模式对快进 / yield 观感的影响（INTERACTIVE vs HEADLESS、Asyncify）→ [`../02-mechanisms/01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md)
- `timing` Accuracy Mode 证据效力（超声波等交叉宣称）→ [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md)

## 4. 典型上限（展开版）

在 overview 缩略「非墙钟实时；快进改变可观测墙钟时长」之上展开：

1. **模型上限**：逻辑时序锚定虚拟微秒钟，**不**承诺与宿主机墙钟实时对齐；HEADLESS 快进可使墙钟耗时远小于虚拟跨度。
2. **单 Gate**：`pal_delay_*` 不得主动步进 `s_virtual_us`；双重步进属不可接受的契约破坏（C14）。
3. **晶振**：当前不仿真晶振/时钟源漂移（非目标；见 [`06`](../02-mechanisms/06-physical-degradation.md) §1）；不得默认宣称含漂移。
4. **与轴 C 分工**：本轴回答「以谁为钟」；HW timer / PWM 周期 / capture「像不像芯片」属轴 C（[`C-timer-semantics.md`](./C-timer-semantics.md)）。
5. **交叉宣称**：超声波 ECHO 脉宽量测属 **A+B**（须 `timing` Accuracy Mode）；按键去抖属 **A+B+F**。见 overview 交叉示例。
6. **回绕**：业务侧 uint32 滴答仍须测回绕；内部超时用绝对 `wakeup_us`，不用「剩余 delta」跨快进。

## 5. 相关 C 场景

场景契约 → [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)。可测状态只查 [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)。

- **C2** — 虚拟微秒逻辑时序
- **C14** — 快进 / 联合仿真步进契约
- **C21** — 时间与计数回绕
