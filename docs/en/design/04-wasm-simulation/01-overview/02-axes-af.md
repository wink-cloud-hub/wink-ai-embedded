# Simulation Multi-Axis Overview (Orthogonal Axes A~F)

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/01-overview/02-axes-af.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Document Level | ① Design Specification (UniSim 3.0 / overview) |
| Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| SSOT Responsibility | **Sole definition source for Axes A~F letters and meanings**; cross-axis matrix (Upper bounds abbreviated) |
| Exclusions | Implementation algorithm details; expanded upper-bound essays (→ [`03-axes/`](../03-axes/)); scenario statuses |
| Migrated From | `04-wasm-simulation-2.0/00-README.md` §1 |
| Last Audit | 2026-08-02 |

Simulation capabilities are categorized and audited across the following orthogonal axes. Each axis evolves independently. Claims of "High Consistency" **must explicitly specify covered axes**.

| Axis | Questions Addressed | Primary Mechanism | Primary Document (3.0) | Typical Upper Bound (Abbreviated) |
|---|---|---|---|---|
| **A. Peripheral Source** | Where sensor/actuator/bus data originates | 4 Channels + PWM subclass (Pin / Bus / Analog / Buffer); PinArbiter; Plugins | [`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md), [`07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md) (Secondary) | No electrical frontend simulation; Channels 3/4 mostly Planned |
| **B. Timebase** | Reference clock for delays / timeouts / pulses | `s_virtual_us` SSOT; Single Gate; bans dual `pal_delay` stepping | [`02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md); C2/C14 in [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) | Non-wallclock real-time; fast-forward alters wallclock duration |
| **C. Timer Semantics** | HW timers / PWM periods / capture | PAL timer; soft stepping approximation; exclusive access gates | [`09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md); C10/C17 in [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md), [ADR-0047](../../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md) | No 10kHz+ hard ISRs; behavioral FOC fast loops |
| **D. Interrupt Model** | ISR execution timing, preemption & nesting | Asyncify cooperative insertion; IRQ queue Poll (**Non-true preemption**) | [`04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md); C4/C15/C20 in [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) | **Cannot** verify priority nesting / hard real-time preemption |
| **E. Scheduler & Concurrency** | Multitasking, blocking, critical sections, multi-core | Cooperative single virtual-core scheduler | [`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md); C3/C5/C9/C16 in [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) | SMP / true preemption $\rightarrow$ Physical hardware |
| **F. Fault & Observability** | OOM, WDT, race conditions, Trace | Fault policies, lint gates, scenario checklists, observation planes | [`05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md), [`11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) (Secondary accuracy), [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md), [`02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md) | Checklist 🚫 items must verify on HIL |

```text
Firmware C (App/BAL/DAL Single-Source Build)
        │
        ├─ A Peripheral Source   ← 08 4 Channels / 07 UniSim Plugin / PinArbiter
        ├─ B/C Time & Timers     ← 02 VirtualClock / 09 Timer Semantics
        ├─ D Interrupt/Order     ← 04 Asyncify + IRQ Poll
        ├─ E Sched & Concurrency ← 03 Cooperative Scheduler / Single Virtual Core
        └─ F Fault & Quality     ← 05/06 + assurance/01/02 + lint + 11 Observability
                ↓
         Physical MCU / HIL (Electrical, Hard Real-Time, Multi-Core...)
```

**Cross-Axis Combinations**:

| Scenario | Involved Axes | Key Constraints |
|---|---|---|
| Ultrasonic ECHO Capture | **A + B** (+ Accuracy Mode) | ECHO edges route via Channel 1 + `s_virtual_us` pulse timing; requires `timing` Accuracy Mode |
| Servo / PWM Output | **A + C** (+ Accuracy Mode) | Channel 1b timing modulation routes to PWM semantics (Axis C); behavioral angle matching |
| I2C Slave Fault / Drop | **A + D + F** | Bus bytes via Channel 2 + IRQ order (Axis D) + Fault injection/observation (Axis F) |
| Button Debouncing | **A + B + F** | Edges via Channel 1 + Virtual clock debounce window (Axis B) + Event observation (Axis F) |

---

## Division of Responsibility with Axis Pages

- This document: **Definitions** + abbreviated upper bounds.
- [`03-axes/`](../03-axes/) pages: Echo the question sentence and **expand** typical upper bounds.
- Axis pages **must never alter** definition phrasing and must link back to this matrix.
