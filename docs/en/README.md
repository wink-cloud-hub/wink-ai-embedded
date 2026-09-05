# Wink-AI Embedded Platform Documentation Hub (English)

> 🌐 **Language / 语言**: [📖 Global Hub](../README.md) | [🇨🇳 简体中文](../zh/README.md) | **🇺🇸 English**

<!-- i18n-meta
source: docs/zh/README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

Welcome to the **Wink-AI Embedded Development & Simulation System (WinkMicroOS)** Documentation Hub.

---

## 🏛️ System Design Specifications (Design SSOT)

| Module | Scope & Responsibilities | Link |
| :--- | :--- | :--- |
| **00. Quick Start** | 5-minute Getting Started & onboarding guide | [00-quick-start/](./design/00-quick-start/01-5min-getting-started.md) |
| **01. System Overview** | Overall architecture, cross-repo contracts, and MVP roadmap | [01-system-overall/](./design/01-system-overall/01-system-overview.md) |
| **02. WinkMicroOS Kernel** | C SDK layered architecture (PAL/DAL/BAL) & runtime specifications | [02-wink-micro-os/](./design/02-wink-micro-os/README.md) |
| **03. App & Safe Codegen** | App business logic model, Manifest Schema, and Safe Codegen pipeline | [03-app-codegen/](./design/03-app-codegen/01-app-business-logic.md) |
| **04. Wasm Simulation** | UniSim simulation engine, Wasm-JS Bridge, and behavioral fidelity | [04-wasm-simulation/](./design/04-wasm-simulation/00-README.md) |
| **05. Frontend Workbench** | Dual-viewport architecture, Canvas & 3D state synchronization | [05-frontend-workbench/](./design/05-frontend-workbench/01-frontend-workbench-architecture.md) |
| **06. Build Toolchain** | Isolated cloud compilation service & Job Protocol | [06-build-toolchain/](./design/06-build-toolchain/01-toolchain-deployment.md) |
| **07. Governance & Registry** | Device Model Registry, fault injection model, and coding conventions | [07-platform-governance/](./design/07-platform-governance/01-device-model-registry.md) |

---

## 💡 Core Technical Designs (RFCs)
- [C Kernel Tech Designs](./tech-designs/core/README.md)
- [MCS-51 Simulation Interception Tech Designs](./tech-designs/mcs51/README.md)
- [Toolchain Tech Designs](./tech-designs/tools/README.md)
- [Frontend Workbench Designs](./tech-designs/frontend/README.md)
- [UniSim Simulation Designs](./tech-designs/unisim/README.md)

---

## 📊 Product & Market Research
- [UniSim Embedded Online Simulation Platform Commercial & Market Analysis](./product/market-analysis.md)
