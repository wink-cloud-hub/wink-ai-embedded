# Simulation Consistency & High-Fidelity Specification (C1~C25)

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/04-assurance/01-consistency-spec.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Document Level | ① Design Specification (UniSim 3.0 / assurance) |
| Document Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| SSOT Responsibility | Scenario **Principles / Solutions / Oracles / Boundaries**; **excludes** testability status matrices |
| Associated Code | Referenced mechanism files per sub-scenario; test harness in `wink-tools/wink.py` |
| Last Audit | 2026-08-02 |
| Governing ADRs | 0001, 0002, 0003, 0009, 0013, 0014, 0019, 0025, 0040, 0042, 0045, 0047, 0053, 0054, 0055 |
| Migrated From | `04-wasm-simulation-2.0/11-consistency-spec.md` |

> **SSOT Division of Responsibility**: Read this document for scenario technical contracts; check [`02-consistency-checklist.md`](./02-consistency-checklist.md) for active testability statuses.

---

## 0. General Rules (Frozen Structure)

### 0.1 5-Field Sub-Scenario Template

Every sub-scenario is documented using the following five fields:

| Field | Meaning |
|---|---|
| **Problem** | Common pitfalls on physical hardware; App/driver patterns triggering them |
| **Hardware vs Simulation** | Divergence mechanisms (why simulation may fail silently or report false greens) |
| **Assurance Solution** | A Constrained Code / B Engine Modeling / C Observability Gates / Hardware Verification |
| **Acceptance Oracle** | Concrete assertion criteria (Standard vocabulary defined in §0.2) |
| **Boundary** | Intentionally unmodeled domains or known approximation bounds |

**Field Exemptions (🚫 / Hardware & HIL Exclusive)**: For sub-scenarios marked **🚫** in [`02`](./02-consistency-checklist.md), **Problem** and **Hardware vs Simulation** may be omitted; **Assurance Solution**, **Acceptance Oracle**, and **Boundary** remain mandatory.

### 0.2 Standard Acceptance Oracle Vocabulary

| Term | Meaning | Typical Assertion Mechanism |
|---|---|---|
| **Fail-Fast / Fail-Loud** | Blocks immediately at compile-time or kernel initialization | `-DWINK_STRICT_NONBLOCKING=1` linker errors, JSON schema gate failures |
| **Fault** | Triggers kernel runtime fault isolation and captures context | Heap quota OOM, soft WDT timeouts, TSan assertions |
| **Bit-Identical / Golden Trace** | Execution trace matches reference 100% bit-for-bit | Single-source state machine traces, seeded PRNG logs |
| **Tolerance Band** | Passes within bounded physical or algorithmic margins | RC curve fits, degradation profile comparisons |
| **Error-Code Parity** | Simulation and hardware return identical `wink_status_t` negative codes ([ADR-0001](../../../decisions/core/0001-error-code-sign-convention.md)) | Fault injection test suites per error code |
| **Hardware / HIL Exclusive** | Explicitly untestable in simulation; deferred to HIL test benches | Hard real-time ISRs, SPICE models, multi-core cache coherency |

### 0.3 Solution Taxonomy & 3 Lines of Defense

| Architectural Layer | Solution Type | Responsibility |
|---|---|---|
| 1st Line: Static Gating | **A Constrained Code** | Compile-time and IDE linting blocking unsafe constructs |
| Simulation Engine Base | **B Engine Modeling** | Wasm/Host simulation of time, buses, and scheduling |
| 2nd Line: Dynamic Traps | **C Observability Gates** | Runtime interception of races, overflows, and timeouts |
| 3rd Line: Hardware Sign-off | **Hardware / HIL** | Accommodating unmodeled microarchitectural physics |

### 0.4 Scenario Table of Contents

