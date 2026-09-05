# Glossary

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/01-overview/05-glossary.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Document Level | ① Design Specification (UniSim 3.0 / overview) |
| Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| SSOT Responsibility | **Sole authoritative definitions for terms**: One-line definition + links to mechanisms/scope |
| Exclusions | Algorithm details, status matrices, roadmap tables |
| Last Audit | 2026-08-02 |

> Addition Rule: New terms in body text must be defined here with a one-line summary.

| Term | One-Line Definition | Details |
|---|---|---|
| UniSim | Browser/Node Wasm simulation engine (`@wink-ai/unisim`); Worker hosts single-source C + JS peripherals via `wasm_bridge` ABI | [`01-architecture.md`](./01-architecture.md), [`10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md) |
| Naming Map | Same engine across contexts: npm `@wink-ai/unisim`, C/CMake `wink_simulator`, Emscripten export `WasmSandbox` (`EXPORT_NAME`), doc "UniSim/Wasm Simulation" | [`01-architecture.md`](./01-architecture.md) |
| Step-Lock Pipe | Co-simulation step contract: Plugin reads controls $\rightarrow$ updates physics over Δt $\rightarrow$ injects into base (3-beat lockstep) | [`01-architecture.md` §2.2](./01-architecture.md) |
| Δt (step delta) | Virtual time advanced per simulation step, bound to `s_virtual_us`; shared by plant and OS for lockstep determinism | [`01-architecture.md` §2.2](./01-architecture.md), [`02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) |
| plant (Controlled Object) | Physical dynamics and environment governed by firmware (chassis kinematics, sensors); isolated in Plugin domain | [`01-architecture.md` §2](./01-architecture.md) |
| 3 Co-Sim Domains | App Control Domain (100% C) / Platform Sim OS / Plugin Physical Domain — Injections occur **strictly** at PAL | [`01-architecture.md`](./01-architecture.md) |
| ProductWorld | Vue 3 main thread 3D UI (In-progress); physical values $\rightarrow$ pins/ADC, **forbidden** direct DAL linkage | [`01-architecture.md`](./01-architecture.md), [`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) |
| PinArbiter | Multi-driver electrical pin arbiter (Logic level + Drive strength); IRQ edge event source | [`07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md), [`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) |
| Asyncify | Emscripten transform: Blocking C calls yield Wasm stack; JS can override imports to return Promises | [`01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md) |
| Fiber | Cooperative coroutine `sim_ctx_*`; Win32 Fibers / Emscripten fiber backends | [`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) |
| WCET | Simulation scheduler timeslice watchdog (~5ms $\rightarrow$ Fault 8002); bypassable in HEADLESS / debugger | [`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) |
| Gate (Clock) | Virtual clock Single Gate `wink_vclock_advance_internal`, maintaining `s_virtual_us` | [`02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) |
| soft-stepping | Behavioral timer/PWM stepping approximation (Axis C); non-cycle-accurate | [`09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) |
| safe-off | Actuator safety shutdown during fault states via `wink_actuator_safe_off_all` | [`05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) |
| safeWrap | Higher-order wrapper around `js_*` imports: Host throw $\rightarrow$ `host_fault` 8003 (`createUnisimImports.ts`) | [`10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md), [`05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) |
| Fault | Latched runtime fault domains (OOM/WCET/Host); triggers safe-off and isolation | [`05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md), [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) |
| Soft WDT | Planned virtual time starvation watchdog (C5.2); **unimplemented** | [`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md), [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) |
| Accuracy Mode | Behavioral \| Timing \| Cycle accuracy policies; orthogonal to Execution Mode | [`11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) |
| Execution Mode | `INTERACTIVE` vs `HEADLESS` ([ADR-0042](../../../decisions/unisim/0042-sim-execution-modes.md)) | [`01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md) |
| Evidence-L1 / L2 | **Evidence Levels** (Accuracy): L1 State machine; L2 Payload/edge causality — Observation validity only | [`11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) |
| Fault-L1 / L2 / L3 | **Fault Injection Tiers**: L1 Pin middleware; L2 Bus; L3 Device error semantics | [`06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) |
| Test-L0…L3 | **Testing Pyramid Tiers**: Compilation $\rightarrow$ Unit $\rightarrow$ Integration $\rightarrow$ Same-binary determinism | [`06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) §9 |
| 4 Channels + 1b | Pin / Bus / Analog / Buffer (PWM classified as Channel 1b Timing Modulation) | [`08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) |
| Bypass | Substituting physical data sources inside PAL; semantic bypasses require JSON gates (ADR-0040) | [`04-methodology.md`](./04-methodology.md), [ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md) |
| HIL | Hardware-in-the-Loop; validates simulation 🚫 scenarios | [`03-production-contract.md`](./03-production-contract.md), [`02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md) |
| selftest / Bringup | `runtime/selftest/`; blocking utilities allowed only outside STRICT mode | [`04-methodology.md`](./04-methodology.md), [`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) |
| Observability Plane | PinTracer + VcdExporter / SessionRecorder / BusAnalyzer | [`11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) |
| A~F | Simulation fidelity orthogonal axes | [`02-axes-af.md`](./02-axes-af.md) |
| C1~C25 | Consistency scenario namespace (Problem / Solution / Oracle contracts) | [`01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) |
