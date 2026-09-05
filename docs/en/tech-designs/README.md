# Wink-AI Technical Design Proposals & RFC Hub (Tech-Designs)

<!-- i18n-meta
source: docs/zh/tech-designs/README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

This directory archives technical proposals (RFCs) and detailed technical designs across the 4 orthogonal domains (`core`, `tools`, `frontend`, `unisim`).

---

## 📂 Domain Indexes

| Domain | Scope & Proposal Areas | Index Entry |
| :--- | :--- | :--- |
| **`core`** | C Kernel architecture (PAL/DAL/BAL), driver refactoring, unified interrupt subsystem | [core/](core/README.md) |
| **`mcs51`** | MCS-51/8051 zero-code simulation interception layer, SFR proxies, dual clock domains | [mcs51/](mcs51/README.md) |
| **`tools`** | CLI toolchain architecture, SDK packaging, Scannable Codegen, Layer Lint rules | [tools/](tools/README.md) |
| **`frontend`** | Workbench HCTR wire-routing, synthetic netlists, Phase B simulation UX | [frontend/](frontend/README.md) |
| **`unisim`** | Wasm simulation observation layers, concurrency stress tests, co-simulation plugins | [unisim/](unisim/README.md) |

---

## 🔄 Lifecycle & SSOT Back-Write Contract
1. **RFC Approval**: Key architectural decisions are recorded into ADRs in `docs/decisions/`.
2. **SSOT Back-Write**: Stable architectural findings must be back-written to `docs/{zh,en}/design/`.
