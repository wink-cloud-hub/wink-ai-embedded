# Core Domain Technical Proposals (Core Tech-Designs RFC)

<!-- i18n-meta
source: docs/zh/tech-designs/core/README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

This directory archives technical proposals and RFCs for the `wink-micro-os` C kernel layers (PAL/DAL/BAL), peripheral drivers, and hardware platform bindings.

---

## 📜 Core Proposals List

| RFC Document | Scope & Focus | Related ADR |
| :--- | :--- | :--- |
| [2026-06-29-dal-peripheral-abstraction-refactoring-proposal.md](./2026-06-29-dal-peripheral-abstraction-refactoring-proposal.md) | DAL Standardization & `config_t` Pattern Refactoring | ADR-0003, ADR-0004 |
| [2026-06-29-wink-micro-os-directory-reorganization-proposal.md](./2026-06-29-wink-micro-os-directory-reorganization-proposal.md) | Ports & Adapters Directory Topology Reorganization | ADR-0021 |
| [2026-07-06-bal-dcst-architecture-refactor.md](./2026-07-06-bal-dcst-architecture-refactor.md) | BAL Layer DCST Architectural Refactoring | ADR-0023, ADR-0037 |
| [2026-07-14-button-event-drive-backends.md](./2026-07-14-button-event-drive-backends.md) | Button Event-driven Engine & Debouncing Backends | ADR-0031 |
| [2026-07-16-dal-progressive-config-disclosure.md](./2026-07-16-dal-progressive-config-disclosure.md) | DAL Progressive Configuration Disclosure & Default Rules | ADR-0034 |
| [2026-07-16-ultrasonic-distance-events.md](./2026-07-16-ultrasonic-distance-events.md) | Ultrasonic Distance Event Stream & Filter Models | ADR-0033 |
| [2026-07-18-dal-dual-mode-auto-pruning.md](./2026-07-18-dal-dual-mode-auto-pruning.md) | DAL Dual-Mode Auto-Pruning & Static Resource Guards | ADR-0036 |
| [2026-07-30-dal-variant-unified-field-design.md](./2026-07-30-dal-variant-unified-field-design.md) | DAL Multi-Variant Unified Struct Field Design | ADR-0046 |
| [flash-override-versioning-fault-tolerance.md](./flash-override-versioning-fault-tolerance.md) | Flash Override Versioning & Fault-Tolerance Protocol | ADR-0044 |
| [pal-i2c-v6-compatibility.md](./pal-i2c-v6-compatibility.md) | PAL I2C ESP-IDF v6.x Compatibility Migration | ADR-0026 |
| [pal-unified-interrupt-subsystem.md](./pal-unified-interrupt-subsystem.md) | PAL Unified Interrupt Subsystem & ISR Context Guards | ADR-0018 |
| [phase-c-inherited-debt.md](./phase-c-inherited-debt.md) | Phase C Technical Debt Assessment & Cleanup Plan | — |
