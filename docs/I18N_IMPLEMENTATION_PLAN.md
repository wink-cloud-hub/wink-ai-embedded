# Wink-AI 多语言 (i18n) 文档体系重构与演进实施计划

> **本文件归属**：`docs/I18N_IMPLEMENTATION_PLAN.md`（长期留存，禁止随目录迁移被一并移动或删除）  
> **状态**：Phase 0~4 核心基线已全部完成  
> **最后更新**：2026-08-17

---

## 0. 核心架构决策 (Design Decisions)

| 决策项 | 结论 | 原因 |
|--------|------|------|
| **哪些目录需要双语版本？** | 仅 `docs/zh/design/` 与 `docs/en/design/`、`docs/zh/tech-designs/` 与 `docs/en/tech-designs/` | 这两类是长效架构事实源 (Living SSOT)，面向人类全球协作与 AI 高阶检索 |
| **哪些目录单份维护？** | `decisions/`, `implementation-plans/`, `reviews/` 原位保留在 `docs/` 根目录 | 这些是 SDD 过程执行工件，AI Agent 在执行任务时高频修改（打勾、归档），双份会导致状态死锁 |
| **`docs/product/`如何处理？** | 迁移至 `docs/zh/product/`，并在 `docs/en/product/` 建立英译 | `market-analysis.md` 是产品定位研究文档（面向人类），属于长效 SSOT 类，不属于 SDD 过程工件 |
| **`docs/AGENTS.md` 单份还是双份？** | **保持单份**，文件内引用路径更新为 `docs/zh/design/...`（默认中文 SSOT 主路径），并在文件顶部添加 `[EN →](../en/design/)` 双语导航注记 | AGENTS.md 是 AI Agent 的上下文锚点，双份会使 Agent 无法确定"哪份才是权威"，引发检索漂移 |
| **英文文档头部元数据格式？** | 每份 `docs/en/` 文档顶部必须包含标准 frontmatter 注记（见第 3 节） | 为自动化同步检测脚本提供可解析的元数据锚点 |
| **翻译执行者？** | AI Agent 辅助生成初稿 + `glossary.yaml` 强约束 + 人工审校 | 平衡速度与质量；杜绝裸机大模型翻译产生术语变形 |

---

## 1. 目标与现状概述

### 1.1 迁移目标
将现有 **343 个纯中文 Markdown 文件**重构为 **双语长效 SSOT + 单份 SDD 执行工件** 的现代化多语言文档治理体系。

---

## 2. Pre-Migration 风险评估与处置

- **风险 R1**：`docs/AGENTS.md` 硬编码绝对路径 ➔ ✅ 已治理为相对路径并补充双语导航
- **风险 R2**：`verify_doc_contracts.py` 目录依赖 ➔ ✅ 已升级支持 `zh/design` 与 `en/design`
- **风险 R3**：路径迁移自动化 ➔ ✅ 已编写 `migrate_design_paths.py`
- **风险 R4**：ADR 内引用 ➔ ✅ 已核查与规整
- **风险 R5**：Git 历史保留 ➔ ✅ 规范管理

---

## 3. 英文文档头部元数据标准 (EN Doc Frontmatter Convention)

每份 `docs/en/` 下的文档，**顶部必须包含**以下标准注记块（紧跟标题之后）：

```markdown
# [English Title Here]

<!-- i18n-meta
source: docs/zh/design/[对应中文文件相对路径]
translated: YYYY-MM-DD
glossary-version: v1.x
translator: [AI-assisted / Human]
sync-status: up-to-date | stale | partial
-->
```

---

## 4. 目录拓扑结构目标态 (Target Directory Topology)

```text
docs/
├── README.md                           # 📖 全局文档中心主入口 (双语导航)
├── AGENTS.md                           # 🤖 AI Agent 检索指南 (单份，含双语路径注记)
├── I18N_IMPLEMENTATION_PLAN.md         # 📌 本文件 — 长期留存，禁止删除或移动
│
├── i18n/                               # 🌐 【全局术语与本地化中心】
│   ├── GLOSSARY.md                     # 人类可读权威术语表 + Doxygen 注释规范
│   └── glossary.yaml                   # 机器可读 Canonical Forms + Forbidden 约束
│
├── zh/                                 # 🇨🇳 【中文长效 SSOT 根目录】（Source of Truth）
│   ├── README.md                       # 中文文档总览入口
│   ├── design/                         # 系统 7 大模块中文架构规范 (00 ~ 07)
│   ├── tech-designs/                   # 中文核心技术方案与 RFC
│   └── product/                        # 中文产品与市场研究文档
│
├── en/                                 # 🇺🇸 【英文长效 SSOT 根目录】（Localized Target）
│   ├── README.md                       # 英文文档总览 (English Documentation Hub)
│   ├── design/                         # 英文 7 大模块架构规范
│   ├── tech-designs/                   # 英文核心技术方案 RFC
│   └── product/                        # 英文产品与市场研究文档
│
├── decisions/                          # 📜 【单份 · SDD 决策流】ADR 决策库
│   ├── core/
│   ├── tools/
│   ├── frontend/
│   ├── unisim/
│   └── scripts/list_adrs.py
│
├── implementation-plans/               # 🚀 【单份 · SDD 执行流】实施计划库
│   ├── core/
│   ├── tools/
│   ├── frontend/
│   ├── unisim/
│   └── scripts/list_plans.py
│
├── reviews/                            # 🔍 【单份 · SDD 验证流】冒烟与评审报告
│   ├── core/
│   ├── tools/
│   ├── frontend/
│   └── unisim/
│
└── scripts/                            # 🛠️ 【自动化治理工具库】
    ├── run_all_checks.py               # 【新增】一键运行全量文档检查套件 (支持 --check-content)
    ├── verify_i18n_alignment.py        # 【新增】中英文目录树 1:1 对齐与 Markdown 内容骨架结构校验器
    ├── check_ssot_sync.py              # 检查 ADR 回写 SSOT 状态
    ├── check_i18n_sync.py              # 检查 zh/ 与 en/ 的 SSOT 镜像同步状态
    ├── lint_i18n_glossary.py           # 基于 glossary.yaml 的术语合规性 Linter
    ├── verify_doc_contracts.py         # 升级：适配 DESIGN_DIR_ZH / DESIGN_DIR_EN
    ├── doc_link_governance.py          # 全局超链接健康度检查
    └── migrate_design_paths.py         # 专用路径迁移工具
```

