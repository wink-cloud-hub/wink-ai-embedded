# Virtual Peripheral Registry and Pin Arbitration

| Field | Content |
|---|---|
| Tier | IIa Core Mechanism Specification (Public Architecture Specification) |
| Status | **Active** (Open Source Specification; Aligned with UniSim 3.0 SSOT) |
| C-ABI SSOT | [`wink-micro-os/targets/wasm/wasm_bridge.h`](../../../../../wink-micro-os/targets/wasm/wasm_bridge.h) |

> 💡 **Architectural Contract & Scope**:
> - This specification serves as the public architectural standard and functional contract;
> - The C runtime (WinkMicroOS) decouples from the simulation environment via standard ABI headers; high-fidelity simulation scheduling engines and calibrated device models are delivered via proprietary UniSim editions.
---

## 1. Mechanism Overview

Specification of peripheral lifecycle, power domains, and 4-value logic (0/1/Z/X) pin arbitration.

---

## 2. Architecture Principles and Boundaries

1. **Determinism and Reproducibility**: Guarantees bit-exact event order reproduction under identical microsecond virtual clock ticks and PRNG seeds.
2. **C/Wasm Contract Decoupling**: Firmware C code exposes behavior through `pal_wasm_*` and `wasm_bridge.h` without business logic alteration.
3. **Layered Isolation**: Adheres to the fidelity constraints defined in [01-overview](../01-overview/01-architecture.md) and [03-axes](../03-axes/00-README.md).

For verification matrices and consistency specifications, see [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md).
