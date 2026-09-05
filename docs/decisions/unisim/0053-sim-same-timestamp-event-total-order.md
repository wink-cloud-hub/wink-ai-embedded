# ADR: 同时间戳事件全序排列

| 项 | 内容 |
|---|---|
| 决策文件名 | `0053-sim-same-timestamp-event-total-order.md` |
| 归属系统 | `unisim` |
| 状态 | **Accepted** |

> 💡 **本地 AI 开发者导航**：
> 本决策的完整背景、方案比选、权衡论证与历史上下文位于私有通道：
> [`docs/.internals/packages/unisim/docs/internals/decisions/0053-sim-same-timestamp-event-total-order.md`](../../.internals/packages/unisim/docs/internals/decisions/0053-sim-same-timestamp-event-total-order.md)

---

## 结论摘要 (Decision Summary)

本架构决策已被正式批准并在 `unisim` 中固化实施。嵌入式 C 运行时（WinkMicroOS）遵照该决策对外暴露的标准契约执行，二者保持解耦。
