# Axis C — Hardware Timer Semantics

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/03-axes/C-timer-semantics.md
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

HW timer / PWM periods / capture behavior

Definition: Refer to Axis C in [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md).

---

## 2. Primary Mechanism

- [`../02-mechanisms/09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) — Soft stepping, duty bypass, capture, `pal_hwtimer`, and FOC behavioral boundaries

---

## 3. Secondary Mechanisms

- PWM as Channel 1b routing (Duty percentage data flow) $\rightarrow$ [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md)
- Hardware FOC & `pal_hwtimer` layering contract $\rightarrow$ [ADR-0047](../../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md)

---

## 4. Typical Upper Bounds (Expanded)

1. **Model Scope**: **No** on-chip 10kHz+ hard timer ISRs; **No** physical PWM–ADC hardware trigger synchronization; hard real-time preemption cannot be proven via simulation.
2. **PWM L2**: `pal_pwm_set_duty` bypasses to plugin duty percentages; carrier edges, dead-time, and center-aligned waveforms are unmodeled.
3. **FOC / Fast Loops**: Governed by ADR-0047; simulation executes deterministic virtual-time soft-stepping without wallclocks or `rand()`.
4. **Input Capture**: Generic hardware capture abstractions remain Planned; pulse durations route via GPIO pin event queues.
5. **Resource Conflicts**: Pin and timer hardware conflicts are gated behaviorally rather than cycle-accurately.
6. **Cross-Axis Claims**: Servo/PWM outputs require **A + C** (+ Accuracy Mode); ultrasonic pulses require **A + B**.

---

## 5. Associated C Scenarios

Scenario specifications reside in [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md). Statuses reside exclusively in [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md).

- **C10** — Fast-Loop ISR (FOC / Hardware Timers)
- **C17** — Peripheral Resource Mutual Exclusion & Clock Coupling
