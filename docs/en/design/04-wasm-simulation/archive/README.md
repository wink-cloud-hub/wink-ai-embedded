# WebAssembly Simulation & Frontend Runtime Engine (UniSim)

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/archive/README.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

> **⚠️ Archived (2026-08-02)**: This directory contains the legacy UniSim **1.0** specifications, superseded by [**3.0 (Active SSOT)**](../00-README.md) (intermediate version 2.0 was archived and removed). The contents below serve as historical reference only and are no longer actively maintained; refer to 3.0 for current active specifications.

This directory documents the architecture and design specifications of the **Wink-AI** frontend Wasm simulation sandbox and browser virtual peripheral runtime library (the UniSim engine).

The simulation layer allows C business logic compiled to `wasm32` bytecode to execute within a Web Worker sandbox, interoperating with Vue 3 / virtual peripheral UIs (and planned ProductWorld 3D) to achieve **behavioral high-fidelity** simulation of embedded devices.

> **Reading Guide**: Document `03` addresses only "how peripheral physical quantities enter firmware"; whole-system fidelity (timing, interrupts, scheduling, testable scenarios) is governed by `05` + `08`. Do not substitute the four-channel routing model for the complete simulation architecture.

---

## Simulation Multi-Axis Overview (Orthogonal Dimensions)

Simulation capabilities must be understood and validated across the orthogonal axes below. Each axis evolves independently; claims of "high consistency" must specify which axes are covered.

| Axis | Question Answered | Primary Mechanism | Primary Document | Typical Upper Bound |
|---|---|---|---|---|
| **A. Peripheral Physical Sources** | Where sensor/actuator/bus data originates | Four channels + PWM subtypes: Pin / Bus / Analog / Buffer; PinArbiter, Plugins | [03](./03-multi-channel-sim-routing.md), [02](./02-virtual-peripheral-registry.md) | No electrical frontend simulation; partial channels Planned |
| **B. Time Base** | Who governs delays / timeouts / pulse widths | `s_virtual_us` SSOT; dual stepping in `pal_delay` prohibited | [05](./05-simulation-consistency-and-fidelity-spec.md) C2/C14, [06](./06-physical-degradation-engine.md) | Not wall-clock real-time; fast-forwarding alters wall-clock duration |
| **C. Timer Hardware Semantics** | HW timers / PWM periods / input captures | PAL timer, software step approximations; resource exclusivity gates | [05](./05-simulation-consistency-and-fidelity-spec.md) C10/C17, [ADR-0047](../../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md) | No 10kHz+ hard ISRs; FOC fast loop is behavioral |
| **D. Interrupt Model** | When ISRs run, preemption / nesting capabilities | Asyncify cooperative yielding, IRQ queue polling (non-preemptive) | [01](./01-wasm-sandbox-lifecycle.md), [05](./05-simulation-consistency-and-fidelity-spec.md) C4/C15/C20 | **Cannot** validate priority nesting or hard real-time preemption |
| **E. Scheduling & Concurrency** | Multitasking, blocking, critical sections, multi-core | Cooperative single virtual core scheduler | [07](./07-scheduler-model.md), [05](./05-simulation-consistency-and-fidelity-spec.md) C3/C5/C9/C16 | SMP / True preemption $\rightarrow$ Physical Hardware |
| **F. Faults & Observation** | OOM, WDT, race conditions, Trace | Fault policies, linting/gates, scenario checklists | [05](./05-simulation-consistency-and-fidelity-spec.md), [08](./08-simulation-consistency-checklist.md), [ADR-0045](../../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md) | Checklist items marked 🚫 require HIL testing |

