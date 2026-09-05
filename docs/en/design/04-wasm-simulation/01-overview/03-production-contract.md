# Production Contract & Fidelity Boundaries

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/01-overview/03-production-contract.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Document Level | ① Design Specification (UniSim 3.0 / overview) |
| Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| SSOT Responsibility | **Official wording for "Completeness of Axes A~F ≠ Virtual-Physical Identity"**; Testability boundaries & bypass discipline |
| Governing ADRs | [0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md) |
| Migrated From | `04-wasm-simulation-2.0/00-README.md` §2–§3; `11-consistency-spec.md` §0.5 |
| Last Audit | 2026-08-02 |

> **Citation Rule**: Other documents **must never** author duplicate product commitments; link back to this document. Scenario statuses reside in [`02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md).

---

## 1. Production Scope (Completeness of Axes A~F ≠ Identity)

Implementing Axes A~F **does not equate** to "Simulation $\equiv$ Real Hardware" or "Skipping physical hardware release verification". This is the formal product commitment ([ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)).

| Claim | Status with Axes A~F Fully Landed |
|---|---|
| Simulation serves as a **high-confidence behavioral pre-check** in CI / Low-Code pipelines (Classifiable gaps, explainable green tests, 🚫 mandates physical hardware) | **Approachable** |
| Simulation results **replace** physical hardware / HIL as release sign-off criteria | **No** |
| Simulation and hardware achieve **bit- / μs-level identity** | **Never Promised** |

```text
Completeness of Axes A~F
  → Production "Behavior / Protocol / Resource Pre-check Pipeline" ✅
  → Production "Virtual-Physical Identity / Skip Hardware Release" ❌

Residual divergence is managed via checklists + HIL/Hardware gates + ADR-0003 policies, not eliminated.
```

---

## 2. Inevitable Residual Divergence (Model Upper Bounds)

| Source | Description |
|---|---|
| Electrical / Analog | ADC quantization, impedance, power integrity, oscillator drift (C11, mostly 🚫 non-goals) |
| Interrupt Model Bounds | Cooperative IRQ $\neq$ NVIC preemptive nesting (C4); default tick $\approx$ 10ms latency |
| Timer / Fast-Loop Bounds | Soft stepping / virtual clock $\neq$ on-chip hard ISRs, PWM–ADC hardware triggers (C10) |
| Same-Timestamp Total Order | [ADR-0053](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md) contract frozen; cross-source bit-exact tests Planned |
| UART Async RX | [ADR-0054](../../../decisions/unisim/0054-sim-uart-async-rx-model-boundary.md): Transactional Partial; Byte stream + RX IRQ **Planned** |
| Floating-Point / Golden | [ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md): Host↔Wasm default tolerance; fast-math checks Planned |
| Multi-Core / Microarch | Single virtual core; cache / DMA contention / silicon errata $\rightarrow$ Physical hardware (C9/C24) |
| Host Environment | Worker, Asyncify yield points alter wallclock perception; **logic timing relies on virtual clocks** |

---

## 3. Converged Consistency Domains

- Axis A + Single-Source Rule: Conversions, timeouts, packet packing, and error recovery execute identically in simulation.
- Axis B: Debouncing, timeouts, and periodic tasks execute reproducibly under `s_virtual_us`.
- Axes C/F Gates: Pin and timer conflicts, illegal blocking, and quota/fault fail-loud protections.
- Axis E Cooperative Subset: Starvation, soft WDTs, and shared-state hazards surfaced earlier.
- Axis F + [`02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md): Untestable scenarios explicitly flagged with 🚫 to prevent false security.

---

## 4. Behavioral Fidelity Boundaries

Wink-AI simulation provides **behavioral (causal) high fidelity**: guaranteeing causal ordering and logical correctness while **not guaranteeing** cycle-accurate or electrical fidelity ([ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)).

### 4.1 Verifiable Scope

When supporting axes land and scenarios are marked ✅/🟡:
- Business state machines
- Sensor/actuator semantics routed through single-source PAL paths
- I2C/UART payload-level protocols
- Timeout/disconnect exception handling
- Logical timing subsets governed by virtual clocks

### 4.2 Non-Verifiable Scope (or Weak Approximations)

- Hard real-time microsecond response times
- **Interrupt preemption and priority nesting**
- On-chip hardware timers / FOC hard ISRs
- Multi-core SMP true parallelism
- Analog circuitry (ADC quantization/impedance/power rails)
- Instruction/microarchitecture-level quirks

### 4.3 Bypass Discipline

- Bypasses must reside strictly in PAL (substituting physical data sources); **direct DAL `#ifdef` branching is forbidden**.
- Semantic bypasses must pass JSON schema gates ([ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md)).
- If ultrasonic measurements utilize C-side `distanceCm` $\rightarrow$ μs shortcuts (deprecated), Channel 1 edge capture must not be claimed as aligned ([`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md)).
