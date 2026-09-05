# Simulation Overall Architecture & Co-Simulation Model

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/01-overview/01-architecture.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Document Level | ① Design Specification (UniSim 3.0 / overview) |
| Document Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| **Landed** | **Landed** (Architecture overview and code map; excludes unlanded commitments) |
| Governing ADRs | [0002](../../../decisions/unisim/0002-dual-target-compilation.md), [0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md), [0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md), [0042](../../../decisions/unisim/0042-sim-execution-modes.md) |
| Associated Code | `wink-micro-os/osal/wasm/`, `wink-micro-os/targets/wasm/`, `wink-micro-os/targets/common/`, `@wink-ai/unisim` (Standalone SDK dependency package) |
| Last Audit | 2026-08-02 |

> This document defines the engine subsystems, their data exchange pipelines, and codebase organization. Detailed mechanisms (clocks/schedulers/interrupts/faults/peripherals) reside in [`02-mechanisms/`](../02-mechanisms/00-README.md).

---

## 1. Layered Architecture Overview

```text
┌────────────────────────────────────────────────────────────┐
│      Vue 3 Main Thread (Canvas / ControlHub / ProductWorld*)│  ← Axis A Injection & Observation
└───────────────────────────────▲────────────────────────────┘
                                │ postMessage
                                ▼
┌────────────────────────────────────────────────────────────┐
│             Web Worker (SimWorker + Plugins)               │
│  PinArbiter / I2C·SPI·UART Bus / VirtualClock / Fault      │  ← Axes A/B/F
│  ┌──────────────────────┐   ┌──────────────────────────┐   │
│  │   Wasm-Core (C OS)   │──►│   Wasm JS Bridge         │   │
│  │ App/BAL/DAL + PAL API│   │ Asyncify · js_pal_*      │   │  ← Axes D/E
│  │ OSAL Cooperative     │   │ InterruptQueue (Poll)    │   │
│  └──────────────────────┘   └──────────────────────────┘   │
└────────────────────────────────────────────────────────────┘
```

Critical Boundaries:

- **App / BAL / DAL are 100% single-source C code**, compiling identically to wasm32 and xtensa (ESP32). App code is strictly forbidden from using `#ifdef SIMULATION` to branch business logic ([ADR-0002](../../../decisions/unisim/0002-dual-target-compilation.md), C1.1).
- **PAL is the sole legitimate bypass insertion point**. Physical inputs (pin levels, pulse widths, bus slave responses, raw ADC values, buffer payloads) are substituted inside PAL Wasm implementations; DAL targets zero simulation macros (see [`08-channel-routing`](../02-mechanisms/08-channel-routing.md)).
- **Zero business physics in the C kernel**. Robot kinematics, sensor degradation algorithms, and environmental models reside exclusively in JS plugins.
- **\* ProductWorld (3D physics/visualization) is planned/in-progress**; canvas and ControlHub serve active Axis A injection and observation.

---

## 2. Decoupling Control and Physical Domains (Co-Simulation)

```text
┌────────────────────────────────────────────────────────┐
│             App Control Domain                         │  ◄── 100% Single-Source C Business Code
└───────────────────────────┬────────────────────────────┘
                            ▼
┌────────────────────────────────────────────────────────┐
│             Platform Sim OS Base                       │  ◄── Domain-Agnostic Engine Base
│   Clocks, Scheduler, Pins, IRQ Poll, Buses, Faults     │
└───────────────────────────┬────────────────────────────┘
                            ▲ (Bidirectional Pin/Bus Data Exchange)
                            ▼
┌────────────────────────────────────────────────────────┐
│           External Environment / Device Plugins        │  ◄── Physics & Degradation Plugins
│    Kinematics/Dynamics, Sensor Degradation, Displays   │
└────────────────────────────────────────────────────────┘
```

3-Layer Responsibilities:

1. **App Control Domain**: State machines, control laws, and protocol serialization in App/BAL/DAL.
2. **Platform Sim OS Base**: Provides `s_virtual_us` virtual clock, cooperative scheduler, virtual pin arbiters, IRQ Poll queues, memory quotas, and fault isolation.
3. **Plugin Domain**: Converts control signals (PWM/GPIO outputs) over Δt physical steps into simulated sensor quantities, writing them back into the base.

### 2.1 Platform Data Plane

Observed and injected APIs exported to JS via `EMSCRIPTEN_KEEPALIVE` (Full inventory in [`10-wasm-js-bridge-abi`](../02-mechanisms/10-wasm-js-bridge-abi.md)):

| Direction | Representative APIs | Purpose |
|---|---|---|
| Export (C→JS Observation) | `pal_wasm_get_gpio_output`, `pal_wasm_get_pwm_duty_percent`, `pal_wasm_get_servo_angle` | Reads control signals emitted by firmware |
| Import (JS→C Injection) | `pal_wasm_set_gpio_input`, `pal_wasm_set_ultrasonic_distance`, `pal_wasm_push_pin_event` | Injects physical values and edge events |
| Control | `pal_wasm_advance_virtual_clock`, `pal_wasm_reset_physical`, `pal_wasm_set_sim_mode` | Stepping, resetting, execution mode selection |

