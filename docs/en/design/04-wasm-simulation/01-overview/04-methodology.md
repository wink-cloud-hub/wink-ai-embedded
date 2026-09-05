# Methodology, Reading Paths & Static Gate Summary

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/01-overview/04-methodology.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Document Level | ① Design Specification (UniSim 3.0 / overview) |
| Document Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| SSOT Responsibility | Reading paths by role; Solution taxonomy; Bypass discipline; **STRICT_NONBLOCKING rationale** |
| Exclusions | CMake/linking/selftest implementation details (→ mechanisms with bidirectional links) |
| Governing ADRs | [0002](../../../decisions/unisim/0002-dual-target-compilation.md), [0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md), [0025](../../../decisions/core/0025-app-blocking-api-honesty-pragma-convention.md), [0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md) |
| Migrated From | `04-wasm-simulation-2.0/00-README.md` §0/§4; `11-consistency-spec.md` §0.1–§0.3 |

> This document defines role-based reading paths, consistency solution taxonomy, and bypass/blocking API boundaries. Implementation details reside in [`02-mechanisms/`](../02-mechanisms/00-README.md); scenario contracts and oracles reside in [`04-assurance/`](../04-assurance/00-README.md).

---

## 1. Reading Paths

### 1.1 "Can scenario X be verified right now?"
$\rightarrow$ Check the status matrix (✅/🟡/❌/🚫) in **[`04-assurance/02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md)**, jumping to [`04-assurance/01-consistency-spec.md`](../04-assurance/01-consistency-spec.md) for mechanisms and oracles.

### 1.2 "What is the implementation status of engine mechanism Y?"
$\rightarrow$ Check **[`04-assurance/03-roadmap-and-governance.md` §1.1](../04-assurance/03-roadmap-and-governance.md)** (Landed/Partial/Stub/Planned/Deprecated matrix).

### 1.3 Mechanism Quick Reference

| Mechanism | Document |
|---|---|
| Worker Isolation / Asyncify / INTERACTIVE & HEADLESS modes | [`02-mechanisms/01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md) |
| Virtual Clock `s_virtual_us` / Single Gate / Fast-forward | [`02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) |
| Cooperative Scheduler / Fibers / Task State Machines / WCET | [`02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) |
| Interrupt Poll Model / IRQ FIFO / Critical Section Replay | [`02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) |
| Memory Quota / OOM / Fault Latching / Safe-off / ASan | [`02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) |
| Physical Degradation & Fault Injection (Jitter/RC/Warmup/Drops) | [`02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) |
| Virtual Peripheral Registry / PinArbiter / JSON Profiles | [`02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md) |
| 4-Channel Routing & Peripheral Selection | [`02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) |
| Axis C Timers / PWM / `pal_hwtimer` Soft Stepping | [`02-mechanisms/09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) |
| Wasm↔JS Full ABI Symbol Inventory & Contracts | [`02-mechanisms/10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md) |
| Accuracy Modes / Observability Planes / Lifecycle & Reset | [`02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) |
| Overall Architecture / Co-Simulation Domains / Data Plane | [`01-architecture.md`](./01-architecture.md) |

### 1.4 Role-Based Reading Workflows

- **App / Low-Code Developers**: [`00-README` §1–§3](../00-README.md) $\rightarrow$ [`02-axes-af.md`](./02-axes-af.md) $\rightarrow$ [`03-production-contract.md`](./03-production-contract.md) $\rightarrow$ [`08-channel-routing`](../02-mechanisms/08-channel-routing.md) $\rightarrow$ [`02-consistency-checklist`](../04-assurance/02-consistency-checklist.md).
- **Driver / DAL Developers**: [`08-channel-routing`](../02-mechanisms/08-channel-routing.md) $\rightarrow$ [`10-wasm-js-bridge-abi`](../02-mechanisms/10-wasm-js-bridge-abi.md) $\rightarrow$ C1/C7/C17 in [`01-consistency-spec`](../04-assurance/01-consistency-spec.md) $\rightarrow$ ADR-0003 / 0040.
- **Simulation Engine / UniSim Developers**: [`01-architecture.md`](./01-architecture.md) $\rightarrow$ `01` through `11` in [`02-mechanisms/`](../02-mechanisms/00-README.md).
- **CI / QA Leads**: [`02-consistency-checklist`](../04-assurance/02-consistency-checklist.md) $\rightarrow$ [`03-roadmap-and-governance`](../04-assurance/03-roadmap-and-governance.md) $\rightarrow$ [`11-accuracy-observation-lifecycle`](../02-mechanisms/11-accuracy-observation-lifecycle.md) $\rightarrow$ [`01-consistency-spec`](../04-assurance/01-consistency-spec.md).

