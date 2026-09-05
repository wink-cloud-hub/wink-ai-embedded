# 轴 D — 中断模型

| 项 | 内容 |
|---|---|
| 层 | Ⅱb 薄索引 |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| 定义出处 | [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md)（禁止改写定义措辞） |

## 1. 回答的问题

ISR 何时跑、能否抢占嵌套

定义见 [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md) 轴 D 行。

## 2. 主机制（primary）

- [`../02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) — Poll 队列、临界区补发、不可验边界

## 3. 次机制（secondary）

- Asyncify / 单线程宿主为何做不到任意指令刺入 → [`../02-mechanisms/01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md)
- 调度 Phase 0 / tick 驱动 Poll、同刻总序 → [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md)

## 4. 典型上限（展开版）

在 overview 缩略「不可验优先级嵌套 / 硬实时抢占」之上展开：

1. **模型上限**：协作式 Poll ≠ NVIC 抢占嵌套；**不可**验优先级嵌套、任意指令间刺入、微秒级硬实时 IRQ。
2. **延迟量级**：未处临界区时，边沿入队到 ISR 最坏约一个调度 tick（默认约 10ms 量级），不是微秒级。
3. **临界区**：nest-count + 最外层 unlock 补发可对齐「开中断瞬间兑现 pending」的行为子集，但**不等价于**真机关全局中断的全部副作用。
4. **timing 宣称禁区**：高波特异步 UART RX、紧周期多字节帧间隔、假设「边沿后 μs 内必进 ISR」的状态机——须 HIL，或降为 behavioral / 改通道模型（与轴 A UART 上限交叉）。
5. **同刻总序**：与定时唤醒 / Pin Event 的跨源总序契约见调度机制与 [ADR-0053](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)；跨源 bit-exact 反测闭环未完成前，不得冒充已证明。
6. **与轴 E 边界**：本轴回答 ISR 何时被派发、能否嵌套；轴 E 回答任务何时切换、能否阻塞/并发。二者在 tick 边界相遇（Phase 0 drain IRQ → 再调度任务），但 ISR 延迟 ≠ 任务调度延迟。
7. **交叉宣称**：I2C 从机故障 / 丢包属 **A+D+F**。见 overview 交叉示例。

## 5. 相关 C 场景

场景契约 → [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)。可测状态只查 [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)。

- **C4** — 临界区与中断抢占 / 嵌套
- **C15** — Host↔Wasm 边界诚实性
- **C20** — 回调重入 / 延迟下半部

