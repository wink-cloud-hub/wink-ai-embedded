# Axis E — Scheduler & Concurrency

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/03-axes/E-scheduler-concurrency.md
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

Multitasking, blocking, critical sections, and multi-core execution

Definition: Refer to Axis E in [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md).

---

## 2. Primary Mechanism

- [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) — Cooperative single virtual-core scheduler, synchronization primitives, and STRICT_NONBLOCKING

---

## 3. Secondary Mechanisms

(No mandatory secondary mechanisms. Host details reside in [`01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md); Phase 0 interrupt polling resides in [`D-interrupt-model.md`](./D-interrupt-model.md).)

---

## 4. Typical Upper Bounds (Expanded)

1. **Model Scope**: Cooperative single virtual-core scheduler; context switches occur exclusively at explicit yield points—**not** true hardware preemption.
2. **Multi-Core**: `core_id` arguments are ignored; lock-free cross-core write tearing, core-pinning assumptions, and cache/DMA coherency require physical hardware + static analysis. SMP simulation is excluded.
3. **Instruction-Level Races**: Instruction interleaving cannot be verified in this model.
4. **Converged Scope**: Starvation, soft WDTs, and illegal blocking are trapped early under cooperative semantics.
5. **Synchronization Primitives**: Mutex/sleep App-level subsets align with FreeRTOS; standalone queue APIs and priority inheritance remain Planned.
6. **Boundary with Axis D**: Axis E governs task context switching; Axis D governs ISR dispatch. Same-timestamp ordering is governed by [ADR-0053](../../../decisions/unisim/0053-sim-same-timestamp-event-total-order.md).

---

## 5. Associated C Scenarios

Scenario specifications reside in [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md). Statuses reside exclusively in [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md).

- **C3** — Shared State Race Conditions
- **C5** — Blocking / Starvation / Watchdogs
- **C9** — Multi-Core SMP True Concurrency
- **C16** — OS Synchronization Primitives Semantic Parity
