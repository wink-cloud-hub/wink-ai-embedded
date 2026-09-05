# Axis B — Timebase

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/03-axes/B-timebase.md
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

Reference clock for delays / timeouts / pulse durations

Definition: Refer to Axis B in [`../01-overview/02-axes-af.md`](../01-overview/02-axes-af.md).

---

## 2. Primary Mechanism

- [`../02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) — `s_virtual_us` SSOT, Single Assignment Gate, HEADLESS fast-forwarding

---

## 3. Secondary Mechanisms

- Execution modes and fast-forward yield behavior (INTERACTIVE vs HEADLESS, Asyncify) $\rightarrow$ [`../02-mechanisms/01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md)
- `timing` Accuracy Mode evidence validity $\rightarrow$ [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md)

---

## 4. Typical Upper Bounds (Expanded)

1. **Model Scope**: Logic timing is anchored to virtual microseconds and is **not** synchronized in real-time with host wallclocks; HEADLESS execution executes much faster than real-time.
2. **Single Gate**: `pal_delay_*` must never actively advance `s_virtual_us` to prevent dual stepping violations (C14).
3. **Oscillator Drift**: Oscillator and crystal ppm drift are intentionally unmodeled.
4. **Division with Axis C**: Axis B defines the reference clock; timer hardware behavior and PWM carrier waveforms belong to Axis C ([`C-timer-semantics.md`](./C-timer-semantics.md)).
5. **Cross-Axis Claims**: Ultrasonic pulse width measurements require **A + B** under `timing` mode; button debouncing requires **A + B + F**.
6. **Wrap-Around**: Application uint32 tick counters must handle overflow; internal timeouts rely on absolute timestamps (`wakeup_us`).

---

## 5. Associated C Scenarios

Scenario specifications reside in [`../04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md). Statuses reside exclusively in [`../04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md).

- **C2** — Virtual Microsecond Logic Timing
- **C14** — Fast-Forward & Co-Simulation Stepping Contracts
- **C21** — Time & Counter Wrap-Around