```text
Firmware C (App/BAL/DAL Homologous Target)
        │
        ├─ A Peripheral Physical Sources ←── 03 Four Channels / UniSim Plugin
        ├─ B/C Time & Timers ←── VirtualClock / PAL timer
        ├─ D Interrupt / Callback Ordering ←── Asyncify + IRQ Queue
        ├─ E Scheduling & Shared State ←── 07 Cooperative Scheduler
        └─ F Faults & Gating ←── 05/08 + Linter
                ↓
         Physical Hardware / HIL (Electrical, Hard Real-Time, Multi-Core...)
```

**Cross-cutting Example**: Ultrasonic "Channel 1 High Consistency" = **A** (ECHO edge) + **B** (VirtualClock pulse width) + recommended **`timing`** Accuracy Mode; relying solely on Hub injection of `distanceCm` shortcuts is insufficient to claim edge capture consistency.

To determine "whether a scenario can be verified now" $\rightarrow$ consult **[08](./08-simulation-consistency-checklist.md)**; for mechanisms and contracts $\rightarrow$ consult **[05](./05-simulation-consistency-and-fidelity-spec.md)**.

### Product Position After A–F Completion (Consistency vs Physical Hardware)

Fulfilling axes A through F **does not equal** "Simulation $\equiv$ Hardware" or "Hardware-free releases". The official standard is defined below (details in [05 §0.4](./05-simulation-consistency-and-fidelity-spec.md#04-af-完备后的生产口径与残余不一致)):

| "Production-Grade" Meaning | After A–F Implementation |
|---|---|
| High-confidence behavioral pre-check in CI / low-code workflows | **Feasible** (escapes categorized, false greens explainable, 🚫 enforced on hardware) |
| Simulation results **substituting** hardware / HIL as final release gate | **No** |
| Simulation and hardware are **bit-for-bit / microsecond-identical** | **Never promised** |

**Residual Inconsistencies that will persist** (model boundaries, not unfinished work): Electrical/analog frontends, hard real-time and preemptive interrupt nesting, silicon-level HW timers / FOC hard ISRs, multi-core SMP, microarchitectural details, host Asyncify/Worker wall-clock pacing, etc.

**Inconsistencies that will be substantially eliminated**: DAL/App homologous conversions/timeouts/protocol framing, logical ordering under virtual clocks, resource conflicts and illegal blocking gates, starvation under cooperative scheduling / soft WDT—transitioning from "random regressions" to "[08](./08-simulation-consistency-checklist.md) known gaps + mandatory hardware tests".

```text
A–F Complete
  → Production-Grade "Behavioral/Protocol/Resource Pre-check Pipeline" ✅
  → Production-Grade "Identity with Physical Hardware / Zero-Hardware Release" ❌

Residual Inconsistencies → Managed via 08 checklist + HIL/Hardware gates + ADR-0003 standards, not eliminated
```

---

## Module Design Documents

*   **[01-wasm-sandbox-lifecycle.md](./01-wasm-sandbox-lifecycle.md)** — Wasm Sandbox Lifecycle & Asyncify (**Axes D/E Entry Point**)
    *   Web Worker isolation, Asyncify suspension/resumption, Wasm Table and GPIO ISR cooperative yielding.
*   **[02-virtual-peripheral-registry.md](./02-virtual-peripheral-registry.md)** — Virtual Circuits / DeviceTree / SchemaForm (**Axis A Configuration Plane**)
    *   Circuit topology, peripheral property forms, wiring and instance registration.
*   **[03-multi-channel-sim-routing.md](./03-multi-channel-sim-routing.md)** — Four-Channel PAL Peripheral Routing & Selection (**Axis A Only**)
    *   Pin / Bus / PWM Duty / Analog / Buffer; layered homology; Plugin Channel boundaries; **not** whole-system architecture.
*   **[04-velxio-migration-analysis.md](./04-velxio-migration-analysis.md)** — Velxio Comparison & Migration Analysis
    *   Inherited assets (Wokwi Elements, wiring, TS device models) and abandoned approaches (instruction-level emulation).
*   **[05-simulation-consistency-and-fidelity-spec.md](./05-simulation-consistency-and-fidelity-spec.md)** — Consistency & Fidelity Principles SSOT (**Axes B–F + Scenario Contracts**)
    *   VirtualClock / Co-Sim; C1–C25 problems, solutions, predictions, boundaries; Phase milestones.
*   **[08-simulation-consistency-checklist.md](./08-simulation-consistency-checklist.md)** — Scenario Testability Index SSOT
    *   "Can this scenario be tested now?"; ✅/🟡/❌/🚫; cross-links to 05.
*   **[06-physical-degradation-engine.md](./06-physical-degradation-engine.md)** — Physical Degradation & Fault Injection (ADR-0009)
    *   Virtual clock architecture, degradation engine, fault tiers, Worker protocols.
*   **[07-scheduler-model.md](./07-scheduler-model.md)** — Cooperative Scheduler Model (ADR-0013/0014) (**Axis E**)
    *   Task state machines, pick_next, host vs wasm boundaries, WCET wall-clock backstops.

---

## Known Simulation Limitations (Behavioral Boundaries)

Wink-AI simulation provides **behavioral (causal) high fidelity**: guaranteeing business logic causal ordering and logical correctness, but **does not guarantee** cycle/tick-level timing fidelity or electrical fidelity. See [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md).

- ✅ **Verifiable** (when corresponding axis is implemented and scenario is ✅/🟡): Business state machines, sensor/actuator semantics via homologous PAL paths, I2C/UART **payload-level** protocols, timeout/disconnect exceptions, subset of logical ordering under virtual clocks.
- ❌ **Non-verifiable** (or weakly approximated): Microsecond-level hard real-time responses, **interrupt preemption and priority nesting**, silicon HW timers / FOC hard ISRs, multi-core SMP concurrency, analog circuitry (ADC quantization/impedance/power rails), instruction-level / microarchitectural dynamics.
- ⚠️ **Peripheral Axis Notice**: Bypasses must reside strictly at PAL (substituting physical quantity sources); **never** implement DAL direct pass-throughs with `#ifdef`. Ultrasonic sensors using C-side `distanceCm` $\rightarrow \mu\text{s}$ shortcuts cannot claim Channel 1 edge capture alignment—see [03 §5.1](./03-multi-channel-sim-routing.md).

---

## Core Simulation Layers

```text
 ┌────────────────────────────────────────────────────────┐
 │     Vue 3 Main Thread (Canvas / ControlHub / World)    │  Axis A Injection & Observation
 └───────────────────────────▲────────────────────────────┘
                             │ postMessage
                             ▼
 ┌────────────────────────────────────────────────────────┐
 │             Web Worker (SimWorker + Plugins)           │
 │  PinArbiter / I2C·SPI·UART / VirtualClock / PluginHost │  Axes A/B
 │  ┌───────────────────────┐    ┌─────────────────────┐  │
 │  │    Wasm-Core (C OS)   ├───►│   Wasm JS Bridge    │  │
 │  │ App/BAL/DAL + PAL API │    │ Asyncify · js_pal_* │  │  Axes D/E
 │  │ OSAL Co-op Scheduler  │    │ IRQ Queue Poll      │  │
 │  └───────────────────────┘    └─────────────────────┘  │
 └────────────────────────────────────────────────────────┘
```

---

## STRICT_NONBLOCKING & Bringup Isolation (ADR-0025)

To expose illegal blocking early during simulation and protect against hardware WDT starvation:

1. **Simulation Defaults to Strict**: CMake enables `-DWINK_STRICT_NONBLOCKING=1`; `WINK_BLOCKING` APIs (such as blocking `dal_ultrasonic_read`) are omitted in headers, causing link-time fail-fast upon misuse.
2. **Bringup / Selftest Isolation**: Blocking helper utilities are placed in `runtime/selftest/`, wrapped in `#ifndef WINK_STRICT_NONBLOCKING`; strict mode leaves stubs only, preventing blocking code from entering the simulation sandbox.
