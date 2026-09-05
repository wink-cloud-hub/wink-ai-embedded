# Ⅲ 验收与治理（assurance）

| 项 | 内容 |
|---|---|
| 层 | Ⅲ 验什么 / 能不能验 / 怎么演进 |
| 状态 | **Active**（2026-08-02 切换；Wasm 仿真现行 SSOT） |
| 职责 | 场景契约、状态矩阵、成熟度总表与维护规程；**不写**引擎实现正文 |

## 依赖方向（单向）

```text
01-consistency-spec.md      （因：原理 / 方案 / 预言 / 边界；五字段模板 + 🚫 豁免；§0.4 TOC + §1–§3；子项锚点）
        │
        │  checklist 只引用场景编号与状态，禁止反写方案正文
        ▼
02-consistency-checklist.md （果：✅/🟡/❌/🚫/— 唯一出处 + 验证入口列；102 子项）

02-mechanisms/* 文首「落地」标签
        │
        │  总表聚合；变更须双边同步
        ▼
03-roadmap-and-governance.md（成熟度总表 / Phase exit / CI / golden 治理 / 代码→文档 / ADR 回写）
```

禁止：checklist 写保障方案；roadmap 另起一套与文首冲突的成熟度说法；spec 内嵌状态矩阵。

## 本目录文件

| 文件 | 职责 | 迁自 2.0 | 本波状态 |
|---|---|---|---|
| [01-consistency-spec.md](./01-consistency-spec.md) | C1~C25；五字段 + oracle（含 Error-Code Parity）；102 子项正文 | `11` | Wave 4B ✅ + 审阅增补 |
| [02-consistency-checklist.md](./02-consistency-checklist.md) | ✅/🟡/❌/🚫/— **唯一出处** + **验证入口**（102 子项） | `12` | Wave 4C ✅ + 审阅修补 |
| [03-roadmap-and-governance.md](./03-roadmap-and-governance.md) | Phase exit、CI、成熟度总表、✅ 翻转、golden 治理、ADR 回写 | `13` | Wave 4A ✅ + 审阅修补 |