### 2.2 Step-Lock Co-Simulation Pipe

At each simulation step:
1. Plugin reads control signals (PWM/GPIO outputs);
2. Updates physical state over `Δt` (bound to `s_virtual_us`) and computes sensor values;
3. Writes sensor values back into the base via injection APIs.

### 2.3 Determinism and Fast-Forwarding

- **Determinism Commitment**: Fixed input + fixed unisim/C build digest + fixed PRNG seed $\rightarrow$ **reproducible event ordering and logical outcomes across runs**. Anchored by `s_virtual_us` Single Gate ([`02-virtual-clock`](../02-mechanisms/02-virtual-clock.md)), cooperative scheduling determinism ([`03-scheduler-and-concurrency`](../02-mechanisms/03-scheduler-and-concurrency.md)), and seeded LCG PRNG (`wink_phys_prng_next`).
- **Non-Guaranteed Scope**: Bit-for-bit cross-browser floating-point parity is not guaranteed; timing relies on virtual clocks, **not wallclock synchronization**.
- **Fast-Forwarding**: In `HEADLESS` mode, C steps virtual clocks directly without Asyncify unwind/rewind overhead, executing faster than real-time for CI batches.

---

## 3. Execution Topology: Worker & Wasm Instances

- **Wasm must execute inside Web Workers**; the main thread handles message-driven rendering only. Running Wasm + Asyncify on the main UI thread starves timers and causes OOM crashes ([`01-sandbox-and-execution` §1](../02-mechanisms/01-sandbox-and-execution.md), C15.5).
- Each Worker encapsulates one Wasm sandbox and one set of SimWorker arbiters (VirtualClock, PinArbiter, Bus, Fault Bridge).

---

## 4. Codebase Map

### 4.1 C Layer (`wink-micro-os`)

| Path | Responsibility |
|---|---|
| `wink-micro-os/osal/wasm/pal_osal_wasm.c` | Wasm OSAL: `s_virtual_us` clock, task scheduler hooks, `wink_vclock_advance_internal` Single Gate, HEADLESS time stepping |
| `wink-micro-os/osal/host/pal_osal_host.c` | Host native OSAL (Win32 Fibers + host virtual clock), matching Wasm semantics |
| `wink-micro-os/targets/wasm/` | Wasm target adapters: `pal_hal_wasm.c`, `pal_irq_wasm.c`, `pal_wasm_physical.c`, `pal_wasm_fault.c`, `pal_wasm_fault_domain.c`, `pal_log_wasm.c`, `pal_storage_wasm.c` |
| `wink-micro-os/targets/wasm/devices/` | `wasm_dev_ultrasonic.c`, `wasm_dev_servo.c`, `wasm_sim_registry.c` |
| `wink-micro-os/targets/wasm/wasm_bridge.h` | **Wasm↔JS ABI SSOT**, centralizing all `js_pal_*`/`pal_wasm_*` declarations and 6 ABI contracts |
| `wink-micro-os/targets/wasm/wink_sim_js.js` | `--js-library` default stubs (Wrapper pattern + `__async:'auto'`) |
| `wink-micro-os/targets/wasm/exported_runtime_functions.json` | Link-time exports and Asyncify configuration SSOT |
| `wink-micro-os/targets/common/include/`, `src/` | Target-agnostic algorithms: `wink_sim_scheduler.*`, `wink_sim_physical.*`, `sim_ctx.h`, `pal_resource.c` |

### 4.2 TypeScript Layer (`@wink-ai/unisim` Simulation Engine Core)

| Logical Module | Responsibilities & Capabilities |
|---|---|
| `VirtualClock` | μs-precision bigint deterministic virtual clock (JS mirror) |
| `PinArbiter` | 4-value logic + 3-level drive strength electrical pin arbiter (Electrical SSOT) |
| `PeripheralRegistry` | Virtual peripheral registry and lifecycle management |
| `Observability Suite` | Pin tracing (Tracer), VCD export (Exporter), session recorder (Recorder), debugger (Debugger) |
| `Wasm Bridge` | `js_pal_*` import factories, I2C/SPI/UART bus drivers, interrupt queues |
| `SimWorker` | Web Worker orchestrator |
| `Physical & Fault Bridge` | Physical degradation bridge and fault injection |
| `Fault Composer` | Noise, delay, and packet drop fault synthesis |
| `Bus Analyzer` | I2C / SPI / UART protocol packet analyzer |
| `Types & ABI Spec` | Pin types, WASM import/export contracts, runtime status and timing contracts |

---

## 5. Single-Source Dual-Target Compilation

- C business logic compiles concurrently to `wasm32-unknown-emscripten` and `xtensa` (ESP-IDF). Host native compilation supports CI unit tests ([ADR-0002](../../../decisions/unisim/0002-dual-target-compilation.md)).
- Target differences are encapsulated behind `sim_ctx_*` (Fibers), `pal_osal_*`, and `pal_hal_*`.
- Zero binary pollution: Simulation-only code (`wink_sim_physical`, `pal_wasm_*`) is excluded from ESP32/baremetal builds by explicit CMake source enumeration.
