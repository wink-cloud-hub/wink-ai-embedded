# Ⅰ 宏观整体（overview）

| 项 | 内容 |
|---|---|
| 层 | Ⅰ 概念 / 架构 / 方法论 / 生产口径 / 术语 |
| 文档状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| 职责 | 回答「仿真是什么、承诺什么、怎么读、词是什么」；**不写**引擎实现细节与场景状态矩阵 |
| 上次核对 | 2026-08-02 |

## 本目录文件

| 文件 | 职责 | 迁自 2.0 | 状态 |
|---|---|---|---|
| [01-architecture.md](./01-architecture.md) | 分层架构、联合仿真三域、代码地图 | `01-architecture.md` | 已迁 |
| [02-axes-af.md](./02-axes-af.md) | **A~F 正交轴定义唯一出处**（上限缩略） | `00-README.md` §1 | 已迁 |
| [03-production-contract.md](./03-production-contract.md) | 完备≠恒等、残余不一致、可验/不可验边界 | `00` §2–§3；`11` §0.5 | 已迁 |
| [04-methodology.md](./04-methodology.md) | 阅读路径、解法类型、旁路纪律、STRICT「为什么」 | `00` §0/§4 | 已迁 |
| [05-glossary.md](./05-glossary.md) | **术语权威释义** | 新建（自 2.0/代码核对） | 已迁 |

## SSOT

- A~F 字母含义：**只**在 `02-axes-af.md` 定义；轴页不得改写措辞。
- 生产口径措辞：**只**在 `03-production-contract.md` 定稿。
- 术语释义：**只**在 `05-glossary.md` 定稿；薄轴页依赖本表，禁止在轴页塞近义定义。
- STRICT：methodology 写纪律；`02-mechanisms/01`（及 scheduler）写构建落地，双向链接（mechanisms 正文仍待迁，stub 已留链）。
- 成熟度词表：**只**在根 [`00-README.md` §3.2](../00-README.md)；本目录不重定义。

## Wave 1 自检（对应根 §7.1，mechanisms 未齐前不切入口）

- [x] `02-axes-af.md` A~F 定义表已填
- [x] `03-production-contract.md` 生产口径已定稿
- [x] `05-glossary.md` 待收词目均有定义（非 TODO）
- [x] `04-methodology.md` ↔ sandbox/scheduler STRICT 双向链接已就位（mechanisms 侧为 stub 回链）
- [ ] 全库无死链 / 切入口 — 待后续波次与 §7 全项
