# Simulation Consistency Scenario Checklist (Status Matrix)

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/04-assurance/02-consistency-checklist.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Document Level | ① Design Specification (UniSim 3.0 / assurance) |
| Document Status | **Active** (Switched 2026-08-02; Active Wasm simulation SSOT) |
| SSOT Responsibility | **"Can scenario X be verified right now?"**; Sole source of truth for ✅/🟡/❌/🚫/**—** statuses |
| Last Audit | 2026-08-02 |
| Governing ADRs | Status update procedures in [`03`](./03-roadmap-and-governance.md); Scenario contracts in [`01`](./01-consistency-spec.md) |
| Migrated From | `04-wasm-simulation-2.0/12-consistency-checklist.md` |

> This document serves as the index for [`01`](./01-consistency-spec.md): storing **active status, residual gaps, verification entry points, and pointers to scenario contracts**. Technical specifications are not duplicated here.

---

## 0. Reading Conventions

### 0.1 Support Levels

| Symbol | Meaning |
|---|---|
| ✅ **Supported** | Core scenario verified; known constraints listed under "Residual Gaps" |
| 🟡 **Partially Supported** | Subset covered or approximated; potential hardware escape |
| ❌ **Unsupported** | Currently untestable; planned for subsequent phases or hardware |
| 🚫 **Intentionally Unmodeled** | Outside product boundaries; approximate or omitted |
| — **N/A** | Feature path unexposed in current product |

### 0.2 Verification Entry Column (Evidence Pointer, not Specification Body)

| Value | Meaning |
|---|---|
| Command / Test ID | Reproducible entry point (e.g., `python wink-tools/wink.py test ...`, lint rule, test name); **does not** duplicate assertions |
| `Pending` | Entry point unlinked. **Promoting to ✅ forbids** leaving `Pending` (see [`03` §4](./03-roadmap-and-governance.md)). Legacy ✅ marked `Pending` are considered **evidence debt** and must not claim audited proof |
| `N/A` | Primary status is 🚫 or — |

### 0.3 Solution Types (Category Overview; Details in `01`)

| Type | Meaning |
|---|---|
| **A** | Static / build constraint (lint / compile-time / JSON gate) |
| **B** | Engine modeling |
| **C** | Observability gate |
| **Hardware** | HIL / real hardware fallback |

### 0.4 Document Pointers

- Five-field template and C1~C25 contract body $\rightarrow$ [`01`](./01-consistency-spec.md)
- Production criteria (Completeness $\ne$ Identity) $\rightarrow$ [`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md)
- Mechanism landing maturity $\rightarrow$ Root [`00-README.md` §3.2](../00-README.md), [`03` §1.1](./03-roadmap-and-governance.md)
- Phase / CI / ✅ promotion / golden governance $\rightarrow$ [`03`](./03-roadmap-and-governance.md)

---

## 1. High-Level Matrix (C1~C25)

| ID | Category | Status | Primary Solution | Phase / Scope | Priority |
|---|---|---|---|---|---|
| [C1](#c1) | Business Causality & State Machines | ✅ | B (+A) | Baseline | High |
| [C2](#c2) | Virtual Microsecond Logic Timing | ✅ | B | Phase 1 | High |
| [C3](#c3) | Shared State Race Conditions | ❌ | B+C | Phase 4 | **Highest** |
| [C4](#c4) | Critical Sections & Interrupt Preemption | 🟡 | B+C | Phase 4 | **Highest** |
| [C5](#c5) | Blocking / Starvation / Watchdogs | 🟡 | A+B+C | Phase 4 Prerequisite | High |
| [C6](#c6) | Stack / Heap / Memory Safety | ✅ | A+C | Phase 2 | High |
| [C7](#c7) | Bus Protocols / CRC | 🟡 | B+A | Phase 3 | Medium-High |
| [C8](#c8) | DMA / Asynchronous Transfer Windows | ❌ | B | Phase 3 | Medium (High for drivers) |
| [C9](#c9) | Multi-Core SMP | ❌ | B Approx / Hardware | ADR-0014 | High |
| [C10](#c10) | Fast-Loop ISR / FOC | 🟡 | B Approx / HIL | ADR-0047 | Medium-High |
| [C11](#c11) | Electrical / Analog | 🚫 | B Tabular Approx | ADR-0003 | Context-dependent |
| [C12](#c12) | CPU / ABI Instruction Level | ❌ | C Dual-Track | Phase 5 | Low (Difficult to debug) |
| [C13](#c13) | Lifecycle / Reset | 🟡 | B+C | Phase 1 Polish | **Highest** |
| [C14](#c14) | Fast-Forward / Co-Sim Stepping | 🟡 | B+C | Phase 1+ | **Highest** |
| [C15](#c15) | Host↔Wasm Boundary | 🟡 | A+C | Baseline | **Highest** |
| [C16](#c16) | OS Synchronization Primitives | 🟡 | B+A | Phase 4 Prerequisite | **Highest** |
| [C17](#c17) | Peripheral Conflicts / Clocks | 🟡 | A+C | Continuous | High |
| [C18](#c18) | Bus Fault State Machines | ❌ | B | Phase 3 Expansion | Medium-High |
| [C19](#c19) | DMA / Buffer Lifecycles | ❌ | B+C | Phase 3 Expansion | Medium (High for drivers) |
| [C20](#c20) | Callback Reentrancy / Bottom-Halves | 🟡 | A+C | Phase 4 | High |
| [C21](#c21) | Time & Counter Wrap-Around | 🟡 | A+C | Continuous | High |
| [C22](#c22) | Power / Low-Power / Clock Gating | 🚫 | Hardware | Mostly Non-Goal | Medium |
| [C23](#c23) | Persistence / NVS | 🟡 | B | As-needed | Medium |
| [C24](#c24) | Cache / DMA RAM | 🚫 | Hardware / C12 | Mostly Non-Goal | Medium |
| [C25](#c25) | Floating-Point / Numerical UB | 🟡 | C | Phase 2 | Medium |

---

## 2. Granular Sub-Scenario Matrix (102 Sub-Scenarios)

<a id="c1"></a>

### C1 — Business Causality & State Machines

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C1.1 Single-Source App/BAL State Transitions | ✅ | — | Pending | [01 §C1.1](./01-consistency-spec.md#c1.1) |
| C1.2 DAL Bypass & `#ifdef SIMULATION` Narrowing | 🟡 | Legacy driver `#ifdef`s (Ultrasonic deprecated shortcut) | Pending | [01 §C1.2](./01-consistency-spec.md#c1.2) |
| C1.3 Fault / Timeout / Disconnect Exception Paths | ✅ | Coverage expands per device | Pending | [01 §C1.3](./01-consistency-spec.md#c1.3) |
| C1.4 Idempotent Recovery & Retry Storms | 🟡 | Lacks unified command count oracles | Pending | [01 §C1.4](./01-consistency-spec.md#c1.4) |
| C1.5 `wink_status_t` Error-Code Alignment | 🟡 | Complete mapping table unit tests pending | Pending | [01 §C1.5](./01-consistency-spec.md#c1.5) |

<a id="c2"></a>

### C2 — Virtual Microsecond Logic Timing

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C2.1 Sleep / Wakeup Fast-Forwarding | ✅ | Crystal ppm drift unmodeled | Pending | [01 §C2.1](./01-consistency-spec.md#c2.1) |
| C2.2 Pulse-In Zero-Yield Loopback | ✅ | Intersects with C14.2 | Pending | [01 §C2.2](./01-consistency-spec.md#c2.2) |
| C2.3 Debouncing & RC Filtering Anchored to Clock | ✅ | Injected synthetic noise | Pending | [01 §C2.3](./01-consistency-spec.md#c2.3) |
| C2.4 Sampling Periods under Single Interrupts | 🟡 | Injected jitter disabled by default | Pending | [01 §C2.4](./01-consistency-spec.md#c2.4) |
| C2.5 Cross-Host Integer & State Determinism | ❌ | Cross-browser golden matrix pending | Pending | [01 §C2.5](./01-consistency-spec.md#c2.5) |

<a id="c3"></a>

### C3 — Shared State Race Conditions

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C3.1 Lock-Free Shared Reads & Writes (Task↔Task) | ❌ | Chaotic scheduler + TSan pending | Pending | [01 §C3.1](./01-consistency-spec.md#c3.1) |
| C3.2 Task↔ISR Unsynchronized Sharing | ❌ | Multi-point ISR insertion pending | Pending | [01 §C3.2](./01-consistency-spec.md#c3.2) |
| C3.3 Compound Struct Tearing | ❌ | Field-level shadow memory pending | Pending | [01 §C3.3](./01-consistency-spec.md#c3.3) |
| C3.4 Publish-Subscribe Ordering Assumptions | ❌ | Weak memory models excluded from daily tests | Pending | [01 §C3.4](./01-consistency-spec.md#c3.4) |

<a id="c4"></a>

### C4 — Critical Sections & Interrupt Preemption

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C4.1 Critical Section Guardrails (Enter / Exit) | 🟡 | Deeper state assertions (Nest count unlock landed) | Pending | [01 §C4.1](./01-consistency-spec.md#c4.1) |
| C4.2 Polling Model Interrupt Dispatch Points | 🟡 | Non-arbitrary instruction insertion | Pending | [01 §C4.2](./01-consistency-spec.md#c4.2) |
| C4.3 Priority Nesting | ❌ | Phase 4+ / Physical hardware | Pending | [01 §C4.3](./01-consistency-spec.md#c4.3) |
| C4.4 Illegal API Invocations inside ISRs (FromISR) | ❌ | Context checking traps pending Fault conversion | Pending | [01 §C4.4](./01-consistency-spec.md#c4.4) |
| C4.5 Pending Interrupt Queue Overflows | 🟡 | Fail-Loud overflow warnings pending enhancement | Pending | [01 §C4.5](./01-consistency-spec.md#c4.5) |

<a id="c5"></a>

### C5 — Blocking / Starvation / Watchdogs

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C5.1 STRICT_NONBLOCKING Symbol Hiding | ✅ | Indirect invocations covered in C5.4 | Pending (`-DWINK_STRICT_NONBLOCKING=1`) | [01 §C5.1](./01-consistency-spec.md#c5.1) |
| C5.2 Soft Watchdog (Virtual Time Starvation) | ❌ | Unimplemented | Pending | [01 §C5.2](./01-consistency-spec.md#c5.2) |
| C5.3 Ready Task Starvation | ❌ | Round-robin scheduling is overly fair | Pending | [01 §C5.3](./01-consistency-spec.md#c5.3) |
| C5.4 Dynamic Indirect Blocking | 🟡 | Runtime gates partial | Pending | [01 §C5.4](./01-consistency-spec.md#c5.4) |
| C5.5 Priority Inversion | ❌ | Priority inheritance unpromised | Pending | [01 §C5.5](./01-consistency-spec.md#c5.5) |

<a id="c6"></a>

### C6 — Stack / Heap / Memory Safety

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C6.1 Static Heap Quota Exhaustion | ✅ | Fixed heap linker flags pending CMake integration | Pending | [01 §C6.1](./01-consistency-spec.md#c6.1) |
| C6.2 Heap Fragmentation | 🟡 | Stress modes optional | Pending | [01 §C6.2](./01-consistency-spec.md#c6.2) |
| C6.3 ASan / UBSan (UAF / Out-of-Bounds / Alignment) | ✅ | Excluded from daily browser Wasm runs | `python wink-tools/wink.py test` (ASan Pass) | [01 §C6.3](./01-consistency-spec.md#c6.3) |
| C6.4 App Layer Ban on Raw Malloc | ✅ | — | `python wink-tools/wink.py lint --pack memory` | [01 §C6.4](./01-consistency-spec.md#c6.4) |
| C6.5 Per-Task Stack Overflow | 🟡 | Fiber stack layout differs from FreeRTOS | Pending | [01 §C6.5](./01-consistency-spec.md#c6.5) |
| C6.6 Buffer Mutation during Transfers | ❌ | Depends on C8/C19 | Pending | [01 §C6.6](./01-consistency-spec.md#c6.6) |

<a id="c7"></a>

### C7 — Bus Protocols / CRC

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C7.1 Single-Source Protocol Framing & CRC | 🟡 | Legacy driver bypasses | Pending | [01 §C7.1](./01-consistency-spec.md#c7.1) |
| C7.2 ACK Timeouts & Retries | 🟡 | — | Pending | [01 §C7.2](./01-consistency-spec.md#c7.2) |
| C7.3 JSON Semantic Simulation Gates | ✅ | — | Pending (ADR-0040 JSON Gate) | [01 §C7.3](./01-consistency-spec.md#c7.3) |
| C7.4 Zero Legacy Bypass Audit | 🟡 | Ongoing elimination | Pending | [01 §C7.4](./01-consistency-spec.md#c7.4) |

<a id="c8"></a>

### C8 — DMA / Asynchronous Transfer Windows

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C8.1 Coarse-Grained Transfer Duration Yields | ❌ | Synchronous bus API legacy | Pending | [01 §C8.1](./01-consistency-spec.md#c8.1) |
| C8.2 Completion Interrupts & Task Wakeup Ordering | ❌ | Depends on C8.1 | Pending | [01 §C8.2](./01-consistency-spec.md#c8.2) |
| C8.3 Synchronous API Deprecation | 🟡 | Requires deprecation annotations | Pending | [01 §C8.3](./01-consistency-spec.md#c8.3) |

<a id="c9"></a>

### C9 — Multi-Core SMP

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C9.1 Single Virtual Core Scope | 🚫 | Multi-core unverified in daily runs (ADR-0014) | N/A | [01 §C9.1](./01-consistency-spec.md#c9.1) |
| C9.2 Chaotic Scheduling Interleaving Approximation | ❌ | Phase 4 | Pending | [01 §C9.2](./01-consistency-spec.md#c9.2) |

<a id="c10"></a>

### C10 — Fast-Loop ISR / FOC

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C10.1 Virtual Time Soft-Stepping Approximation | 🟡 | Non-hard-real-time; ADR-0047 | Pending | [01 §C10.1](./01-consistency-spec.md#c10.1) |
| C10.2 PWM–ADC Hardware Sync Degradation | 🚫 | Hardware / HIL Exclusive | N/A | [01 §C10.2](./01-consistency-spec.md#c10.2) |
| C10.3 Layering Boundaries for Fast Loops | ✅ | Fast loop lint whitelist in place | Pending (`wink lint`) | [01 §C10.3](./01-consistency-spec.md#c10.3) |

<a id="c11"></a>

### C11 — Electrical / Analog

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C11.1 SPICE & Power Integrity | 🚫 | — | N/A | [01 §C11.1](./01-consistency-spec.md#c11.1) |
| C11.2 Degradation Engine Tabular Models | ✅ | Tabular empirical parameters | Pending | [01 §C11.2](./01-consistency-spec.md#c11.2) |
| C11.3 ADC Quantization & Capacitance | 🚫 | Certified analog accuracy deferred to hardware | N/A | [01 §C11.3](./01-consistency-spec.md#c11.3) |

<a id="c12"></a>

### C12 — CPU / ABI Instruction Level

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C12.1 Daily Fast-Track Builds | ✅ | Non-instruction-accurate | Pending | [01 §C12.1](./01-consistency-spec.md#c12.1) |
| C12.2 Nightly Dual-Track Builds | ❌ | Phase 5 | Pending | [01 §C12.2](./01-consistency-spec.md#c12.2) |
| C12.3 Struct Packing, Enums & Bitfields | 🟡 | `_Static_assert` active; bitfields compiler-dependent | Pending | [01 §C12.3](./01-consistency-spec.md#c12.3) |
| C12.4 Endianness & Unaligned Memory Access | 🟡 | Host UBSan active | Pending | [01 §C12.4](./01-consistency-spec.md#c12.4) |

<a id="c13"></a>

### C13 — Lifecycle / Reset

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C13.1 Cold Boot: Defaults & Pin States | 🟡 | `pal_wasm_reset_physical` implemented; test harness pending | Pending | [01 §C13.1](./01-consistency-spec.md#c13.1) |
| C13.2 Soft Reboot & Reset Reason Codes | ❌ | Pending injection models | Pending | [01 §C13.2](./01-consistency-spec.md#c13.2) |
| C13.3 Peripheral De-init & Re-init | 🟡 | Idempotency contracts require normalization | Pending | [01 §C13.3](./01-consistency-spec.md#c13.3) |
| C13.4 Task Lifecycles & Zombie GC | 🟡 | GC active; relies on Sanitizers to catch UAF | Pending | [01 §C13.4](./01-consistency-spec.md#c13.4) |
| C13.5 Long-Duration Soak Accounting | ❌ | Soak test benchmarks pending | Pending | [01 §C13.5](./01-consistency-spec.md#c13.5) |

<a id="c14"></a>

### C14 — Fast-Forward / Co-Sim Stepping

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C14.1 Clock Single Gate Rule | 🟡 | Single Gate active (ADR-0042); CI assertions pending | Pending | [01 §C14.1](./01-consistency-spec.md#c14.1) |
| C14.2 Fast-Forwarding across Edges | 🟡 | Global minimum event interval pending | Pending | [01 §C14.2](./01-consistency-spec.md#c14.2) |
| C14.3 Plant $\leftrightarrow$ OS Lockstep Drift | 🟡 | Wallclock access ban pending enforcement | Pending | [01 §C14.3](./01-consistency-spec.md#c14.3) |
| C14.4 Pin Event Queue Capacity | 🟡 | Fail-Loud overflow assertions pending | Pending | [01 §C14.4](./01-consistency-spec.md#c14.4) |
| C14.5 Observation & Injection Ordering | 🟡 | Step ordering contracts pending test harness | Pending | [01 §C14.5](./01-consistency-spec.md#c14.5) |

<a id="c15"></a>

### C15 — Host↔Wasm Boundary

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C15.1 Asyncify Stack Suspension Contracts | 🟡 | Wrapper `'auto'` active; comprehensive regression suite pending | Pending | [01 §C15.1](./01-consistency-spec.md#c15.1) |
| C15.2 Banning Reentrant Interrupt Pushes | ✅ | Push exports permanently removed | Pending (Symbol audit) | [01 §C15.2](./01-consistency-spec.md#c15.2) |
| C15.3 BigInt & Pointer ABI Integrity | ✅ | Third-party plugin type risks | Pending | [01 §C15.3](./01-consistency-spec.md#c15.3) |
| C15.4 Semantic Bypass Leakage | 🟡 | Runtime probes require enhancement | Pending | [01 §C15.4](./01-consistency-spec.md#c15.4) |
| C15.5 Web Worker Isolation | ✅ | — | Pending | [01 §C15.5](./01-consistency-spec.md#c15.5) |

<a id="c16"></a>

### C16 — OS Synchronization Primitives

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C16.1 Mutex Timeouts & Recursion Policies | 🟡 | Compatibility table test coverage partial | Pending | [01 §C16.1](./01-consistency-spec.md#c16.1) |
| C16.2 Queue / Ringbuf Full Policies | 🟡 | Detailed return code parity | Pending | [01 §C16.2](./01-consistency-spec.md#c16.2) |
| C16.3 Contested Timeouts & Expiration Order | 🟡 | Tie-breaking resolution requires test fixtures | Pending | [01 §C16.3](./01-consistency-spec.md#c16.3) |
| C16.4 Task Notifications & Event Groups | — | N/A until exposed | N/A | [01 §C16.4](./01-consistency-spec.md#c16.4) |
| C16.5 Deadlock Detection | ❌ | Optional gate | Pending | [01 §C16.5](./01-consistency-spec.md#c16.5) |

<a id="c17"></a>

### C17 — Peripheral Conflicts / Clocks

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C17.1 Pin Multiplexing (Pin-Mux) Conflicts | 🟡 | Code generation checks require strengthening | Pending | [01 §C17.1](./01-consistency-spec.md#c17.1) |
| C17.2 Timer / PWM Channel Allocation Conflicts | 🟡 | Resource table incomplete | Pending | [01 §C17.2](./01-consistency-spec.md#c17.2) |
| C17.3 APB / Bus Frequency Side Effects | 🚫 | Hardware / HIL Exclusive | N/A | [01 §C17.3](./01-consistency-spec.md#c17.3) |
| C17.4 PWM Duty Update Glitches | 🟡 | Next-cycle application models optional | Pending | [01 §C17.4](./01-consistency-spec.md#c17.4) |
| C17.5 Shared Bus Contention | 🟡 | Overlapping transfer window detection | Pending | [01 §C17.5](./01-consistency-spec.md#c17.5) |

<a id="c18"></a>

### C18 — Bus Fault State Machines

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C18.1 I2C NACK / Clock Stretching / Lockup | ❌ | Phase 3 Expansion | Pending | [01 §C18.1](./01-consistency-spec.md#c18.1) |
| C18.2 UART Framing Errors & FIFO Overflows | ❌ | Phase 3 Expansion | Pending | [01 §C18.2](./01-consistency-spec.md#c18.2) |
| C18.3 SPI Modes / CS Timing / Full-Duplex | ❌ | Phase 3 Expansion (Mode parameters active) | Pending | [01 §C18.3](./01-consistency-spec.md#c18.3) |
| C18.4 Transaction Aborts & Partial Frames | ❌ | Inherits broad category | Pending | [01 §C18.4](./01-consistency-spec.md#c18.4) |
| C18.5 Multi-Master Bus Arbitration | 🚫 | Mostly unmodeled in products | N/A | [01 §C18.5](./01-consistency-spec.md#c18.5) |

<a id="c19"></a>

### C19 — DMA / Buffer Lifecycles

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C19.1 Half-Transfer & Double-Buffering | ❌ | Depends on C8 | Pending | [01 §C19.1](./01-consistency-spec.md#c19.1) |
| C19.2 Buffer Mutation during Active Transfers | ❌ | Depends on C8 | Pending | [01 §C19.2](./01-consistency-spec.md#c19.2) |
| C19.3 Stale Descriptors following Aborts | ❌ | Inherits broad category | Pending | [01 §C19.3](./01-consistency-spec.md#c19.3) |
| C19.4 DMA-Accessible Memory Partitioning | 🟡 | Tag checks and hardware validation | Pending | [01 §C19.4](./01-consistency-spec.md#c19.4) |

<a id="c20"></a>

### C20 — Callback Reentrancy / Bottom-Halves

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C20.1 Invoking Yielding DAL APIs inside ISRs | 🟡 | Context checks pending Fault conversion | Pending | [01 §C20.1](./01-consistency-spec.md#c20.1) |
| C20.2 Nested Sensor Callbacks Mutating Actuators | 🟡 | Queue decoupling rules | Pending | [01 §C20.2](./01-consistency-spec.md#c20.2) |
| C20.3 Deferred Workqueue Execution Ordering | — | N/A until exposed | N/A | [01 §C20.3](./01-consistency-spec.md#c20.3) |

<a id="c21"></a>

### C21 — Time & Counter Wrap-Around

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C21.1 uint32 Millisecond / Tick Rollover | 🟡 | Dedicated fast-forward test cases pending | Pending | [01 §C21.1](./01-consistency-spec.md#c21.1) |
| C21.2 Relative Timeouts Spanning Jumps | 🟡 | Absolute deadlines active; tests pending | Pending | [01 §C21.2](./01-consistency-spec.md#c21.2) |
| C21.3 Sequence Number & Ring Buffer Rollover | 🟡 | Inherits broad category | Pending | [01 §C21.3](./01-consistency-spec.md#c21.3) |
| C21.4 Unit Conversion Overflow & Truncation | ❌ | Boundary value test suites pending | Pending | [01 §C21.4](./01-consistency-spec.md#c21.4) |

<a id="c22"></a>

### C22 — Power / Low-Power / Clock Gating

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C22.1 Light / Deep Sleep Wakeup | 🚫 | Minimal stubs optional; hardware verified | N/A | [01 §C22.1](./01-consistency-spec.md#c22.1) |
| C22.2 Peripheral Clock Gating | 🚫 | — | N/A | [01 §C22.2](./01-consistency-spec.md#c22.2) |
| C22.3 Brownout & Undervoltage Resets | 🚫 | Reset reason injection covered in C13.2 | N/A | [01 §C22.3](./01-consistency-spec.md#c22.3) |

<a id="c23"></a>

### C23 — Persistence / NVS

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C23.1 Power Loss Write Tearing | 🟡 | As-needed injection | Pending | [01 §C23.1](./01-consistency-spec.md#c23.1) |
| C23.2 Key Space Exhaustion & Out-of-Storage | 🟡 | Inherits broad category | Pending | [01 §C23.2](./01-consistency-spec.md#c23.2) |
| C23.3 Simulation Power-Down Semantics | 🟡 | Worker destruction = Power-down | Pending | [01 §C23.3](./01-consistency-spec.md#c23.3) |

<a id="c24"></a>

### C24 — Cache / DMA RAM

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C24.1 DMA Buffer Memory Placement | 🚫 | Tag checks in C19.4; hardware verified | N/A | [01 §C24.1](./01-consistency-spec.md#c24.1) |
| C24.2 Cache Coherency Assumptions | 🚫 | Hardware verification | N/A | [01 §C24.2](./01-consistency-spec.md#c24.2) |
| C24.3 IRAM / Flash Execution Latency Placement | 🟡 | Linker script differences documented | Pending | [01 §C24.3](./01-consistency-spec.md#c24.3) |

<a id="c25"></a>

### C25 — Floating-Point / Numerical UB

| Sub-scenario | Status | Residual Gaps | Verification Entry | Contract |
|---|---|---|---|---|
| C25.1 Signed Integer Overflow & Shift UB | 🟡 | Host UBSan active | Pending | [01 §C25.1](./01-consistency-spec.md#c25.1) |
| C25.2 NaN / Infinity Propagation & FTZ | 🟡 | Control loop NaN assertions | Pending | [01 §C25.2](./01-consistency-spec.md#c25.2) |
| C25.3 Floating-Point Determinism | 🟡 | Tolerance Band evaluation (ADR-0055) | Pending | [01 §C25.3](./01-consistency-spec.md#c25.3) |

---

## 3. Recommended Reading Order

1. [C14](#c14) + [C15](#c15) — Fast-Forward / Lockstep & Host Boundaries
2. [C13](#c13) + [C16](#c16) — Cold Boot & OS Semantics
3. [C5](#c5) — Soft WDT / Starvation (C5.2/C5.3)
4. [C8](#c8) + [C18](#c18)/[C19](#c19) — Asynchronous DMA, Bus Faults & Buffers
5. [C3](#c3) + [C4](#c4) — Race Conditions, Critical Sections & Interrupts
6. [C17](#c17) — Pin & Timer Resource Contention
7. [C7](#c7) — Bypass Elimination Audits (C7.4)
8. [C12](#c12) — Nightly Dual-Track ABI Builds
9. [C9](#c9)/[C10](#c10)/[C11](#c11)/[C22](#c22)/[C24](#c24) — Hardware / HIL / 🚫 Scope Boundaries

---

## 4. Explicitly Excluded Scopes

Official boundaries defined in [`../01-overview/03-production-contract.md`](../01-overview/03-production-contract.md). Scenarios marked 🚫 or "Hardware / HIL Exclusive" **must never** cite simulation test passes as release sign-offs.

---

## 5. Maintenance Procedures

1. **Status Updates**: Modify **only this document** when updating scenario support statuses, residual gaps, or verification entry points.
2. **Contract Updates**: Modify **only** [`01`](./01-consistency-spec.md) when updating problem definitions or oracles.
3. **New Scenarios**: Document all 5 fields in `01` first, then add an entry here.
4. **Promoting to ✅**: Requires non-empty verification entry points.
