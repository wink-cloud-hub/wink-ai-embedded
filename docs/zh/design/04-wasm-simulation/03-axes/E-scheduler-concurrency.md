# 轴 E — 调度与并发

| 项 | 内容 |
|---|---|
| 层 | Ⅱb 薄索引 |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| 定义出处 | [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md)（禁止改写定义措辞） |

## 1. 回答的问题

多任务、阻塞、临界区、多核

定义见 [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md) 轴 E 行。

## 2. 主机制（primary）

- [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) — 协作式单虚拟核调度、同步原语、STRICT_NONBLOCKING

## 3. 次机制（secondary）

（无强制 secondary。执行模式 / Asyncify 宿主细节按需见 [`../02-mechanisms/01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md)；中断 Poll 与调度 Phase 0 交叉见 [`D-interrupt-model.md`](./D-interrupt-model.md)。）

## 4. 典型上限（展开版）

在 overview 缩略「SMP / 真抢占 → 真机」之上展开：

1. **模型上限**：协作式单虚拟核；切换只发生在显式 yield 点——**不是**真抢占；纯 CPU 计算段不可被打断。
2. **多核**：`core_id` 不按核派发；无锁跨核撕裂、钉核时序、跨核 cache/DMA 一致性等 → 真机 + 静态分析。SMP 仿真当前拒绝（未来须新 ADR）。
3. **指令级竞态**：任意指令间刺入的撕裂不可在本模型下验（与轴 D 上限同源）。
4. **可收敛子集**：饿死、软 WDT、部分共享状态 / 阻塞门禁可在协作语义下更早暴露；混沌交错调度等增强尚未作为默认可验证据。
5. **同步原语**：Mutex/Sleep 等 App 级 API 行为子集可对齐；独立 Queue API、死锁检测、优先级继承等能力未闭合前勿当作已验契约。
6. **与轴 D 边界**：本轴回答任务何时切换、能否阻塞/并发；轴 D 回答 ISR 何时被派发、能否嵌套。共享 tick（Phase 0 drain → 再调度），但 ISR 延迟 ≠ 任务调度延迟；同刻事件总序见 [ADR-0053](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md)。

## 5. 相关 C 场景

场景契约 → [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md)。可测状态只查 [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)。

- **C3** — 共享状态竞态
- **C5** — 阻塞 / 饿死 / 看门狗
- **C9** — 多核 SMP 真实并发
- **C16** — OS 同步原语语义对齐

