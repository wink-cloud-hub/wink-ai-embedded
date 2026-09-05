# 🛠️ Documentation Governance & i18n Automation Tooling

<!-- i18n-meta
source: docs/scripts/README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

English | [简体中文](README.md)

This directory contains the automation tooling suite for **Wink-AI / WinkMicroOS** documentation governance, black-box architectural contract enforcement, ADR back-write synchronization tracking, and bidirectional i18n directory tree & content structural skeleton alignment verifications.

---

## 📋 Tooling Matrix & Capabilities

| Script | Primary Responsibility | Recommended Trigger | CI Gate |
|---|---|---|:---:|
| **[`run_all_checks.py`](run_all_checks.py)** | **One-click suite runner** (orchestrates all verification tools below) | Pre-PR / CI Pipeline | 🚨 Blocker |
| **[`verify_i18n_alignment.py`](verify_i18n_alignment.py)** | **1:1 Directory tree symmetry + Markdown structural skeleton matching** | Doc updates / Translation | 🚨 Blocker |
| **[`lint_i18n_glossary.py`](lint_i18n_glossary.py)** | **Terminology Linter** (enforces `docs/i18n/glossary.yaml` forbidden terms) | Translation / Pre-PR | 🚨 Blocker |
| **[`check_i18n_sync.py`](check_i18n_sync.py)** | **Translation coverage & `i18n-meta` header analysis** | Milestone reviews | ℹ️ Report |
| **[`check_ssot_sync.py`](check_ssot_sync.py)** | **ADR SSOT back-write completion inspector** | Architecture reviews | ⚠️ Warning |
| **[`verify_doc_contracts.py`](verify_doc_contracts.py)** | **Black-box architectural insulation verification** | Core design changes | 🚨 Blocker |
| **[`doc_link_governance.py`](doc_link_governance.py)** | **Global Markdown relative link & anchor health checker** | Refactoring / Path moves | 🚨 Blocker |

---

## 🚀 CLI Usage & Common Workflows

### 1. Run Complete Quality Suite
```bash
# Standard run (tree symmetry, black-box contracts, glossary linter, link health)
python docs/scripts/run_all_checks.py

# Deep run (enables Markdown structural skeleton verification)
python docs/scripts/run_all_checks.py --check-content
```

### 2. Multi-Language & Migration Alignment Verifier (`verify_i18n_alignment.py`)
```bash
# 1) Check 1:1 bidirectional tree symmetry between docs/zh/ and docs/en/
python docs/scripts/verify_i18n_alignment.py

# 2) Enable content structural skeleton check (H1/H2/H3 levels, code blocks, tables, Mermaid diagrams)
python docs/scripts/verify_i18n_alignment.py --check-content

# 3) Migration parity check: Verify docs/design <-> docs/zh/design for 100% complete migration
python docs/scripts/verify_i18n_alignment.py --source-dir docs/design --target-dir docs/zh/design --check-content

# 4) Technical designs migration check: docs/tech-designs <-> docs/zh/tech-designs
python docs/scripts/verify_i18n_alignment.py --source-dir docs/tech-designs --target-dir docs/zh/tech-designs --check-content

# 5) CI strict mode (fails if tree or content skeleton diverges)
python docs/scripts/verify_i18n_alignment.py --strict --strict-content --min-score 80.0
```

### 3. Terminology Linter (`lint_i18n_glossary.py`)
```bash
# Scans all English docs to block forbidden machine-translated terms (e.g., "Friend", "Dali")
python docs/scripts/lint_i18n_glossary.py
```

### 4. Global Link Health Check (`doc_link_governance.py`)
```bash
# Validates all relative links and file references across the repository
python docs/scripts/doc_link_governance.py
```

---

## 🔒 CI/CD Workflow Integration

Recommended GitHub Actions workflow step:

```yaml
- name: Run Documentation & i18n Quality Gates
  run: |
    python docs/scripts/run_all_checks.py --check-content --strict
```
