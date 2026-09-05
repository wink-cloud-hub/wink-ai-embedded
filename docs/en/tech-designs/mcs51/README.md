# MCS-51/8051 Zero-Code Simulation Interception Layer Technical Designs & RFCs (MCS-51 Tech-Designs)

<!-- i18n-meta
source: docs/zh/tech-designs/mcs51/README.md
translated: 2026-09-01
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

This directory archives technical proposals, data plane specifications, timing contracts, and prototype spike assets for the 8051/Keil C51 simulation interception layer (`frameworks/mcs51/`).

---

## 📂 Technical Design Index

| Document Name | Core Theme & Objectives | Governing ADR / Status |
| :--- | :--- | :--- |
| **[2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md](./2026-08-27-mcs51-zero-code-simulation-and-proxy-design.md)** | MCS-51 Zero-Code Simulation Interception Layer Master Plan & Proxy Architecture | [ADR-0070](../../../decisions/core/0070-mcs51-zero-code-simulation-interception-layer.md) (Accepted) |
| **[2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md](./2026-08-27-mcs51-sfr-proxy-rmw-and-edge-detection-design.md)** | SFR Proxy, Shadow Memory, RMW Red Lines & Pin Edge Detection Data Plane | [ADR-0071](../../../decisions/core/0071-sfr-proxy-rmw-edge-data-plane.md) (Accepted) |
| **[2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md](./2026-08-27-mcs51-clock-domains-and-timing-consistency-design.md)** | Dual Clock Domain Mapping, Quota Force-Yield & Catch-Up Reconciliation | [ADR-0072](../../../decisions/core/0072-dual-clock-domain-and-quota-catchup.md) (Accepted) |
| **[2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md](./2026-08-27-mcs51-user-code-compatibility-and-limitations-guide.md)** | User Keil C51 Source Compatibility Matrix, Dialect Erasure & Limitations | Design Guide (Active) |
| **[mcu-compat-plan.md](./mcu-compat-plan.md)** | Multi-Architecture MCU Compatibility Roadmap & Ecosystem Expansion | Roadmap (Accepted) |

---

## 🔬 Prototype Experiments (Spikes & Assets)

- **[spikes/S1-yield-api.md](./spikes/S1-yield-api.md)**: Spike S1 - Cooperative Fiber Yield & Quota Force-Yield Prototype
- **[spikes/S2-compiler-dialect-chain.md](./spikes/S2-compiler-dialect-chain.md)**: Spike S2 - Compiler Dialect Sanitization Chain (Keil `.c` ➔ C++17 `.cpp`)
- **[spikes/S3-codegen-thermal-contract.md](./spikes/S3-codegen-thermal-contract.md)**: Spike S3 - Board-Level Codegen & NTC Thermal Closed-Loop Contract