---

## 2. Solution Taxonomy (Assurance Strategy Classification)

| Type | Responsibility | Representative Techniques |
|---|---|---|
| **A Constrained Code** | Blocks unsafe syntax at compile/IDE time | Lint rules, `STRICT_NONBLOCKING` symbol hiding, `wink-app.json` pin collision gates |
| **B Engine Modeling** | Replicates physics/time/buses in Wasm/Host | `s_virtual_us` Single Gate, zero-yield pin queues, 100% single-source DAL, deterministic noise |
| **C Observability Gates** | Intercepts runtime concurrency/overflow/exhaustion | Heap quota faults, shadow memory TSan, soft WDT, ASan/UBSan |
| **Hardware / HIL** | Accommodates intentionally unmodeled physics | HIL board test automation, hard ISRs, SPICE, multi-core cache sampling |

---

## 3. Bypass Discipline

- **PAL is the sole legitimate bypass location**: Only physical data sources are substituted (pin levels, pulse widths, bus responses, raw ADC values); App/BAL/DAL logic compiles identically ([ADR-0002](../../../decisions/unisim/0002-dual-target-compilation.md)).
- **Banning DAL `#ifdef SIMULATION` business branching**: Replacing entire drivers in JS prevents CRC, timeouts, and retries from running in simulation ([ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md) Decision 2).
- **Fail-Loud JSON Gates**: Semantic bypasses undeclared in `wink-app.json` must fail at build/run time ([ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md)).
- External plugin outputs (3D collision distances) must write back through injection APIs into ECHO edges or raw ADC counts, **never** returning directly as DAL values ([`08-channel-routing`](../02-mechanisms/08-channel-routing.md)).

---

## 4. STRICT_NONBLOCKING & Bringup Isolation

### 4.1 Rationale (ADR-0025)

1. **Simulation Default Strictness**: `wink-micro-os/CMakeLists.txt` builds `wink_simulator` with `-DWINK_STRICT_NONBLOCKING=1`; `WINK_BLOCKING` APIs are hidden in headers $\rightarrow$ **Link-time fail-fast**.
2. **Bringup / Selftest Isolation**: Blocking utilities reside in `wink-micro-os/runtime/selftest/`, isolated behind `#ifndef WINK_STRICT_NONBLOCKING`.
3. **App Main Loop & Callbacks Ban Blocking Pragmas**.

### 4.2 Implementation References

- [`02-mechanisms/01-sandbox-and-execution.md` §5](../02-mechanisms/01-sandbox-and-execution.md#5-strict_nonblocking-构建落地怎么做) — CMake scope, selftest isolation, Asyncify/HEADLESS boundaries.
- [`02-mechanisms/03-scheduler-and-concurrency.md` §8](../02-mechanisms/03-scheduler-and-concurrency.md#8-strict_nonblocking-编译期门禁adr-0025) — Cooperative blocking semantics, LIGHT assertions, `app_loop` rules.

---

## 5. Mechanism Maturity vs Scenario Testability (Orthogonal)

| Dimension | Question | Marking System | Location |
|---|---|---|---|
| **Mechanism Maturity** | Has the engine capability landed? | Landed / Partial / Stub / Planned / Deprecated | Root [`00-README.md` §3.2](../00-README.md#32-实现成熟度词表机制落地状态); mechanisms frontmatter; [`03-roadmap-and-governance` §1.1](../04-assurance/03-roadmap-and-governance.md) |
| **Scenario Testability** | Can scenario C be verified right now? | ✅ / 🟡 / ❌ / 🚫 | Exclusively in [`02-consistency-checklist.md`](../04-assurance/02-consistency-checklist.md) |
