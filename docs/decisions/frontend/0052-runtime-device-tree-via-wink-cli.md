# ADR: 工作台设备树动态解析契约

| 项 | 内容 |
|---|---|
| 决策文件名 | `0052-runtime-device-tree-via-wink-cli.md` |
| 归属系统 | `embedded-frontend` |
| 状态 | **Accepted** |

> 💡 **本地 AI 开发者导航**：
> 本决策的完整背景、方案比选、权衡论证与历史上下文位于私有通道：
> [`docs/.internals/packages/embedded-frontend/docs/internals/decisions/0052-runtime-device-tree-via-wink-cli.md`](../../.internals/packages/embedded-frontend/docs/internals/decisions/0052-runtime-device-tree-via-wink-cli.md)

---

## 结论摘要 (Decision Summary)

本架构决策已被正式批准并在 `embedded-frontend` 中固化实施。嵌入式 C 运行时（WinkMicroOS）遵照该决策对外暴露的标准契约执行，二者保持解耦。
