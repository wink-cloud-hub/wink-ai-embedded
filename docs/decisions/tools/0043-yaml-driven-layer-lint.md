# ADR: YAML 驱动的分层 Lint 架构

| 项 | 内容 |
|---|---|
| 决策文件名 | `0043-yaml-driven-layer-lint.md` |
| 归属系统 | `wink-tools` |
| 状态 | **Accepted** |

> 💡 **本地 AI 开发者导航**：
> 本决策的完整背景、方案比选、权衡论证与历史上下文位于私有通道：
> [`docs/.internals/packages/wink-tools/docs/internals/decisions/0043-yaml-driven-layer-lint.md`](../../.internals/packages/wink-tools/docs/internals/decisions/0043-yaml-driven-layer-lint.md)

---

## 结论摘要 (Decision Summary)

本架构决策已被正式批准并在 `wink-tools` 中固化实施。嵌入式 C 运行时（WinkMicroOS）遵照该决策对外暴露的标准契约执行，二者保持解耦。
