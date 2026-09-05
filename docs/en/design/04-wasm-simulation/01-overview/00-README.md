# Ⅰ Overview (overview)

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/01-overview/00-README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Layer | Ⅰ Concepts / Architecture / Methodology / Production Scope / Lexicon |
| Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| Responsibility | Defines "what simulation is, what it promises, how to read it, and its terminology"; **excludes** engine implementation details and scenario status matrices |
| Last Audit | 2026-08-02 |

---

## Directory Index

| File | Responsibility | Migrated From 2.0 | Status |
|---|---|---|---|
| [01-architecture.md](./01-architecture.md) | Layered architecture, 3-domain co-simulation, code map | `01-architecture.md` | Migrated |
| [02-axes-af.md](./02-axes-af.md) | **Unique SSOT definitions for Orthogonal Axes A~F** | `00-README.md` §1 | Migrated |
| [03-production-contract.md](./03-production-contract.md) | Completeness vs identity, residual divergence, testability boundaries | `00` §2–§3; `11` §0.5 | Migrated |
| [04-methodology.md](./04-methodology.md) | Reading paths, solution taxonomy, bypass discipline, STRICT rationale | `00` §0/§4 | Migrated |
| [05-glossary.md](./05-glossary.md) | **Authoritative Terminology Glossary** | Extracted | Migrated |

---

## SSOT Rules

- Axes A~F letter definitions: Defined **strictly** in `02-axes-af.md`.
- Production contract scope: Defined **strictly** in `03-production-contract.md`.
- Terminology glossary: Defined **strictly** in `05-glossary.md`.
- STRICT non-blocking: Methodology captures discipline; `02-mechanisms/01` captures build mechanics with bidirectional links.
- Maturity vocabulary: Defined **strictly** in root [`00-README.md` §3.2](../00-README.md).

---

## Wave 1 Self-Audit (Corresponding to Root §7.1)

- [x] `02-axes-af.md` Axes A~F definition table completed
- [x] `03-production-contract.md` Production contract scope finalized
- [x] `05-glossary.md` All targeted entries defined
- [x] `04-methodology.md` STRICT bidirectional links in place
- [ ] Repository-wide link audit completed
