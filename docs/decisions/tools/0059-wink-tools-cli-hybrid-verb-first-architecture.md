# ADR: CLI 动词优先混合路由架构

| 项 | 内容 |
|---|---|
| 决策文件名 | `0059-wink-tools-cli-hybrid-verb-first-architecture.md` |
| 归属系统 | `wink-tools` |
| 状态 | **Accepted** |

> 💡 **架构说明**：
> 本架构决策界定外部系统与嵌入式运行时的技术边界，具体实施方案由各子系统遵循标准 C-ABI 与接口规范对接落地。
---

## 结论摘要 (Decision Summary)

本架构决策已被正式批准并在 `wink-tools` 中固化实施。嵌入式 C 运行时（WinkMicroOS）遵照该决策对外暴露的标准契约执行，二者保持解耦。
