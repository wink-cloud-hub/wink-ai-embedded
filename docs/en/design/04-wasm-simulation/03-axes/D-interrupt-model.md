# Axis D — Interrupt Model

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/03-axes/D-interrupt-model.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Layer | Ⅱb Lean Axis Index |
| Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| Definition Source | [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md) (Wording strictly frozen) |

---

## 1. Question Addressed

When ISRs execute; whether preemption and nesting are supported

Definition: Refer to Axis D in [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md).

---

## 2. Primary Mechanism

- [`../02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) — Polling queue, critical section replay, and non-verifiable boundaries

---

## 3. Secondary Mechanisms

- Asyncify and single-threaded host limitations regarding arbitrary instruction preemption $\rightarrow$ [`../02-mechanisms/01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md)
- Scheduler Phase 0 / tick-driven polling and same-timestamp total order $\rightarrow$ [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md)

---

## 4. Typical Upper Bounds (Expanded)

1. **Model Scope**: Cooperative polling $\neq$ NVIC hardware preemption; **cannot** verify priority nesting, instruction-level preemption, or microsecond hard real-time IRQ latency.
2. **Latency Order of Magnitude**: Outside critical sections, edge-to-ISR latency spans approximately one scheduler tick ($\approx 10\text{ms}$).
3. **Critical Sections**: Outermost unlock replay matches immediate pending dispatch, but does **not** replicate all side effects of global hardware interrupt masking.
4. **Timing Claim Exclusions**: High-baud asynchronous UART RX and tight inter-byte frame spacing cannot claim timing fidelity and must verify on HIL.
5. **Same-Timestamp Total Order**: Governed per [ADR-0053](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md); cross-source bit-exact verification remains Planned.
6. **Boundary with Axis E**: Axis D governs when ISRs dispatch; Axis E governs task context switching.
7. **Cross-Axis Claims**: I2C slave faults and drops require **A + D + F**.

---

## 5. Associated C Scenarios

Scenario specifications reside in [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md). Statuses reside exclusively in [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md).

- **C4** — Critical Sections & Interrupt Preemption / Nesting
- **C15** — Host↔Wasm Boundary Integrity
- **C20** — Callback Reentrancy & Deferred Bottom-Halves
