# 🛠️ 文档治理与多语言自动化工具库 (Documentation & i18n Tooling)

[English Version](README.en.md) | 简体中文

本目录包含 **Wink-AI / WinkMicroOS** 平台的全局文档治理、黑盒契约校验、ADR 回写状态追踪、多语言（i18n）目录树与内容骨架对齐校验等全套自动化工具。

---

## 📋 工具矩阵与功能清单

| 脚本名称 | 核心职责 | 推荐执行时机 | 阻断级别 (CI) |
|---|---|---|:---:|
| **[`run_all_checks.py`](run_all_checks.py)** | **一键执行全量检查套件**（串联调度下述所有工具） | PR 提交前 / CI Pipeline | 🚨 阻断 |
| **[`verify_i18n_alignment.py`](verify_i18n_alignment.py)** | **中英文目录树 1:1 双向对称校验 + 内容结构骨架对齐** | 修改文档 / i18n 同步 | 🚨 阻断 |
| **[`lint_i18n_glossary.py`](lint_i18n_glossary.py)** | **术语合规性检查**（依据 `docs/i18n/glossary.yaml` 拦截禁用词） | 翻译文档 / PR 提交 | 🚨 阻断 |
| **[`check_i18n_sync.py`](check_i18n_sync.py)** | **双语翻译覆盖率与 `i18n-meta` 状态分析** | 阶段性审查 / 统计进度 | ℹ️ 报告 |
| **[`check_ssot_sync.py`](check_ssot_sync.py)** | **ADR 决策回写 SSOT 状态检查** | ADR 结项 / 架构评审 | ⚠️ 警告 |
| **[`verify_doc_contracts.py`](verify_doc_contracts.py)** | **文档黑盒隔离契约校验**（禁止未声明的跨层引用） | 修改核心设计文档 | 🚨 阻断 |
| **[`doc_link_governance.py`](doc_link_governance.py)** | **全局 Markdown 超链接与锚点有效性检查** | 文档重构 / 路径迁移 | 🚨 阻断 |

---

## 🚀 常用运行指令

### 1. 一键运行全量治理套件
```bash
# 基础运行（执行目录树对齐、黑盒契约、术语规范、超链接健康度）
python docs/scripts/run_all_checks.py

# 深度运行（开启 Markdown 内容结构骨架校验）
python docs/scripts/run_all_checks.py --check-content
```

### 2. 多语言对齐与迁移一致性校验 (`verify_i18n_alignment.py`)
```bash
# 1) 检查 docs/zh/ 与 docs/en/ 目录树是否 1:1 对称
python docs/scripts/verify_i18n_alignment.py

# 2) 开启内容结构骨架校验（比对标题层级 H1/H2/H3、代码块、表格、Mermaid 架构图等）
python docs/scripts/verify_i18n_alignment.py --check-content

# 3) 迁移完整性对齐比对：对比旧 docs/design 与新 docs/zh/design 是否 100% 完整无损迁移
python docs/scripts/verify_i18n_alignment.py --source-dir docs/design --target-dir docs/zh/design --check-content

# 4) 对比技术设计目录：docs/tech-designs 与 docs/zh/tech-designs
python docs/scripts/verify_i18n_alignment.py --source-dir docs/tech-designs --target-dir docs/zh/tech-designs --check-content

# 5) CI 严格模式（目录树不对称或结构得分过低时返回非零退出码）
python docs/scripts/verify_i18n_alignment.py --strict --strict-content --min-score 80.0
```

### 3. 术语合规性检查 (`lint_i18n_glossary.py`)
```bash
# 扫描所有英文文档，拦截机器直译与禁用词（如 "Friend", "Dali", "Universal"）
python docs/scripts/lint_i18n_glossary.py
```

### 4. 全局超链接健康度检查 (`doc_link_governance.py`)
```bash
# 检查全仓所有 Markdown 文件的相对路径链接是否有效
python docs/scripts/doc_link_governance.py
```

---

## 🔒 CI/CD 集成规范

在 GitHub Actions 或本地 pre-commit 钩子中，推荐配置以下步骤：

```yaml
- name: Run Documentation & i18n Quality Gates
  run: |
    python docs/scripts/run_all_checks.py --check-content --strict
```
