---
paths:
  - "**/*.md"
---

# Markdown Documentation & Architecture Decision Record (ADR) Standards

Guidelines and rules for writing, refactoring, and maintaining documentation and architecture decision records.

## 1. Document Categories and Placement

Our design documentation is organized into **a five-layer hierarchical system** under `docs/design/` as defined in [CLAUDE.md](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/CLAUDE.md). Each layer has a distinct purpose, audience, and lifecycle.

### 1.1 Five-Layer Documentation System

| Layer | Directory | Purpose | Lifecycle | When to Create |
|-------|-----------|---------|-----------|----------------|
| **① Design Specifications** | `01-system-overall/`, `02-wink-micro-os/`, `03-app-codegen/`, `04-wasm-simulation/`, `05-frontend-workbench/`, `06-build-toolchain/`, `07-platform-governance/` | **Living specifications** — the single source of truth for system architecture. Represents the current, actual system design. Organized by module/domain. | Continuously updated | At project inception; updated when design changes are finalized |
| **② Technical Design Specifications** | `tech-designs/` | Detailed component-level design: architecture diagrams, API layouts, compatibility matrices, option comparisons with rationale. | Stable (archived after implementation) | When implementing a non-trivial feature that requires design decisions beyond just task breakdown |
| **③ Implementation Plans** | `implementation-plans/` | **Executable task plans**. Datestamped, containing task breakdown, timelines, acceptance criteria, risk tracking, and verification gates. Naming: `YYYY-MM-DD-[feature-name]-plan.md` | One-time (archived after completion) | Before starting any non-trivial implementation that requires coordination or has multiple steps |
| **④ Review Records** | `reviews/` | Point-in-time snapshots of code/architecture audits. Naming: `YYYY-MM-DD-[review-topic]-review.md` | Read-only — never edit after finalization | After any formal architecture or code review |
| **⑤ Architecture Decision Records (ADRs)** | `decisions/` | Major design decisions with context, alternatives considered, and consequences. Sequentially numbered (four digits: `0001-xxx.md`). | Read-only after Accepted — must backport decisions to Design Specifications | When facing a significant architectural choice with lasting impact |

### 1.2 Documentation Flow Rules

```
┌─────────────────┐
│  Technical Issue│
└────────┬────────┘
         │
         ▼
┌─────────────────┐     Accepted      ┌─────────────────────┐
│  Write ADR      │ ────────────────► │ Update Layer ①      │
└────────┬────────┘                   │ Design Specs        │
         │                            └─────────────────────┘
         ▼
┌─────────────────┐
│  Needs code?    │─┐
└────────┬────────┘ │
         │ Yes      │
         ▼          │ No
┌─────────────────┐ │
│  Write Layer ②  │ │
│  Tech Design    │ │
└────────┬────────┘ │
         │          │
         ▼          │
┌─────────────────┐ │
│  Write Layer ③  │ │
│  Implementation  │ │
│  Plan            │ │
└────────┬────────┘ │
         │          │
         ▼          │
┌─────────────────┐ │
│  Execute &      │ │
│  Verify          │ │
└────────┬────────┘ │
         │          │
         ▼          │
┌─────────────────┐ │
│  Write Layer ④  │ │
│  Review Record   │◄┘
└─────────────────┘
```

### 1.3 Cross-Referencing Convention

Always include these links:
- **Implementation Plans (③)** link to **Technical Designs (②)** and vice versa
- **ADRs (⑤)** in `Accepted` state **MUST** link to the updated **Design Specifications (①)**
- All documents should reference related ADRs when applicable

Example document header with cross-references:
```markdown
# ESP-IDF v6.x I2C Compatibility Design

| 项 | 内容 |
|----|------|
| 创建日期 | 2026-06-27 |
| 关联 ADR | (待定) |
| 关联实施计划 | `implementation-plans/2026-06-27-esp-idf-v6-i2c-compat-plan.md` |
| 关联设计规范 | `02-wink-micro-os/02-pal-platform-abstraction.md` |
```

## 2. Decision Backporting (Single Source of Truth)

**Critical Convention:** Whenever an ADR transitions to **Accepted**, the decision details, API layouts, and specifications **MUST** be backported and updated in the corresponding active `01~07` design specifications immediately. An ADR is a history log; the active specs must always represent the latest system design.

## 3. ADR Structure and Lifecycle Logs

All ADRs must adhere to the following structure:
- A header block with a metadata table:
  ```markdown
  # ADR-XXXX：[Title in Chinese]

  | 项 | 内容 |
  |---|---|
  | 状态 | **[Proposed（提议中） / Accepted（已采纳） / Rejected（已拒绝）]** |
  | 日期 | YYYY-MM-DD |
  | 触发 | [Reason / Reference review report] |
  | 影响范围 | [Impacted layers] |
  | 决策者 | [Decision makers] |
  ```
- Sections:
  - **背景（Context）**: Why the decision is needed, problems with current designs.
  - **方案比选（Options）**: Alternatives evaluated with pros/cons.
  - **决策结论（Decision）**: The recommended option and justifications.
  - **后果与约束（Consequences & Constraints）**: Side effects, migration efforts, and code generation guidelines.
  - **遵循与后续（Compliance & Follow-up）**: Action items for backporting.
- **Status Change Log (底部状态变更日志)**: Place this at the very bottom of the document to record transitions:
  ```markdown
  ---

  *本 ADR 状态变更请在此记录：*
  - YYYY-MM-DD：Proposed（[Details]）
  - YYYY-MM-DD：Accepted（[Details / Decision maker]）
  ```

## 4. Checking ADR Statuses

To manage and inspect ADR statuses, run the helper script:
- **Default (List proposed/pending decisions)**:
  ```bash
  python docs/design/decisions/scripts/list_adrs.py
  ```
- **List all ADRs (Overview table)**:
  ```bash
  python docs/design/decisions/scripts/list_adrs.py -a
  ```
- **Filter by specific status** (e.g., `Accepted`):
  ```bash
  python docs/design/decisions/scripts/list_adrs.py -s Accepted
  ```

*On Windows, you can also double-click [list_adrs.bat](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/design/decisions/scripts/list_adrs.bat) in file explorer to quickly check pending decisions.*