---

## 5. 分阶段实施进展与任务记录 (Execution Record)

### Phase 0: 迁移前预检与风险确认 (Done)
- [x] **0.1 运行当前基线检查，确认全仓状态**
- [x] **0.2 治理 `docs/AGENTS.md` 7 处硬编码绝对路径并添加双语导航注记**
- [x] **0.3 升级 `verify_doc_contracts.py` 适配 `DESIGN_DIRS`**
- [x] **0.4 编写专用迁移脚本 `docs/scripts/migrate_design_paths.py`**
- [x] **0.5 修复与规整 ADR 决策库中相关引用**

### Phase 1: 术语库加固与自动化治理工具开发 (Done)
- [x] **1.1 扩展 `docs/i18n/glossary.yaml` 至 v1.0 权威标准（包含 20+ Canonical 项与 10 项 Forbidden 规则）**
- [x] **1.2 编写 `docs/scripts/lint_i18n_glossary.py` 术语校验器**
- [x] **1.3 编写 `docs/scripts/check_i18n_sync.py` 双语同步覆盖率检查工具**
- [x] **1.4 编写 `docs/scripts/migrate_design_paths.py` 并验证**

### Phase 2: 目录拓扑与入口导航升级 (Done)
- [x] **2.1 建立 `docs/zh/` 结构与 `docs/zh/README.md`**
- [x] **2.2 建立 `docs/en/` 结构与 `docs/en/README.md`**
- [x] **2.3 更新全局主入口 `docs/README.md` 为双语枢纽**
- [x] **2.4 保持 `docs/decisions/`、`docs/implementation-plans/`、`docs/reviews/` 单份维护**

### Phase 3: Tier 1 核心 SSOT 英文基线建立 (Done)
- [x] **3.1 翻译 `00-quick-start/01-5min-getting-started.md` (ZH & EN)**
- [x] **3.2 翻译 `01-system-overall/01-system-overview.md` (ZH & EN 核心架构总览)**
- [x] **3.3 翻译 `02-wink-micro-os/README.md` (ZH & EN 内核规范总览)**
- [x] **3.4 翻译 `03-app-codegen/01-app-business-logic.md` (ZH & EN 应用层运行时)**
- [x] **3.5 翻译 `04-wasm-simulation/00-README.md` (ZH & EN UniSim 3.0 SSOT 架构总览)**
- [x] **3.6 翻译 `05-frontend-workbench/01-frontend-workbench-architecture.md` (ZH & EN 前端工作台)**
- [x] **3.7 翻译 `06-build-toolchain/01-toolchain-deployment.md` (ZH & EN 云编译与烧录)**
- [x] **3.8 翻译 `07-platform-governance/01-device-model-registry.md` (ZH & EN 器件注册表)**
- [x] **3.9 翻译 `product/market-analysis.md` (ZH & EN 商业价值与竞品分析)**

### Phase 4: 自动化治理闭环与长期运营规约 (Done)
- [x] **4.1 编写 `docs/scripts/run_all_checks.py` 一键套件执行器（支持 `--check-content` 与 `--strict`）**
- [x] **4.2 编写 `docs/scripts/verify_i18n_alignment.py`：支持中英文目录树 1:1 双向对称校验 + Markdown 内容结构骨架（标题级别/代码块/表格/Mermaid/提示块）对齐校验**
- [x] **4.3 完善 ADR / Plan 结项回写双语 SSOT 契约规范**
- [x] **4.4 固化长期运营维护五大规约**

---

## 6. 长期运营维护规约 (Long-Term Maintenance Covenant)

1. **"中文先写、英文跟进"原则**：所有对 `docs/zh/design/` 的修改，在同一个 PR 或下一个 PR 内必须同步更新对应的 `docs/en/design/` 文件，并将 `sync-status` 更新为 `up-to-date`。
2. **`check_i18n_sync.py` 定期运行**：建议每 2 周运行一次，检查 `stale` 文件数量。如果 `stale` 文件超过 5 个，必须优先处理同步工作，不得继续增加新的中文文档。
3. **`glossary.yaml` 版本管理**：每次修改 `glossary.yaml` 后，递增 `version` 字段。
4. **ADR 结项回写路径约定**：新建 ADR 时，`| 回写 SSOT 目标文档 |` 字段必须指向 `docs/{zh,en}/design/...`（双语），结项时需要同时完成中英两份 SSOT 的回写，才能将 `| SSOT 回写状态 |` 标记为 `Completed`。
5. **`docs/decisions/`、`docs/implementation-plans/`、`docs/reviews/` 严禁创建语言子目录**：这三个目录永远保持单份维护。