| ID | Title | Primary Axis | Mechanism Link | Checklist Anchor |
|---|---|---|---|---|
| C1 | Business Causality & State Machines | A | [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) | [`./02-consistency-checklist.md#c1`](./02-consistency-checklist.md#c1) |
| C2 | Virtual Microsecond Logic Timing | B | [`../02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) | [`./02-consistency-checklist.md#c2`](./02-consistency-checklist.md#c2) |
| C3 | Shared State Race Conditions | E | [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) | [`./02-consistency-checklist.md#c3`](./02-consistency-checklist.md#c3) |
| C4 | Critical Sections & Interrupt Preemption | D | [`../02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) | [`./02-consistency-checklist.md#c4`](./02-consistency-checklist.md#c4) |
| C5 | Blocking / Starvation / Watchdogs | E | [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) | [`./02-consistency-checklist.md#c5`](./02-consistency-checklist.md#c5) |
| C6 | Stack / Heap / Memory Safety | F | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | [`./02-consistency-checklist.md#c6`](./02-consistency-checklist.md#c6) |
| C7 | Bus Protocols / CRC / Error Recovery | A | [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) | [`./02-consistency-checklist.md#c7`](./02-consistency-checklist.md#c7) |
| C8 | DMA / Bus Asynchronous Windows | A | [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) | [`./02-consistency-checklist.md#c8`](./02-consistency-checklist.md#c8) |
| C9 | Multi-Core SMP True Concurrency | E | [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) | [`./02-consistency-checklist.md#c9`](./02-consistency-checklist.md#c9) |
| C10 | Fast-Loop ISR (FOC / Hardware Timers) | C | [`../02-mechanisms/09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) | [`./02-consistency-checklist.md#c10`](./02-consistency-checklist.md#c10) |
| C11 | Electrical / Analog Characteristics | F | [`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) | [`./02-consistency-checklist.md#c11`](./02-consistency-checklist.md#c11) |
| C12 | CPU / ABI Instruction Level | F | [`../02-mechanisms/10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md) | [`./02-consistency-checklist.md#c12`](./02-consistency-checklist.md#c12) |
| C13 | Lifecycle / Reset / Boot Sequences | F | [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) | [`./02-consistency-checklist.md#c13`](./02-consistency-checklist.md#c13) |
| C14 | Fast-Forward / Co-Sim Stepping | B | [`../02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) | [`./02-consistency-checklist.md#c14`](./02-consistency-checklist.md#c14) |
| C15 | Host↔Wasm Boundary Integrity | D | [`../02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) | [`./02-consistency-checklist.md#c15`](./02-consistency-checklist.md#c15) |
| C16 | OS Synchronization Primitives | E | [`../02-mechanisms/03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md) | [`./02-consistency-checklist.md#c16`](./02-consistency-checklist.md#c16) |
| C17 | Peripheral Conflicts / Clock Coupling | C | [`../02-mechanisms/09-timer-and-pwm-semantics.md`](../02-mechanisms/09-timer-and-pwm-semantics.md) | [`./02-consistency-checklist.md#c17`](./02-consistency-checklist.md#c17) |
| C18 | Bus Fault State Machines | A | [`../02-mechanisms/06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md) | [`./02-consistency-checklist.md#c18`](./02-consistency-checklist.md#c18) |
| C19 | DMA / Buffer Lifecycles | A | [`../02-mechanisms/08-channel-routing.md`](../02-mechanisms/08-channel-routing.md) | [`./02-consistency-checklist.md#c19`](./02-consistency-checklist.md#c19) |
| C20 | Callback Reentrancy / Bottom-Halves | D | [`../02-mechanisms/04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md) | [`./02-consistency-checklist.md#c20`](./02-consistency-checklist.md#c20) |
| C21 | Time & Counter Wrap-Around | B | [`../02-mechanisms/02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md) | [`./02-consistency-checklist.md#c21`](./02-consistency-checklist.md#c21) |
| C22 | Power / Low-Power / Clock Gating | F | [`../02-mechanisms/07-peripheral-registry.md`](../02-mechanisms/07-peripheral-registry.md) | [`./02-consistency-checklist.md#c22`](./02-consistency-checklist.md#c22) |
| C23 | Persistence / NVS / Flash Wear | F | [`../02-mechanisms/11-accuracy-observation-lifecycle.md`](../02-mechanisms/11-accuracy-observation-lifecycle.md) | [`./02-consistency-checklist.md#c23`](./02-consistency-checklist.md#c23) |
| C24 | Caches / Memory Attributes / DMA RAM | F | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | [`./02-consistency-checklist.md#c24`](./02-consistency-checklist.md#c24) |
| C25 | Floating-Point / Numerics & UB | F | [`../02-mechanisms/05-memory-and-faults.md`](../02-mechanisms/05-memory-and-faults.md) | [`./02-consistency-checklist.md#c25`](./02-consistency-checklist.md#c25) |

---

## 1. Underlying Principles of Simulation Consistency

### 1.1 Core Baseline: Virtual Microsecond Clock SSOT
`s_virtual_us` serves as the sole timebase SSOT ([`02-virtual-clock.md`](../02-mechanisms/02-virtual-clock.md)). Active stepping inside `pal_delay_ms/us` is strictly prohibited to prevent dual-stepping divergence (C14).

### 1.2 Decoupling Control & Physical Domains
3-Tier Co-Simulation: App Control Domain (100% C) $\leftrightarrow$ Platform Sim OS Base $\leftrightarrow$ Plugin Physical Domain ([`01-architecture.md`](../01-overview/01-architecture.md)).

### 1.3 Zero-Yield Synchronous Event-Driven Fast-Forwarding
Pin Event Queues eliminate Asyncify overhead during pulse measurement in HEADLESS runs.

---

## 2. Scenario Consistency Specifications (C1~C25)

<a id="c1"></a>

### C1 — Business Causality & State Machine Logic

<a id="c1.1"></a>

#### C1.1 Single-Source App/BAL State Transitions
- **Problem**: Simulation running simplified logic masks physical branch divergences.
- **Hardware vs Simulation**: Hardware and simulation must execute single-source C code ([ADR-0002](../../../decisions/unisim/0002-dual-target-compilation.md)).
- **Assurance Solution**: **B** Dual-target single-source compilation; bans `#ifdef SIMULATION` in App logic.
- **Acceptance Oracle**: State transition sequences are **Bit-Identical** under identical input traces.
- **Boundary**: Divergences originating in PAL/DAL bypasses fall under C1.2/C7.

<a id="c1.2"></a>

#### C1.2 DAL Bypass & `#ifdef SIMULATION` Narrowing
- **Problem**: Replacing entire drivers in JS skips CRC, timeouts, and error handling.
- **Hardware vs Simulation**: Hardware executes full drivers; simulation skips protocol stacks if bypassed wholesale.
- **Assurance Solution**: **A+B** Sinks bypasses into PAL; enforces ADR-0040 JSON gates.
- **Acceptance Oracle**: Undeclared bypasses Fail-Loud; declared devices execute C protocol logic.
- **Boundary**: Physical source data stems from plugins without claiming electrical parity.

<a id="c1.3"></a>

#### C1.3 Fault / Timeout / Disconnect Exception Paths
- **Problem**: Happy paths pass while error recovery paths remain unverified.
- **Hardware vs Simulation**: Hardware faults are sporadic; simulation injects faults deterministically.
- **Assurance Solution**: **B** Degradation engine bus packet drops (`drop_permil`) and timeout injections.
- **Acceptance Oracle**: Injected faults transition state machines to designated safe states.
- **Boundary**: Electrical impedance shifts during physical disconnects fall under C11.

<a id="c1.4"></a>

#### C1.4 Idempotent Recovery & Retry Storms
- **Problem**: Error recovery storms duplicate actuator commands.
- **Hardware vs Simulation**: Cooperative scheduling can mask overlapping retry windows.
- **Assurance Solution**: **B+C** Injects ACK latency and verifies actuator command monotonicity.
- **Acceptance Oracle**: Actuator command count $\le$ upper specification bound.
- **Boundary**: Network-level exactly-once semantics are outside OS scope.

<a id="c1.5"></a>

#### C1.5 `wink_status_t` Error-Code Cross-Target Alignment
- **Problem**: Divergent negative error codes returned across targets cause misrouted error handling ([ADR-0001](../../../decisions/core/0001-error-code-sign-convention.md)).
- **Hardware vs Simulation**: Target adapters may map native errors inconsistently.
- **Assurance Solution**: **A+C** Unified fault $\rightarrow$ `wink_status_t` mapping table and unit tests.
- **Acceptance Oracle**: **Error-Code Parity**: Identical negative status codes returned across targets for identical faults.
- **Boundary**: Unmapped third-party SDK error codes must not leak into App/BAL contracts.

---

<a id="c2"></a>

### C2 — Virtual Microsecond Logic Timing

<a id="c2.1"></a>

#### C2.1 Sleep / Wakeup Fast-Forwarding
- **Problem**: Wallclock dependencies cause CI jitter and browser tab throttling.
- **Hardware vs Simulation**: Hardware relies on wallclocks; simulation relies on `s_virtual_us`.
- **Assurance Solution**: **B** Fast-forwards `s_virtual_us` to `next_wakeup_us`.
- **Acceptance Oracle**: Reproducible wakeup timestamps; wallclock execution $\ll$ virtual duration.
- **Boundary**: Crystal ppm drift is intentionally unmodeled.

<a id="c2.2"></a>

#### C2.2 Pulse-In Zero-Yield Loopback
- **Problem**: Stack suspension on `pulse_in` causes massive performance degradation.
- **Hardware vs Simulation**: Hardware busy-waits or uses capture; simulation uses Pin Event Queues.
- **Assurance Solution**: **B** Synchronous event-driven fast-forwarding.
- **Acceptance Oracle**: Pulse measurements return within tolerance with 0 Asyncify yields in HEADLESS mode.
- **Boundary**: Skipping intermediate edges during jumps falls under C14.2.

<a id="c2.3"></a>

#### C2.3 Debouncing & RC Filtering Anchored to Virtual Time
- **Problem**: Wallclock-based filtering produces inconsistent results across machines.
- **Hardware vs Simulation**: Hardware exhibits analog noise; simulation anchors to `s_virtual_us`.
- **Assurance Solution**: **B** `wink_phys_*` routines read virtual timestamps exclusively.
- **Acceptance Oracle**: Filtered outputs match golden trace vectors.
- **Boundary**: Noise amplitudes are synthetic injection parameters.

<a id="c2.4"></a>

#### C2.4 Sampling Periods under Single Interrupts
- **Problem**: Assumed periodic execution disrupted by scheduling jitter.
- **Hardware vs Simulation**: Hardware experiences preemption jitter; cooperative simulation is deterministic.
- **Assurance Solution**: **B** Virtual periodicity assertions; **C** Injected timing jitter.
- **Acceptance Oracle**: Nominal period error is 0 under virtual time.
- **Boundary**: Hard real-time microsecond periodic guarantees fall under C10.

<a id="c2.5"></a>

#### C2.5 Cross-Host Integer & State Determinism
- **Problem**: Identical seeds drift across different JS/Wasm runtimes.
- **Hardware vs Simulation**: Simulators must ensure cross-host determinism.
- **Assurance Solution**: **B+C** HEADLESS golden runs with fixed seeds and registration orders.
- **Acceptance Oracle**: **Bit-Identical** state transitions across host platforms.
- **Boundary**: Canvas interpolation frame rates are excluded from golden traces.

---

<a id="c3"></a>

### C3 — Shared State Race Conditions

<a id="c3.1"></a>

#### C3.1 Lock-Free Shared Reads & Writes (Task↔Task)
- **Problem**: Cooperative round-robin scheduling masks concurrency hazards.
- **Hardware vs Simulation**: Multi-core hardware crashes on unsynchronized access.
- **Assurance Solution**: **B** Chaotic PRNG scheduling; **C** Shadow memory TSan.
- **Acceptance Oracle**: Known race conditions trigger Faults in chaotic mode; disappear with critical sections.
- **Boundary**: True multi-core overlapping writes fall under C9.

<a id="c3.2"></a>

#### C3.2 Task↔ISR Unsynchronized Sharing
- **Problem**: Multi-byte state half-updated when interrupted by an ISR.
- **Hardware vs Simulation**: Hardware interrupts at arbitrary instructions; simulation polls at yield points.
- **Assurance Solution**: **B** Interspersed ISR insertion points; **C** TSan ISR context tracking.
- **Acceptance Oracle**: Unprotected multi-byte structures trigger race condition traps.
- **Boundary**: Arbitrary instruction-level preemption is unmodeled.

<a id="c3.3"></a>

#### C3.3 Compound Struct Tearing
- **Problem**: Multi-field structs read partially updated.
- **Assurance Solution**: **C** Struct version tracking; **A** Mandates seqlocks or critical sections.
- **Acceptance Oracle**: Torn reads trigger Faults.
- **Boundary**: Lock-free ABA hazards are not fully guaranteed.

<a id="c3.4"></a>

#### C3.4 Publish-Subscribe Ordering Assumptions
- **Problem**: Flag updated before payload without memory barriers.
- **Assurance Solution**: **A** Bans bare flags in protocols; **C** TSan inspections.
- **Acceptance Oracle**: Lint checks block bare flag synchronization.
- **Boundary**: Full hardware memory consistency models fall under C12/C24.

---

<a id="c4"></a>

### C4 — Critical Sections & Interrupt Preemption

<a id="c4.1"></a>

#### C4.1 Critical Section Guardrails (Enter / Exit)
- **Problem**: Mismatched enter/exit calls or sleeping within critical sections.
- **Assurance Solution**: **B+C** State machine assertions on `pal_os_critical_enter/exit`.
- **Acceptance Oracle**: Unpaired exit triggers Fault; pending IRQs deferred until unlock.
- **Boundary**: Does not replicate all hardware side effects of global interrupt disabling.

<a id="c4.2"></a>

#### C4.2 Polling Model Interrupt Dispatch Points
- **Problem**: Assuming preemption occurs at arbitrary execution cycles.
- **Assurance Solution**: **B** Documents cooperative polling at tick and yield boundaries.
- **Acceptance Oracle**: ISRs dispatch at designated yield points.
- **Boundary**: Arbitrary instruction preemption is untestable.

<a id="c4.3"></a>

#### C4.3 Priority Nesting
- **Assurance Solution**: **Hardware / HIL Exclusive**.
- **Acceptance Oracle**: Nested priority test cases execute on physical hardware.
- **Boundary**: Simulation models single-level non-nested interrupts only.

<a id="c4.4"></a>

#### C4.4 Illegal API Invocations inside ISRs (FromISR)
- **Problem**: Calling blocking or lock-acquiring APIs inside interrupt routines.
- **Assurance Solution**: **A** ISR-safe API whitelisting; **C** Runtime context checking.
- **Acceptance Oracle**: Calling `mutex_lock` inside an ISR triggers a Fault.
- **Boundary**: Closed-source third-party libraries require manual annotation.

<a id="c4.5"></a>

#### C4.5 Pending Interrupt Queue Overflows
- **Problem**: Burst edge events exceed queue capacity (`PAL_WASM_INTERRUPT_QUEUE_SIZE = 16`).
- **Assurance Solution**: **C** Fail-Loud queue overflow counters and warnings.
- **Acceptance Oracle**: Overflowing events increment observable counters or trigger faults.
- **Boundary**: Physical hardware interrupt controller state registers are unmodeled.

---

<a id="c5"></a>

### C5 — Blocking / Starvation / Watchdogs

<a id="c5.1"></a>

#### C5.1 STRICT_NONBLOCKING Compile-Time Symbol Hiding
- **Problem**: App inadvertently calls blocking APIs, triggering hardware WDT resets.
- **Assurance Solution**: **A** `-DWINK_STRICT_NONBLOCKING=1` hides `WINK_BLOCKING` headers.
- **Acceptance Oracle**: Invoking blocking functions fails at link time with undefined references.
- **Boundary**: Bringup and selftest utilities are isolated behind non-strict builds.

<a id="c5.2"></a>

#### C5.2 Soft Watchdog (Virtual Time Starvation)
- **Problem**: Application task infinite loop starving watchdog feeding.
- **Assurance Solution**: **B+C** Virtual time watchdog triggers Fault if un-fed for $> N$ virtual ms.
- **Acceptance Oracle**: Starved tasks trigger watchdog Faults within the deadline.
- **Boundary**: Does not replace physical CPU execution timeslice guards (WCET 8002).

<a id="c5.3"></a>

#### C5.3 Ready Task Starvation
- **Problem**: Un-yielding tasks starving lower priority threads.
- **Assurance Solution**: **C** Starvation latency counters; **B** Bounded fairness scheduling.
- **Acceptance Oracle**: Starvation test cases log warnings or trigger Faults.
- **Boundary**: Does not replicate true FreeRTOS preemptive timeslicing.

<a id="c5.4"></a>

#### C5.4 Dynamic Indirect Blocking
- **Problem**: Blocking calls occurring via function pointers escaping static analysis.
- **Assurance Solution**: **C** Runtime entry assertions on blocking APIs.
- **Acceptance Oracle**: Indirect blocking invocations trapped by runtime gates.
- **Boundary**: Raw assembly calls cannot be guaranteed.

<a id="c5.5"></a>

#### C5.5 Priority Inversion
- **Assurance Solution**: **Hardware / HIL Exclusive**.
- **Acceptance Oracle**: Verified on hardware test benches.
- **Boundary**: Priority inheritance protocols are unmodeled in simulation.

---

<a id="c6"></a>

### C6 — Stack / Heap / Memory Safety

<a id="c6.1"></a>

#### C6.1 Static Heap Quota Exhaustion
- **Problem**: Massive browser memory masking embedded memory leaks.
- **Assurance Solution**: **B** Fixed heap capping (256 KiB default); returns `WINK_ERR_NO_MEM` (-13).
- **Acceptance Oracle**: OOM scenarios trigger fault isolation with status code parity.
- **Boundary**: Heap fragmentation layout differs from physical microcontrollers.

<a id="c6.2"></a>

#### C6.2 Heap Fragmentation
- **Problem**: Repeated allocation cycles leading to allocation failures despite sufficient total heap.
- **Assurance Solution**: **B** Heap stress modes; **C** Injected allocation failures.
- **Acceptance Oracle**: Stress modes exercise error handling paths.
- **Boundary**: Does not mirror ESP-IDF heap geometry.

<a id="c6.3"></a>

#### C6.3 ASan / UBSan (UAF / Out-of-Bounds / Alignment)
- **Problem**: Wild pointers and buffer overflows failing silently.
- **Assurance Solution**: **C** ASan + UBSan enabled on host and CI test runs.
- **Acceptance Oracle**: Memory corruptions trapped and logged by sanitizers.
- **Boundary**: Full ASan runs may be restricted on daily Wasm browser sessions due to overhead.

<a id="c6.4"></a>

#### C6.4 App Layer Ban on Raw Malloc
- **Problem**: Direct dynamic allocations in business logic violating memory models.
- **Assurance Solution**: **A** Static lint rules banning raw `malloc` in App sources.
- **Acceptance Oracle**: Lint checks fail on violations.
- **Boundary**: Kernel and driver static object pools are exempted.

<a id="c6.5"></a>

#### C6.5 Per-Task Stack Overflow
- **Problem**: Small task stack configurations overflowing.
- **Assurance Solution**: **C** Fiber stack watermarks and `STACK_OVERFLOW_CHECK=2`.
- **Acceptance Oracle**: Deep recursion tests trigger stack overflow traps.
- **Boundary**: Byte-level alignment with Xtensa stack red zones is unmodeled.

<a id="c6.6"></a>

#### C6.6 Buffer Mutation during Asynchronous Transfers
- **Problem**: Mutating transfer buffers before transmission completes.
- **Assurance Solution**: Refer to C19.
- **Acceptance Oracle**: Mutating active buffers triggers Faults or data corruption assertions.
- **Boundary**: Requires asynchronous transfer windows from C8.

---

<a id="c7"></a>

### C7 — Bus Protocols / CRC / Error Recovery

<a id="c7.1"></a>

#### C7.1 Single-Source Protocol Framing & CRC
- **Problem**: Bypassing CRC checks skips bad packet error recovery verification.
- **Assurance Solution**: **B** Single-source DAL drivers; bad CRC injection.
- **Acceptance Oracle**: Corrupted CRC packets trigger recovery paths.
- **Boundary**: Physical electrical error waveforms fall under C11/C18.

<a id="c7.2"></a>

#### C7.2 ACK Timeouts & Retries
- **Problem**: Timeout thresholds diverging between simulation and hardware.
- **Assurance Solution**: **B** Virtual time timeouts; `drop_permil` packet loss injection.
- **Acceptance Oracle**: Fixed PRNG seeds yield reproducible retry counts and error codes.
- **Boundary**: Cable propagation delays are unmodeled.

<a id="c7.3"></a>

#### C7.3 JSON Semantic Simulation Gates
- **Problem**: Undeclared peripheral instances utilizing semantic shortcuts.
- **Assurance Solution**: **A** Enforces ADR-0040 schema gates.
- **Acceptance Oracle**: Undeclared peripherals Fail-Loud.
- **Boundary**: Gates validate declaration existence, not electrical correctness.

<a id="c7.4"></a>

#### C7.4 Zero Legacy Bypass Audit
- **Problem**: Legacy drivers retaining full `#ifdef SIMULATION` branches.
- **Assurance Solution**: **A+C** CI static scanning for unauthorized simulation macros.
- **Acceptance Oracle**: Audit logs report 0 undeclared bypasses.
- **Boundary**: Physical data source hooks in PAL are permitted.

---

<a id="c8"></a>

### C8 — DMA / Bus Asynchronous Windows

<a id="c8.1"></a>

#### C8.1 Coarse-Grained Transfer Duration Yields
- **Problem**: Synchronous 0µs transfers masking buffer concurrency races.
- **Assurance Solution**: **B** Suspends tasks based on byte transfer times, waking via completion IRQs.
- **Acceptance Oracle**: Concurrent buffer mutations during transfer windows are trapped.
- **Boundary**: Bit-level cycle-accurate timing is unmodeled.

<a id="c8.2"></a>

#### C8.2 Completion Interrupts & Task Wakeup Ordering
- **Problem**: Fragile assumptions regarding callback execution versus task resumption order.
- **Assurance Solution**: **B** Sets completion flags prior to resuming waiting tasks.
- **Acceptance Oracle**: Ordering validated under deterministic and chaotic scheduling.
- **Boundary**: Hardware DMA interrupt priority nuances may differ.

<a id="c8.3"></a>

#### C8.3 Synchronous API Deprecation
- **Problem**: Retaining blocking bus transfers creates C8 gaps.
- **Assurance Solution**: **B** Bus driver migration roadmaps; **A** Annotates synchronous symbols.
- **Acceptance Oracle**: Target buses expose asynchronous models or explicit synchronous annotations.
- **Boundary**: Transitory exemptions are documented.

---

<a id="c9"></a>

### C9 — Multi-Core SMP True Concurrency

<a id="c9.1"></a>

#### C9.1 Single Virtual Core Scope
- **Assurance Solution**: **Hardware / HIL Exclusive** ([ADR-0014](../../../decisions/unisim/0014-sim-single-virtual-core.md)).
- **Acceptance Oracle**: No false claims of multi-core simulation.
- **Boundary**: Dual-core execution is verified exclusively on real hardware.

<a id="c9.2"></a>

#### C9.2 Chaotic Scheduling Interleaving Approximation
- **Problem**: High-frequency interleaving can expose certain race conditions.
- **Assurance Solution**: **B** Chaotic PRNG scheduling; physical multi-core regression suites.
- **Acceptance Oracle**: Concurrency tests pass on hardware.
- **Boundary**: Cache coherency and inter-core interrupt routing are unmodeled.

---

<a id="c10"></a>

### C10 — Fast-Loop ISR (FOC / Hardware Timers)

<a id="c10.1"></a>

#### C10.1 Virtual Time Soft-Stepping Approximation
- **Problem**: Wasm lacks 20kHz hard real-time interrupt capabilities.
- **Assurance Solution**: **B** Deterministic soft-stepping based on [ADR-0047](../../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md).
- **Acceptance Oracle**: Fixed virtual time steps produce reproducible control state vectors.
- **Boundary**: Does not replicate 50µs hard real-time execution.

<a id="c10.2"></a>

#### C10.2 PWM–ADC Hardware Synchronization Degradation
- **Problem**: Hardware-triggered ADC sampling degraded in simulation.
- **Assurance Solution**: **B** Documents synchronous plant sampling at step boundaries.
- **Acceptance Oracle**: Phase-critical algorithms are assigned to HIL suites.
- **Boundary**: Cycle-accurate triggering is Hardware/HIL exclusive.

<a id="c10.3"></a>

#### C10.3 Layering Boundaries for Fast Loops
- **Problem**: Business logic embedded into fast loops harming testability.
- **Assurance Solution**: **A** Enforces ADR-0047 separation (BAL `control/` pure math; DAL hardware blocks).
- **Acceptance Oracle**: Lint checks block invalid PAL calls inside control loops.
- **Boundary**: Whitelist evolves with product scope.

---

<a id="c11"></a>

### C11 — Electrical / Analog Characteristics

<a id="c11.1"></a>

#### C11.1 SPICE & Power Integrity
- **Assurance Solution**: **Hardware / HIL Exclusive**.
- **Acceptance Oracle**: Excluded from simulation commitments.
- **Boundary**: Intentionally unmodeled.

<a id="c11.2"></a>

#### C11.2 Degradation Engine Tabular Models
- **Problem**: Idealized sensors producing brittle PID tuning parameters.
- **Assurance Solution**: **B** Injects jitter, noise, warmup delays, and drops ([`06-physical-degradation.md`](../02-mechanisms/06-physical-degradation.md)).
- **Acceptance Oracle**: Injected parameters shift sensor distributions reproducibly.
- **Boundary**: Empirical models rather than SPICE physics.

<a id="c11.3"></a>

#### C11.3 ADC Quantization, Reference Voltages & Parasitic Capacitance
- **Assurance Solution**: **Hardware / HIL Exclusive**.
- **Acceptance Oracle**: Certified analog accuracy validated on hardware.
- **Boundary**: High-precision analog characteristics are unmodeled.

---

<a id="c12"></a>

### C12 — CPU / ABI Instruction Level

<a id="c12.1"></a>

#### C12.1 Daily Fast-Track Builds (Wasm / Host Native)
- **Assurance Solution**: **B** Fast functional iterations accepting Wasm ABI abstractions.
- **Acceptance Oracle**: Unit tests provide sub-second developer feedback.
- **Boundary**: Non-instruction-accurate.

<a id="c12.2"></a>

#### C12.2 Nightly Dual-Track Builds (RISC-V / Hardware Binaries)
- **Problem**: Struct alignment, padding, and calling convention bugs passing in Wasm but failing on hardware.
- **Assurance Solution**: **C** Nightly cross-compiled binary comparisons.
- **Acceptance Oracle**: Padding-sensitive unit tests trap ABI differences.
- **Boundary**: Slower execution; excluded from daily PR runs.

<a id="c12.3"></a>

#### C12.3 Struct Packing, Enums & Bitfields
- **Assurance Solution**: **A** Packing conventions + `_Static_assert` checks; **C** Dual builds.
- **Acceptance Oracle**: `_Static_assert(sizeof(...))` validates struct layouts across targets.
- **Boundary**: Bitfield layouts remain compiler-dependent.

<a id="c12.4"></a>

#### C12.4 Endianness & Unaligned Memory Access
- **Assurance Solution**: **C** Host UBSan; explicit protocol serialization helpers.
- **Acceptance Oracle**: Unaligned accesses trapped by UBSan on host builds.
- **Boundary**: Wasm and Xtensa architectures handle unaligned access differently.

---

<a id="c13"></a>

### C13 — Lifecycle / Reset / Boot Sequences

<a id="c13.1"></a>

#### C13.1 Cold Boot: BSS, Static Defaults & Pin States
- **Problem**: Hot-reusing worker instances retaining dirty static state across runs.
- **Hardware vs Simulation**: Hardware boots from pristine power-on states.
- **Assurance Solution**: **B** Initialization enforces `pal_wasm_reset_physical()` and scheduler resets.
- **Acceptance Oracle**: Successive initializations match cold-boot state vectors.
- **Boundary**: Hardware strapping pin states are unmodeled.

<a id="c13.2"></a>

#### C13.2 Soft Reboot & Reset Reason Codes
- **Problem**: Control branches depending on `esp_reset_reason` values.
- **Assurance Solution**: **B** Injects reset reasons (Brownout, Watchdog).
- **Acceptance Oracle**: Injected reset codes route to proper recovery state machines.
- **Boundary**: Physical power rail drop waveforms are unmodeled.

<a id="c13.3"></a>

#### C13.3 Peripheral De-initialization & Re-initialization
- **Problem**: Re-initializing without de-initializing leaking handles and ISR registrations.
- **Assurance Solution**: **C** Duplicate registration assertions and resource accounting.
- **Acceptance Oracle**: Re-initialization behaves idempotently or reports Fail-Loud errors per API contract.
- **Boundary**: Idempotency must be explicitly declared in the API contract.

<a id="c13.4"></a>

#### C13.4 Task Lifecycles & Zombie GC
- **Problem**: Accessing task stacks or queues after `task_delete`.
- **Assurance Solution**: **B** Main loop GC transitions state `ZOMBIE` $\rightarrow$ `TERMINATED`; **C** UAF sanitizers.
- **Acceptance Oracle**: Post-deletion access triggers ASan traps or Faults.
- **Boundary**: Nuances with asynchronous FreeRTOS deletion require compatibility mappings.

<a id="c13.5"></a>

#### C13.5 Long-Duration Soak Resource Accounting
- **Problem**: Long-running loops leaking handles or accumulating heap watermarks.
- **Hardware vs Simulation**: Hardware eventually panics from OOM; large simulation heaps mask leaks.
- **Assurance Solution**: **C** Soak tests: Asserts handle counts and heap watermarks return to baseline after $N$ cycles.
- **Acceptance Oracle**: Handle count returns to initial baseline; net heap growth is $\le$ threshold.
- **Boundary**: Intentionally cached objects must declare steady-state ownership.

---

<a id="c14"></a>

### C14 — Fast-Forward / Co-Sim Stepping

<a id="c14.1"></a>

#### C14.1 Clock Single Gate Rule & Banning Dual Stepping
- **Problem**: Both delay functions and workers advancing clocks, corrupting timebases.
- **Assurance Solution**: **A+B** Enforces single `wink_vclock_advance_internal()` Gate.
- **Acceptance Oracle**: Secondary writers fail assertions.
- **Boundary**: HEADLESS internal scheduler jumps are documented legitimate callers.

<a id="c14.2"></a>

#### C14.2 Fast-Forwarding across Intermediate Edges & Debounce Windows
- **Problem**: Jumping directly to `next_wakeup` skips intermediate GPIO edges.
- **Assurance Solution**: **B** Drains pending pin events prior to fast-forwarding jumps.
- **Acceptance Oracle**: Intermediate edges fire reliably during fast-forwarded intervals.
- **Boundary**: Queue overflow policies fall under C4.5/C14.4.

<a id="c14.3"></a>

#### C14.3 Plant $\leftrightarrow$ OS Lockstep Drift
- **Problem**: JavaScript physics engines reading wallclocks rather than `s_virtual_us`.
- **Assurance Solution**: **B** Step-Lock: Shared `virtual_dt` advances OS and plant synchronously.
- **Acceptance Oracle**: Plant physics reproducible across runs; wallclock reads fail tests.
- **Boundary**: Visual interpolation is permitted but cannot alter control decisions.

<a id="c14.4"></a>

#### C14.4 Pin Event Queue Capacity
- **Problem**: High-density callbacks overflowing event queues.
- **Assurance Solution**: **C** Queue full triggers Faults; **B** Configurable queue depth.
- **Acceptance Oracle**: Queue overflows are observable and never fail silently.
- **Boundary**: Buffer depths are independent of microcontroller hardware FIFOs.

<a id="c14.5"></a>

#### C14.5 Observation & Injection Ordering
- **Problem**: Plant writing inputs and reading GPIO outputs without synchronization barriers.
- **Assurance Solution**: **B** Enforces step ordering: Read Controls $\rightarrow$ Update Plant $\rightarrow$ Inject Inputs.
- **Acceptance Oracle**: Violations of execution ordering fail test fixtures.
- **Boundary**: Multi-worker setups require explicit cross-thread barriers.

---

<a id="c15"></a>

### C15 — Host↔Wasm Boundary Integrity

<a id="c15.1"></a>

#### C15.1 Asyncify Stack Suspension Contracts
- **Problem**: `sleep` failing to return Promises or missing `__async: 'auto'` metadata causing infinite rewind loops.
- **Assurance Solution**: **A** JS library wrapper patterns; **C** State assertions on invalid Asyncify states.
- **Acceptance Oracle**: Malformed import overrides fail CI; valid imports suspend/resume cleanly.
- **Boundary**: Covers declared repository imports only.

<a id="c15.2"></a>

#### C15.2 Banning Reentrant Interrupt Pushes
- **Problem**: Reentrant push interrupts corrupting suspended Asyncify stacks.
- **Assurance Solution**: **B** Eliminates `_trigger_wasm_interrupt`; uses queue polling exclusively ([`04-interrupt-model.md`](../02-mechanisms/04-interrupt-model.md)).
- **Acceptance Oracle**: Static symbols confirm push methods are absent; reentrant tests execute safely.
- **Boundary**: Polling latencies fall under C4.2.

<a id="c15.3"></a>

#### C15.3 BigInt & Pointer ABI Integrity
- **Problem**: JavaScript numbers truncating uint64 timestamps or corrupting pointers.
- **Assurance Solution**: **A** `-s WASM_BIGINT=1`; TypeScript enforces `bigint` across timing APIs ([`10-wasm-js-bridge-abi.md`](../02-mechanisms/10-wasm-js-bridge-abi.md)).
- **Acceptance Oracle**: 64-bit timestamps round-trip without loss of precision.
- **Boundary**: Third-party JavaScript plugins must adhere to the same type contracts.

<a id="c15.4"></a>

#### C15.4 Semantic Bypass Leakage
- **Problem**: Workbench UI or tests calling internal bypasses, dodging schema gates.
- **Assurance Solution**: **A** Enforces ADR-0040; **C** Runtime probes.
- **Acceptance Oracle**: Unauthorized bypass invocations fail immediately.
- **Boundary**: Manual memory edits via debuggers are excluded.

<a id="c15.5"></a>

#### C15.5 Web Worker Isolation & Main Thread Starvation
- **Problem**: Running Wasm + Asyncify on the main UI thread starving timers and causing OOM panics.
- **Assurance Solution**: **A** Enforces hosting Wasm inside dedicated Web Workers ([`01-sandbox-and-execution.md`](../02-mechanisms/01-sandbox-and-execution.md)).
- **Acceptance Oracle**: Architectural lint rules block main-thread execution paths.
- **Boundary**: Node test harnesses follow specific worker_threads rules.

---

<a id="c16"></a>

### C16 — OS Synchronization Primitives

<a id="c16.1"></a>

#### C16.1 Mutex Timeouts & Recursion Policies
- **Problem**: Status codes and recursion semantics diverging from FreeRTOS.
- **Assurance Solution**: **B** Semantic parity matrices ([`03-scheduler-and-concurrency.md`](../02-mechanisms/03-scheduler-and-concurrency.md)); unit tests for `timeout_fired`.
- **Acceptance Oracle**: Unit tests assert identical return codes across targets.
- **Boundary**: Priority inheritance falls under C5.5.

<a id="c16.2"></a>

#### C16.2 Queue / Ringbuf Full & Overflow Policies
- **Problem**: Divergent drop-oldest versus rejection policies causing silent data loss.
- **Assurance Solution**: **B** Frozen API contracts; **C** Full/empty telemetry counters.
- **Acceptance Oracle**: Overflowing queues match contractual return codes and drop behaviors.
- **Boundary**: Zero-copy DMA ring buffers fall under C19.

<a id="c16.3"></a>

#### C16.3 Contested Timeouts & Event Wakeup Ordering
- **Problem**: Undefined resolution order when events and timeouts expire simultaneously.
- **Assurance Solution**: **B** Freezes priority resolution order (events take precedence over timeouts).
- **Acceptance Oracle**: Simultaneous expirations produce deterministic outcomes.
- **Boundary**: Intentional hardware divergences must be explicitly documented.

<a id="c16.4"></a>

#### C16.4 Task Notifications & Event Groups
- **Problem**: Auto-clearing bits and multi-bit wait semantics prone to subtle bugs.
- **Assurance Solution**: **B** Formal unit tests if exposed; **A** Otherwise hidden behind experimental flags.
- **Acceptance Oracle**: Exposed primitives carry complete semantic test suites.

<a id="c16.5"></a>

#### C16.5 Deadlock Detection
- **Problem**: Task A holding Lock 1 waiting for Lock 2 while Task B holds Lock 2 waiting for Lock 1.
- **Assurance Solution**: **C** Wait-for-graph cycle detection and timeout assertions; **A** Lock ordering rules.
- **Acceptance Oracle**: Deadlock fixtures trigger timeouts or explicit Faults.
- **Boundary**: Does not constitute a formal mathematical proof of deadlock-freedom.

---

<a id="c17"></a>

### C17 — Peripheral Conflicts / Clock Coupling

<a id="c17.1"></a>

#### C17.1 Pin Multiplexing (Pin-Mux) Conflicts
- **Problem**: Two peripheral instances claiming identical GPIO pins.
- **Assurance Solution**: **A** JSON device tree conflict checks; registration-time assertions.
- **Acceptance Oracle**: Conflicting declarations Fail-Loud during code generation and startup.
- **Boundary**: Dynamic runtime pin remuxing requires dedicated API contracts.

<a id="c17.2"></a>

#### C17.2 Timer / PWM Channel Allocation Conflicts
- **Problem**: Hardware timer instance claimed concurrently by PWM and input capture.
- **Assurance Solution**: **A** Resource allocation tables; **C** Duplicate acquisition Faults.
- **Acceptance Oracle**: Dual-allocation fixtures report configuration errors.
- **Boundary**: Hardware-specific shared modes require explicit whitelist annotations.

<a id="c17.3"></a>

#### C17.3 APB / Bus Frequency Side Effects
- **Problem**: Modifying CPU frequency silently altering UART baud rates or PWM base clocks.
- **Assurance Solution**: **Hardware / HIL Exclusive**.
- **Acceptance Oracle**: Verified on physical hardware benches.
- **Boundary**: Dynamic clock tree reconfiguration is unmodeled.

<a id="c17.4"></a>

#### C17.4 PWM Duty Update Glitches & Phase Continuity
- **Problem**: Duty cycle modifications generating transient glitches.
- **Assurance Solution**: **B** Optional next-cycle boundary application models.
- **Acceptance Oracle**: Updates align with cycle boundaries where modeled.
- **Boundary**: Electrical glitch waveforms fall under C11.

<a id="c17.5"></a>

#### C17.5 Shared Bus Contention
- **Problem**: Multiple tasks issuing concurrent `i2c_transfer` calls without locks.
- **Assurance Solution**: **A** Bus locking rules; **C** Overlapping transfer window detection.
- **Acceptance Oracle**: Unsynchronized concurrent transfers trigger contention Faults.
- **Boundary**: Multi-master arbitration falls under C18.

---

<a id="c18"></a>

### C18 — Bus Fault State Machines

<a id="c18.1"></a>

#### C18.1 I2C NACK / Clock Stretching / Bus Lockup
- **Problem**: Testing packet loss while neglecting NACKs and SCL line lockups.
- **Assurance Solution**: **B** Injects NACKs and clock-stretch timeouts; tests recovery logic.
- **Acceptance Oracle**: Injected faults match expected error codes; bus recovery sequences succeed.
- **Boundary**: Electrical pull-up rise times are unmodeled.

<a id="c18.2"></a>

#### C18.2 UART Framing Errors / Break Signals / FIFO Overflows
- **Problem**: Frame corruptions and FIFO overflows passing unhandled.
- **Assurance Solution**: **B** Injects framing errors and RX overflows ([ADR-0054](../../../decisions/unisim/0054-sim-uart-async-rx-model-boundary.md)).
- **Acceptance Oracle**: Overflows trigger proper driver error and discard policies.
- **Boundary**: Bit sampling phase alignment is unmodeled.

<a id="c18.3"></a>

#### C18.3 SPI Modes / Chip-Select Timing / Full-Duplex
- **Problem**: Incorrect CPOL/CPHA clock polarity or chip-select edge assumptions.
- **Assurance Solution**: **B** Models SPI mode parameters; mismatched modes fail transactions.
- **Acceptance Oracle**: Invalid mode configurations fail; valid modes match golden traces.
- **Boundary**: Physical board trace propagation delays are unmodeled.

<a id="c18.4"></a>

#### C18.4 Transaction Aborts & Partial Frame Recovery
- **Problem**: Aborting transfers leaving state machines locked in partial frames.
- **Assurance Solution**: **B** Abort APIs with single-source recovery logic.
- **Acceptance Oracle**: Subsequent transactions succeed cleanly following aborts.
- **Boundary**: Hardware DMA abort subtleties overlap with C19.

<a id="c18.5"></a>

#### C18.5 Multi-Master Bus Arbitration
- **Assurance Solution**: **Hardware / HIL Exclusive**.
- **Acceptance Oracle**: Multi-master collisions verified on hardware benches.
- **Boundary**: Complex multi-master arbitration waveforms are unmodeled.

---

<a id="c19"></a>

### C19 — DMA / Buffer Lifecycles

<a id="c19.1"></a>

#### C19.1 Half-Transfer & Double-Buffering
- **Problem**: Modeling full completion while omitting half-transfer callback paths.
- **Assurance Solution**: **B** Optional half-completion events; double-buffer ownership assertions.
- **Acceptance Oracle**: Callback invocations and buffer indices match specifications.
- **Boundary**: Coarse-grained timing without cycle-accurate microstepping.

<a id="c19.2"></a>

#### C19.2 Buffer Mutation during Active Transfers
- **Problem**: CPU modifying buffer contents while DMA hardware is actively transmitting.
- **Assurance Solution**: **C** Active transfer window write detection; **A** Buffer lifecycle rules.
- **Acceptance Oracle**: Writes during active transmission windows trigger Faults.
- **Boundary**: Requires asynchronous transfer windows from C8.

<a id="c19.3"></a>

#### C19.3 Stale Descriptors & Double-Frees following Aborts
- **Problem**: Aborts leaving stale descriptors or causing double-free panics.
- **Assurance Solution**: **B** State machine cleanup; **C** Double-free sanitizers.
- **Acceptance Oracle**: Re-initialization succeeds cleanly following an abort.
- **Boundary**: Hardware descriptor link-list formats are unmodeled.

<a id="c19.4"></a>

#### C19.4 DMA-Accessible Memory Partitioning
- **Problem**: Placing DMA buffers in internal SRAM regions inaccessible to DMA controllers.
- **Assurance Solution**: **A** Memory partition tags; **Hardware Verification**.
- **Acceptance Oracle**: Allocations in invalid memory regions fail.
- **Boundary**: Physical microcontroller cache/DMA RAM layouts are verified on hardware.

---

<a id="c20"></a>

### C20 — Callback Reentrancy / Bottom-Halves

<a id="c20.1"></a>

#### C20.1 Invoking Yielding DAL APIs inside ISRs
- **Problem**: Synchronously polling sensors inside interrupt callbacks causing deadlocks.
- **Assurance Solution**: **A** Callback context rules; **C** Context checks trapping blocking calls.
- **Acceptance Oracle**: Illegal blocking calls inside ISRs trigger Faults.
- **Boundary**: Overlaps with C4.4.

<a id="c20.2"></a>

#### C20.2 Nested Sensor Callbacks Mutating Actuators
- **Problem**: Input callbacks driving output actuators directly, causing deep recursion.
- **Assurance Solution**: **A** Recommends queue-based decoupling; **C** Call depth assertions.
- **Acceptance Oracle**: Excessive recursion depths emit warnings.
- **Boundary**: Synchronous control is not forbidden but must document recursion risks.

<a id="c20.3"></a>

#### C20.3 Deferred Workqueue Execution Ordering
- **Problem**: Relying on unverified workqueue execution priorities.
- **Assurance Solution**: **A** Freezes execution ordering if deferred primitives are exposed.
- **Acceptance Oracle**: Execution order is verified via unit tests.
- **Boundary**: Does not replicate Linux softirq semantics.

---

<a id="c21"></a>

### C21 — Time & Counter Wrap-Around

<a id="c21.1"></a>

#### C21.1 uint32 Millisecond / Tick Rollover
- **Problem**: Using signed subtraction for `now - last`, corrupting timeouts on rollover.
- **Assurance Solution**: **A** Coding guidelines enforcing unsigned subtraction; **C** Fast-forward rollover tests.
- **Acceptance Oracle**: Timeouts function identically across rollover boundaries.
- **Boundary**: Tests Application-level uint32 ticks despite uint64 `s_virtual_us` kernel storage.

<a id="c21.2"></a>

#### C21.2 Relative Timeouts Spanning Fast-Forward Jumps
- **Problem**: Relative deadlines miscomputed when crossing fast-forward intervals.
- **Assurance Solution**: **A** Enforces absolute deadlines (`wakeup_us`).
- **Acceptance Oracle**: Expired tasks wake up deterministically following jumps.
- **Boundary**: Overlaps with C14.

<a id="c21.3"></a>

#### C21.3 Sequence Number & Ring Buffer Index Rollover
- **Problem**: Index calculations wrapping incorrectly across power-of-two boundaries.
- **Assurance Solution**: **C** Dedicated boundary unit tests with fuzz testing seeds.
- **Acceptance Oracle**: Push and pop operations remain consistent across rollovers.
- **Boundary**: Application custom sequence counters require dedicated tests.

<a id="c21.4"></a>

#### C21.4 Unit Conversion Overflow & Truncation
- **Problem**: Multiplying `ms * 1000` overflowing uint32 ranges.
- **Assurance Solution**: **A** Audited conversion APIs; **C** Boundary value test suites.
- **Acceptance Oracle**: Overflowing inputs Fail-Loud or saturate per specification; valid ranges convert reversibly.
- **Boundary**: Crystal ppm drift is unmodeled.

---

<a id="c22"></a>

### C22 — Power / Low-Power / Clock Gating

<a id="c22.1"></a>

#### C22.1 Light / Deep Sleep Wakeup
- **Assurance Solution**: **Hardware / HIL Exclusive**.
- **Acceptance Oracle**: Sleep/wake power cycles validated on physical hardware.
- **Boundary**: Current consumption curves and wake transient waveforms are unmodeled.

<a id="c22.2"></a>

#### C22.2 Peripheral Clock Gating
- **Assurance Solution**: **Hardware / HIL Exclusive**.
- **Acceptance Oracle**: Gated clocks validated on physical hardware.
- **Boundary**: Clock distribution trees are unmodeled.

<a id="c22.3"></a>

#### C22.3 Brownout & Undervoltage Resets
- **Assurance Solution**: **Hardware / HIL Exclusive** (Reset reason injection covered in C13.2).
- **Acceptance Oracle**: Brownout recovery validated on physical hardware.
- **Boundary**: Analog supply voltage decay waveforms are unmodeled.

---

<a id="c23"></a>

### C23 — Persistence / NVS / Flash Wear

<a id="c23.1"></a>

#### C23.1 Power Loss Write Tearing
- **Problem**: Sudden power failure during flash write corrupting stored images.
- **Assurance Solution**: **B** Injects simulated power cuts during writes; tests dual-copy recovery.
- **Acceptance Oracle**: Damaged writes trigger fallback recovery to defaults.
- **Boundary**: Physical flash cell endurance wear is unmodeled.

<a id="c23.2"></a>

#### C23.2 Key Space Exhaustion & Out-of-Storage
- **Assurance Solution**: **B** Storage quota limits; **C** Returns out-of-memory errors on exhaustion.
- **Acceptance Oracle**: Full-storage return codes match physical target specifications.
- **Boundary**: Wear leveling algorithms are unmodeled.

<a id="c23.3"></a>

#### C23.3 Simulation Power-Down Semantics
- **Problem**: Browser page reloads clearing non-volatile storage unexpectedly.
- **Assurance Solution**: **A** Documents worker destruction semantics versus snapshot persistence.
- **Acceptance Oracle**: Snapshot tests validate storage round-trips.
- **Boundary**: Browser IndexedDB storage quotas are governed separately.

---

<a id="c24"></a>

### C24 — Caches / Memory Attributes / DMA RAM

<a id="c24.1"></a>

#### C24.1 DMA Buffer Memory Placement
- **Assurance Solution**: **A** Memory allocation region tags; **Hardware Verification**.
- **Acceptance Oracle**: Buffers allocated in non-DMA memory fail validation checks.
- **Boundary**: Physical memory mapping is verified on hardware.

<a id="c24.2"></a>

#### C24.2 Cache Coherency Assumptions
- **Assurance Solution**: **Hardware / HIL Exclusive**.
- **Acceptance Oracle**: Hardware regression suites validate cache flushing.
- **Boundary**: Cache line write-back coherency is unmodeled.

<a id="c24.3"></a>

#### C24.3 IRAM / Flash Execution Latency Placement
- **Assurance Solution**: **Hardware / HIL Exclusive**.
- **Acceptance Oracle**: Linker scripts and execution performance validated on physical targets.
- **Boundary**: Fast-path cache miss penalties are unmodeled.

---

<a id="c25"></a>

### C25 — Floating-Point / Numerics & UB

<a id="c25.1"></a>

#### C25.1 Signed Integer Overflow & Shift Undefined Behavior
- **Problem**: Signed overflow and out-of-range bit shifts triggering C undefined behavior.
- **Hardware vs Simulation**: Wasm wraps two's complement deterministically, while compiler optimizations (-O2) on Xtensa may eliminate checks.
- **Assurance Solution**: **C** UBSan on host builds; **A** Strict coding guidelines.
- **Acceptance Oracle**: Undefined behaviors trapped by UBSan on host test runs.
- **Boundary**: Wasm and Xtensa overflow behaviors may still diverge.

<a id="c25.2"></a>

#### C25.2 NaN / Infinity Propagation & Flush-to-Zero
- **Problem**: Division by zero generating NaNs that propagate silently through control loops.
- **Hardware vs Simulation**: Wasm preserves IEEE-754 NaNs, whereas microcontroller hardware FPUs may configure flush-to-zero (FTZ).
- **Assurance Solution**: **B** Control loop NaN assertion unit tests; hardware FPU mode comparisons.
- **Acceptance Oracle**: Injected NaNs trigger safe states or report explicit errors.
- **Boundary**: Hardware FTZ rounding modes are not emulated cycle-accurately.

<a id="c25.3"></a>

#### C25.3 Floating-Point Determinism
- **Problem**: Cross-platform FPU reductions causing trajectory drift ([ADR-0055](../../../decisions/unisim/0055-sim-fp-determinism-and-golden-policy.md)).
- **Hardware vs Simulation**: Differences in libm implementations, FMA contractions, and rounding modes prevent bit-exact cross-target parity.
- **Assurance Solution**: **B** Golden traces evaluated against defined Tolerance Bands (`fp_mode=tolerance`).
- **Acceptance Oracle**: Computations evaluate within the declared tolerance envelope.
- **Boundary**: Bit-for-bit identity across disparate hardware targets is never guaranteed.

---

## 3. Governance & Lifecycle Index

Milestones, CI verification tiers, and maintenance procedures reside in [`03-roadmap-and-governance.md`](./03-roadmap-and-governance.md).
