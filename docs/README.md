# Wink-AI Embedded Platform Global Documentation Hub

> 🌐 **Language / 语言**: **Global Hub** | [🇨🇳 简体中文](./zh/README.md) | [🇺🇸 English](./en/README.md)

Welcome to the **Wink-AI Embedded Development & Simulation System (WinkMicroOS)** documentation hub. This repository adopts a modern **Bilingual Source of Truth (Bilingual SSOT) + Single-Copy SDD Process Artifacts** documentation governance model, organized across 4 orthogonal domains (`core`, `tools`, `frontend`, `unisim`).

---

## 🏛️ Documentation Topology Overview

```text
docs/
├── README.md                           # 📖 Global Documentation Hub Entrance (This file)
├── AGENTS.md                           # 🤖 AI Agent Search Guide & Black-box Insulation Rules
├── I18N_IMPLEMENTATION_PLAN.md         # 📌 Internationalization (i18n) Architecture & Roadmap
│
├── i18n/                               # 🌐 【Global Terminology & Localization Center】
│   ├── GLOSSARY.md                     # Human-readable authoritative glossary & Doxygen conventions
│   └── glossary.yaml                   # Machine-readable Canonical & Forbidden term constraints
│
├── zh/                                 # 🇨🇳 【Chinese Living SSOT Root】
│   ├── README.md                       # Chinese Documentation Hub
│   ├── design/                         # System 7-Module Architecture Specs (00 ~ 07)
│   ├── tech-designs/                   # Core Technical Proposals & RFCs (5 Domains)
│   └── product/                        # In-Depth Product & Market Research Reports
│
├── en/                                 # 🇺🇸 【English Living SSOT Root】
│   ├── README.md                       # English Documentation Overview
│   ├── design/                         # System 7-Module Architecture Specs (00 ~ 07)
│   ├── tech-designs/                   # Core Technical Proposals & RFCs (5 Domains)
│   └── product/                        # Product & Market Research Reports
│
├── decisions/                          # 📜 【Single-Copy · SDD Decision Stream】ADR Repository
│   ├── core/                           # C Kernel Decisions (PAL / DAL / BAL)
│   ├── tools/                          # CLI / Codegen / Lint Toolchain Decisions
│   ├── frontend/                       # Frontend Interaction & Device Tree SSOT Decisions
│   ├── unisim/                         # Wasm Simulation Engine & Physics Decisions
│   └── scripts/list_adrs.py            # ADR Status & Domain Scanner CLI
│
├── implementation-plans/               # 🚀 【Single-Copy · SDD Execution Stream】Plan Repository
│   ├── core/                           # C Kernel Implementation Plans (active / archived)
│   ├── tools/                          # Toolchain Implementation Plans (active / archived)
│   ├── frontend/                       # Frontend Implementation Plans (active / archived)
│   ├── unisim/                         # Simulation Engine Implementation Plans (active / archived)
│   └── scripts/list_plans.py           # Plan Status & Domain Scanner CLI
│
├── reviews/                            # 🔍 【Single-Copy · SDD Verification Stream】Review Reports
│   ├── core/                           # C Kernel Architecture Reviews & Hardware Smoke Reports
│   ├── tools/                          # Toolchain Architecture Review Reports
│   ├── frontend/                       # Frontend Reviews & Simulation Alignment Reports
│   └── unisim/                         # Simulation Engine Reviews & Deep Dive Reports
│
└── scripts/                            # 🛠️ 【Automated Governance & Quality Tooling】
    ├── run_all_checks.py               # One-click suite runner for all governance gates
    ├── verify_i18n_alignment.py        # Bidirectional 1:1 tree symmetry & AST skeleton validator
    ├── lint_i18n_glossary.py           # Terminology compliance linter (via glossary.yaml)
    ├── check_ssot_sync.py              # ADR SSOT back-write completion inspector
    ├── check_i18n_sync.py              # Bilingual coverage & i18n-meta sync analyzer
    ├── verify_doc_contracts.py         # Black-box architectural insulation validator
    └── doc_link_governance.py          # Global Markdown link health & dead link checker
```

---

## 📋 Quick Navigation & CLI Tools

### 1. Command-Line Query Tools (CLI Tools)
* **Inspect Implementation Plans (filter by domain `-d`)**:
  ```powershell
  python docs/implementation-plans/scripts/list_plans.py       # Show active plans
  python docs/implementation-plans/scripts/list_plans.py -a    # Show all plans (including archived)
  python docs/implementation-plans/scripts/list_plans.py -d unisim  # Filter by simulation domain
  ```
* **Inspect Architecture Decision Records (ADRs)**:
  ```powershell
  python docs/decisions/scripts/list_adrs.py -a
  python docs/decisions/scripts/list_adrs.py -d frontend
  ```
* **Run Full Documentation & i18n Quality Gates**:
  ```powershell
  python docs/scripts/run_all_checks.py
  ```

### 2. Documentation Governance & SSOT Back-write Contract

> 📌 **Governance Principles**:
> **Architecture Changes ➔ `design/` (SSOT)    │  Major Decisions ➔ `decisions/` (ADR)**
> **Technical Proposals ➔ `tech-designs/`      │  Completed Plans ➔ `archived/`**
> **Chinese Source ➔ `zh/design/`              │  English Target ➔ `en/design/`**

#### 🔄 SSOT Back-write Contract
To ensure Architecture Decision Records (ADRs) and Technical Designs (Tech-Designs) remain strictly aligned with `docs/{zh,en}/design/` (SSOT) over time, the following rules apply:
1. **Metadata Header**: All ADR and Tech-Design metadata tables must specify `| **SSOT Target Document** |` and `| **SSOT Sync Status** | (Pending / Completed)`.
2. **Lifecycle Closure**: When an ADR transitions to `Accepted` or an implementation plan is moved to `archived/`, the author must back-write new architecture contracts into the corresponding `docs/{zh,en}/design/00~07/` chapters and update the status to `Completed`.
3. **Automated Verification**: Run `python docs/scripts/check_ssot_sync.py` to detect any pending ADR back-write items.
