# C 内核领域技术方案 (Core Tech-Designs RFC)

本目录归档 `wink-micro-os` C 内核抽象层 (PAL/DAL/BAL)、驱动模型与硬件适配的技术方案。

---

## 📜 核心方案列表

| 方案文档 | 核心内容 | 对应 ADR |
| :--- | :--- | :--- |
| [2026-06-29-dal-peripheral-abstraction-refactoring-proposal.md](./2026-06-29-dal-peripheral-abstraction-refactoring-proposal.md) | DAL 器件抽象层标准化与 `config_t` 重构提案 | ADR-0003, ADR-0004 |
| [2026-06-29-wink-micro-os-directory-reorganization-proposal.md](./2026-06-29-wink-micro-os-directory-reorganization-proposal.md) | WinkMicroOS Ports & Adapters 目录拓扑重组 | ADR-0021 |
| [2026-07-06-bal-dcst-architecture-refactor.md](./2026-07-06-bal-dcst-architecture-refactor.md) | BAL 业务抽象层 DCST 架构重构方案 | ADR-0023, ADR-0037 |
| [2026-07-14-button-event-drive-backends.md](./2026-07-14-button-event-drive-backends.md) | 按键事件驱动与消抖后端设计 | ADR-0031 |
| [2026-07-16-dal-progressive-config-disclosure.md](./2026-07-16-dal-progressive-config-disclosure.md) | DAL 渐进式配置披露与默认值策略 | ADR-0034 |
| [2026-07-16-ultrasonic-distance-events.md](./2026-07-16-ultrasonic-distance-events.md) | 超声波测距事件化与测距过滤模型 | ADR-0033 |
| [2026-07-18-dal-dual-mode-auto-pruning.md](./2026-07-18-dal-dual-mode-auto-pruning.md) | DAL 双模自动裁剪与资源守卫 | ADR-0036 |
| [2026-07-30-dal-variant-unified-field-design.md](./2026-07-30-dal-variant-unified-field-design.md) | DAL 多变体统一字段与结构体设计 | ADR-0046 |
| [flash-override-versioning-fault-tolerance.md](./flash-override-versioning-fault-tolerance.md) | 闪存覆盖与多版本容错设计 | ADR-0044 |
| [pal-i2c-v6-compatibility.md](./pal-i2c-v6-compatibility.md) | PAL I2C ESP-IDF v6.x 兼容性改造方案 | ADR-0026 |
| [pal-unified-interrupt-subsystem.md](./pal-unified-interrupt-subsystem.md) | PAL 统一中断子系统与 ISR 上下文守卫 | ADR-0018 |
| [phase-c-inherited-debt.md](./phase-c-inherited-debt.md) | Phase C 遗留架构技术债务梳理与清理计划 | — |
