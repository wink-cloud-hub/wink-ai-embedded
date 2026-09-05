# 轴 F — 故障与观测

| 项 | 内容 |
|---|---|
| 层 | Ⅱb 薄索引 |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| 定义出处 | [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md)（禁止改写定义措辞） |

## 1. 回答的问题

OOM、WDT、竞态、Trace

定义见 [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md) 轴 F 行。

## 2. 主机制（primary）

- [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) — 堆配额 / Fault 锁存 / safe-off / 消毒器

## 3. 次机制（secondary）

- 物理退化与注入（观测侧输入）→ [`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md)
- Accuracy Mode、观测平面、生命周期证据效力 → [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md)

## 4. 典型上限（展开版）

在 overview 缩略「清单中须 HIL 的项必须真机」之上展开：

1. **治理上限**：checklist 中标为真机/HIL 独占的场景，仿真绿灯**不得**当作放行依据（正式口径见 [`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md)）。
2. **配额 / OOM**：固定堆封顶是设计契约；构建层三链接标志尚未全部接线前，不得宣称「配额门禁已生效」。
3. **故障域 / 功耗**：ABI 可预埋，实现未闭合时不得宣称域隔离或能耗模型可用。
4. **观测分级**：`behavioral` / `timing` / `cycle` 证据效力不同；错误 Accuracy Mode 下的 Trace 不得越级宣称。
5. **复位边界**：热 reset 与冷启动覆盖面不同；跳过物理复位的热复用路径不得宣称覆盖冷启动类场景。
6. **交叉宣称**：按键去抖属 **A+B+F**；总线故障属 **A+D+F**。浮点/golden 容差策略见 [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md)。

## 5. 相关 C 场景

场景契约 → [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)。可测状态只查 [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)。Accuracy / 观测细节见 [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md)。

- **C6** — 栈 / 堆 / 内存安全
- **C15** — Host↔Wasm 边界诚实性
- **C25** — 浮点 / 数值与编译器 UB
- （门禁类）亦常交叉 **C5**（软 WDT）、**C13**（生命周期 / 复位）

