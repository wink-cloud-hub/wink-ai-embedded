# 4.5 Wasm Simulation Consistency & Fidelity Specification

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/archive/05-simulation-consistency-and-fidelity-spec.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Creation Date | 2026-07-31 |
| Document Tier | ① Design Specification (`04-wasm-simulation/`) |
| Status | **Active** |
| Related Docs | [08-simulation-consistency-checklist.md](./08-simulation-consistency-checklist.md), [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md), [ADR-0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md), [ADR-0013](../../../decisions/unisim/0013-sim-cooperative-scheduler.md), [ADR-0014](../../../decisions/unisim/0014-sim-single-virtual-core.md), [ADR-0019](../../../decisions/unisim/0019-wasm-imports-override-and-asyncify-syntax.md), [ADR-0025](../../../decisions/core/0025-app-blocking-api-honesty-pragma-convention.md), [ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md), [ADR-0042](../../../decisions/unisim/0042-sim-execution-modes.md), [ADR-0045](../../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md), [ADR-0047](../../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md), [01-wasm-sandbox-lifecycle.md](./01-wasm-sandbox-lifecycle.md), [06-physical-degradation-engine.md](./06-physical-degradation-engine.md), [07-scheduler-model.md](./07-scheduler-model.md) |

> **Positioning & SSOT Division of Responsibilities**:
> This specification serves as the **Single Source of Truth (SSOT) for WinkOS Wasm simulation core consistency principles, scenario assurance mechanisms, and engine implementation designs**.
> - **For underlying consistency principles, scenario mechanisms, and technical solutions** $\rightarrow$ Read this specification.
> - **To check the current testable status of specific scenarios (✅/🟡/❌/🚫)** $\rightarrow$ Consult [08 Simulation Consistency Checklist](./08-simulation-consistency-checklist.md) (the scenario readability index SSOT).

---

## 0. Responsibilities & Interface Boundaries

| Document | Core Question Answered | Permitted Scope | Prohibited Duplication |
|---|---|---|---|
| **This Spec (05)** | What are the fundamental principles of simulation consistency? What are the problems, solutions, and oracles for C1–C25 categories and sub-scenarios? | Virtual clock/decoupling principles, C1–C25 category + sub-scenario contracts, Phase technical designs | Category readability status matrices (✅/🟡/❌/🚫 owned exclusively by 08) |
| **[08 Checklist](./08-simulation-consistency-checklist.md)** | Can this scenario be tested right now? What is the primary solution type? | C1–C25 and sub-scenario testability matrices, solution types A/B/C, residual gaps, back-links to 05 | Repeating full sub-scenario technical narrative, repeating engine mechanisms, maintaining parallel task tables |

### 0.1 Sub-Scenario Document Template (Mandatory 5 Fields per Sub-Scenario)

| Field | Meaning |
|---|---|
| **Problem** | Common physical hardware pitfalls; what App/driver patterns trigger them |
| **Hardware vs Simulation** | Discrepancy mechanism (why simulation escapes or produces false positives) |
| **Assurance Solution** | A Specification / B Engine Modeling / C Observation Gates / Hardware Sign-off; key implementation details |
| **Acceptance Oracle** | Assertions defining what constitutes "verified" (measurable test oracle) |
| **Boundary** | Domains intentionally unmodeled or approximated |

#### Standard Acceptance Oracle Vocabulary Compilation

| Standard Term | Agreed Meaning | Typical Assertion Mechanism |
|---|---|---|
| **Fail-Fast / Fail-Loud** | Compile-time or boot-time immediate blocking with explicit errors | `-DWINK_STRICT_NONBLOCKING=1` compilation failure, JSON gate blocking |
| **Fault** | Runtime triggering and capture of Wasm/Host kernel fault containment zones | Heap quota OOM (`-13`), soft WDT timeout, shadow memory TSan concurrency assertions |
| **Bit-Identical / Golden Trace** | Execution traces or outputs matching reference baselines 100% bit-for-bit / microsecond-for-microsecond | Homologous state machine transition trace diffs, fixed PRNG packet drop/debounce outputs |
| **Tolerance Band** | Accepted within a predefined physical/algorithmic tolerance envelope (e.g. $\pm 2\%$) | RC low-pass filter curve fitting, sensor degradation curve matching |
| **Hardware / HIL Exclusive** | Marked untestable in simulation; strictly delegated to physical hardware or HIL test suites | 20kHz hard real-time ISRs, SPICE electrical dynamics, multi-core cache coherency |

Testability markers (✅/🟡/❌/🚫) **are written only in 08**; this specification describes mechanisms and solutions.

### 0.2 Solution Types & Three Lines of Defense Mapping (Shared Vocabulary with 08)

The "Assurance Solution" for each sub-scenario is composed of three defense lines cooperating with core simulation modeling:

| Architectural Tier / Defense Line | Solution Type | Core Responsibility | Typical Mechanism (WinkOS Toolchain & Kernel) |
|---|:---:|---|---|
| **1st Line: Static Gating** | **A. Constraint** | Compile-time or IDE-time blocking of unsafe code patterns, preventing illegal API leakage | `wink-tools/tools/lint` rules, `-DWINK_STRICT_NONBLOCKING=1` hidden blocking symbols, `wink-app.json` static pin-conflict gating |
| **Simulation Core Base** | **B. Engine Modeling** | Faithfully modeling physical time, bus transfers, and scheduling behavior in Wasm/Host kernels | `s_virtual_us` virtual microsecond clock SSOT, Zero-Yield pin event queues, deterministic chaotic scheduler (PRNG), 100% homologous DAL drivers |
| **2nd Line: Dynamic Traps** | **C. Observation Gate** | Capturing and isolating dynamic concurrency, overflow, exhaustion, and timeout anomalies at runtime | Static heap quota Fault (`-13`), shadow memory (TSan) race assertions, soft WDT timeouts, ASan / UBSan sanitizers |
| **3rd Line: Hardware Sign-off** | **Hardware / HIL** | Validating physical, hard real-time, and microarchitectural characteristics intentionally unmodeled in simulation | Tier 3 HIL automated board test pipelines, 20kHz hard real-time ISRs, SPICE electrical simulations, multi-core cache sampling |

### 0.3 Category Overview (C1～C25)

| ID | Category | Product Importance | Primary Phase / Scope |
|---|---|---|---|
| C1 | Business Causality / State Machines | High | Baseline |
| C2 | Virtual Microsecond Logic Timing | High | Phase 1 |
| C3 | Shared State Race Conditions | **Highest** | Phase 4 |
| C4 | Critical Sections & Interrupt Preemption / Nesting | **Highest** | Phase 4 |
| C5 | Blocking / Starvation / Watchdogs | High | Phase 4 Precursor |
| C6 | Stack / Heap / Memory Safety | High | Phase 2 |
| C7 | Bus Protocols / CRC / Error Recovery | Medium-High | Phase 3 |
| C8 | DMA / Bus Asynchronous Windows | Medium (Drivers: High) | Phase 3 |
| C9 | Multi-Core SMP True Concurrency | High | Unicore approximation; Hardware sign-off |
| C10 | Fast-Loop ISRs (FOC / HW Timers) | Medium-High | ADR-0047; HIL |
| C11 | Electrical / Analog Circuits | Product Dependent | 🚫 Non-goal |
| C12 | CPU / ABI Instruction Level | Low (Hard to debug) | Phase 5 |
| C13 | Lifecycle / Reset / Boot Sequences | **Highest** | Baseline Enhancement |
| C14 | Fast-Forward / Co-Sim Stepping Contracts | **Highest** | Phase 1+ |
| C15 | Host↔Wasm Boundary Integrity | **Highest** | Baseline + Gates |
| C16 | OS Synchronization Primitive Alignment | **Highest** | Phase 4 Precursor |
| C17 | Peripheral Resource Conflicts / Clock Couplings | High | Codegen + Sim assertions |
| C18 | Bus Fault State Machines (Beyond CRC) | Medium-High | Phase 3 Extension |
| C19 | DMA / Buffer Lifecycles | Medium (Drivers: High) | Phase 3 Extension |
| C20 | Callback Reentrancy / Deferred Bottom-Halves | High | A+B |
| C21 | Time & Counter Wrap-Around | High | C+B |
| C22 | Power / Low-Power / Clock Domains | Medium | Mostly 🚫 / Hardware |
| C23 | Persistence / NVS / Wear | Medium | Behavioral subset |
| C24 | Caches / Memory Attributes / DMA RAM | Medium | Leans C12; Hardware |
| C25 | Floating-Point / Numerics & Compiler UB | Medium | C (UBSan) |

### 0.4 Production Scope & Residual Inconsistencies After A–F Completion

The directory [README](./README.md) deconstructs simulation into orthogonal axes **A–F** (Peripheral physical sources / Time base / Timers / Interrupts / Scheduling & concurrency / Fault observation). This section specifies: **Axis completion goals must not be conflated with "virtual-physical identity"** (aligning with [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md)).

#### Production Scope Definition

| Claim | Target After A–F Completion? |
|---|---|
| Simulation serves as a **high-confidence behavioral pre-check** prior to release (catching logic/protocol/resource escapes early in CI and low-code mainlines) | **Yes** — "Production-usable simulation pre-check" |
| Simulation outputs can **substitute physical hardware / HIL alone** as final release sign-off | **No** |
| Simulation is **bit-for-bit / microsecond / preemption-order identical** with hardware in arbitrary scenarios | **No** — Never promised as a product claim |

#### Inconsistencies That Will Inevitably Persist (Model Upper Bounds)

| Origin | Description |
|---|---|
| Electrical / Analog | ADC quantization, impedance, power rails, crystal drift (mostly C11 🚫) |
| Interrupt Model Boundaries | Cooperative IRQ polling $\neq$ NVIC preemptive nesting (C4 cannot prove zero escape in simulation) |
| Timers / Fast-Loop Boundaries | Soft-stepping / virtual clocks $\neq$ silicon hard ISRs or PWM–ADC hardware triggers (C10) |
| Multi-Core / Microarchitecture | Single virtual core; caches, DMA bus contention, silicon errata $\rightarrow$ Hardware sign-off |
| Host Environment | Worker and Asyncify suspension points alter wall-clock perception; logical timing relies on virtual clocks, not wall-clock pacing |

#### Inconsistencies Targeted for Substantial Convergence

- Axis A + Homology Rules: Conversions / timeouts / protocol framing / error recoveries execute realistically in simulation.
- Axis B: Debouncing, timeouts, and periodic tasks reproduce deterministically under `s_virtual_us`.
- Axis C/F (Gating Subset): Pin/timer resource conflicts, illegal blocking, and quota/fault Fail-Loud mechanics.
- Axis E (Cooperative Subset): Starvation, soft WDT, and shared-state bugs exposed earlier.
- Axis F + [08](./08-simulation-consistency-checklist.md): Untestable scenarios explicitly marked 🚫, preventing "green in simulation = safe on hardware" assumptions.

#### Management Principles

Residual simulation $\leftrightarrow$ hardware differences must be **managed by category**:

1. Check [08](./08-simulation-consistency-checklist.md) scenario status;
2. Items marked 🚫 / hardware-exclusive enter HIL or board-level gates;
3. Use ADR-0003 "behavioral high fidelity" terminology consistently across internal and external reviews; claims that "multi-axis implementation eliminates physical hardware testing" are strictly prohibited.

---

## 1. Underlying Simulation Consistency Principles

### 1.1 Core Baseline: Virtual Microsecond Clock SSOT (Virtual-Time Clock)

The WinkOS simulation kernel replaces host wall-clock reliance with a monotonically increasing virtual microsecond clock `s_virtual_us` as the single time SSOT ([ADR-0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md), [06](./06-physical-degradation-engine.md)):

* **Zero-Overhead Seamless Time Jumping (Fast-Forwarding)**: Under HEADLESS mode when all fibers are asleep or awaiting events, `pal_sim_scheduler_run` fast-forwards `s_virtual_us` to the nearest `next_wakeup_us`, allowing lengthy timing sequences to execute in milliseconds.
* **Physical Logic Binding**: Debouncing (`wink_phys_debounce_step`), RC low-pass filtering (`wink_phys_rc_lowpass`), and soft timers anchor to `s_virtual_us`, guaranteeing deterministic reproduction across disparate host machines.
* **SSOT Red Line**: `pal_delay_ms/us` is **forbidden** from advancing clocks directly; the sole write entry point is `pal_wasm_advance_virtual_clock` (with HEADLESS internal single-gate caller, see [ADR-0042](../../../decisions/unisim/0042-sim-execution-modes.md)). Dual stepping represents a C14-level escape.

### 1.2 Co-Simulation with Decoupled Control & Physical Domains (Co-Simulation)

```text
┌────────────────────────────────────────────────────────┐
│               App Control Domain                       │  ◄── 100% C business code (Homologous build)
└───────────────────────────┬────────────────────────────┘
                            ▼
┌────────────────────────────────────────────────────────┐
│               Platform Sim OS                          │  ◄── Domain-neutral
│   (Clock, Scheduler, Virtual Pins, Interrupt Poll, Bus)│
└───────────────────────────┬────────────────────────────┘
                            ▲ (Bidirectional Pin/Bus exchange)
                            ▼
┌────────────────────────────────────────────────────────┐
│             Simulation Plugins (Physics / Devices)     │  ◄── Physics algorithm plugins
│    - Kinematics/dynamics, sensor degradation state etc.│
└────────────────────────────────────────────────────────┘
```

#### 1.2.1 Platform Data Plane
* **Exports**: `pal_wasm_get_gpio_output`, `pal_wasm_get_pwm_duty_percent`, etc.
* **Injections**: `pal_wasm_set_gpio_input`, `pal_wasm_set_ultrasonic_distance`, etc.
* Kernel maintains **zero application-specific physical traces**; robot kinematics and physical algorithms reside strictly in plugins.

#### 1.2.2 Co-Simulation Stepping Contract (Step-Lock Pipe)
1. Plugin reads control signals (PWM, etc.);
2. Updates physical state over $\Delta t$ and computes sensor quantities;
3. Writes back to platform core via injection APIs.  
Step-lock breaks (plant out of sync with OS clock) are governed by **C14**.

### 1.3 Zero-Yield Synchronous Event-Driven Fast-Forwarding

Naive pin timing loopbacks yielding Asyncify on `pal_gpio_pulse_in` degrade performance by 10–50$\times$ due to Unwind/Rewind overhead. Mechanism:

1. **Pin Event Queue**: C side maintains a linked list of scheduled pin transitions.
2. **Zero-Yield Callback**: Trig pin edges trigger synchronous plugin callbacks, and the plugin enqueues Echo edge timestamps.
3. **Synchronous Clock Leap**: `pulse_in` advances `s_virtual_us` directly and returns pulse width with **zero Asyncify suspensions**.

---

## 2. Scenario-Specific Consistency Assurance (C1～C25 Categories + Sub-Scenarios)

> Testability statuses are maintained exclusively in [08](./08-simulation-consistency-checklist.md). Below are the problem statements, solutions, oracles, and boundaries.

---

### C1 — Business Causality & State Machine Logic

**Goal**: App/BAL state machines, sensor semantics, actuator commands, and fault/timeout paths remain causally consistent between simulation and physical hardware.

#### C1.1 Homologous App/BAL State Transitions
* **Problem**: Simulation running a "simplified logic fork" produces green state machines while hardware branches differently.
* **Hardware vs Simulation**: Hardware and simulation must share identical C sources ([ADR-0002](../../../decisions/unisim/0002-dual-target-compilation.md)).
* **Solution**: **B** Dual-target homologous compilation; `#ifdef SIMULATION` altering business branches in App layer is forbidden.
* **Oracle**: Identical input sequences yield bit-identical / semantically equivalent state transition golden traces.
* **Boundary**: Driver-level differences in DAL/PAL fall under C1.2/C7.

#### C1.2 DAL Bypass / `#ifdef SIMULATION` Narrowing
* **Problem**: Whole drivers replaced by JS $\rightarrow$ CRC, timeouts, and retries never execute in simulation (ADR-0003 Decision 2).
* **Hardware vs Simulation**: Hardware runs full driver logic; full-layer simulation bypasses allow protocol escapes.
* **Solution**: **A+B** Physical source substitution only; [ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md) JSON Fail-Loud gates; continuous auditing of residual bypasses.
* **Oracle**: Undeclared semantic bypasses fail at compile/boot time; declared peripherals execute protocol logic in C.
* **Boundary**: Physical quantities originate from plugins/injections; electrical equivalence is not claimed.

#### C1.3 Fault / Timeout / Disconnection Exception Paths
* **Problem**: Only happy paths tested; hardware disconnection/timeout state machines unverified.
* **Hardware vs Simulation**: Hardware faults are sporadic; simulation leverages deterministic fault injection.
* **Solution**: **B** Degradation engine / bus `drop_permil` / timeout injections ([06](./06-physical-degradation-engine.md)).
* **Oracle**: Injected disconnection/timeout transitions into specified fault states, with reproducible recovery sequences.
* **Boundary**: Electrical-tier disconnection impedance changes fall under C11 🚫.

#### C1.4 Idempotent Recovery & Retry Storms
* **Problem**: Error recovery paths duplicate queue entries or send duplicate actuator commands.
* **Hardware vs Simulation**: Fixed cooperative timing may mask overlapping retry windows.
* **Solution**: **B+C** Fault injection elongates ACK timeouts; track command count / sequence monotonicity.
* **Oracle**: Actuator command count under single fault injection $\le$ specification limit; zero undefined duplicate dispatches.
* **Boundary**: Network-layer exactly-once delivery falls outside OS scope.

---

### C2 — Virtual Microsecond Logic Timing (Single-Task / Single-Interrupt Friendly)

**Goal**: In single-task or non-overlapping interrupt scenarios, logical timing (sleep, pulse width, debounce, sampling periods) remains deterministically consistent. Multi-consumer clocks and fast-forward side effects fall under **C14/C21**.

#### C2.1 `sleep` / Scheduled Wakeup Fast-Forwarding
* **Problem**: Wall-clock dependencies cause background tab throttling and CI jitter.
* **Hardware vs Simulation**: Hardware approximates wall-clock; simulation enforces virtual time.
* **Solution**: **B** `s_virtual_us` + scheduler fast-forwarding to `next_wakeup_us`.
* **Oracle**: Wakeup timestamp sequences reproduce identically under identical seeds; wall-clock duration $\ll$ virtual span.
* **Boundary**: Crystal $\pm 50\text{ppm}$ drift not modeled by default (requires degradation engine injection).

#### C2.2 Pulse Width Measurement Zero-Yield Loopback
* **Problem**: `pulse_in` yielding causes performance collapse or timing drift.
* **Hardware vs Simulation**: Hardware busy-waits or captures input; simulation resolves synchronously via Pin Event Queues (§1.3).
* **Solution**: **B** Synchronous event-driven fast-forwarding.
* **Oracle**: Measured return values fall within tolerance contracts; Asyncify suspensions $= 0$ in HEADLESS paths.
* **Boundary**: Fast-forward skipping intermediate edges is governed by C14.2.

#### C2.3 Debouncing / RC Low-Pass Anchored to Virtual Clock
* **Problem**: Filter windows tracking wall-clocks produce varying results across machines.
* **Hardware vs Simulation**: Hardware contains analog noise; simulation filters must bind to `s_virtual_us`.
* **Solution**: **B** `wink_phys_*` reads virtual clocks uniformly ([06](./06-physical-degradation-engine.md)).
* **Oracle**: Fixed input edge sequences + fixed parameters $\rightarrow$ filter outputs match golden vectors.
* **Boundary**: Noise amplitude is an injection parameter, not circuit-level thermal noise.

#### C2.4 Single-Interrupt Friendly Sampling Periods
* **Problem**: Periodic tasks expecting fixed intervals are disrupted by scheduling jitter.
* **Hardware vs Simulation**: Hardware experiences preemption jitter; cooperative simulation is more stable (potentially overly ideal).
* **Solution**: **B** Virtual period assertions; optional **C** controlled jitter injection.
* **Oracle**: Period error $= 0$ in virtual time without injection; business logic meets specification or fails explicitly under jitter.
* **Boundary**: Hard real-time periodic constraints fall under C10.

---

### C3 — Shared State Race Conditions (Task↔Task / Task↔ISR)

**Goal**: Lockless sharing, multi-field tearing, and Task/ISR cross-accesses are stimulated and detected.

#### C3.1 Lockless Shared Read/Write (Task↔Task)
* **Problem**: Tasks reading/writing variables without locks; fixed cooperative interleaving masks errors.
* **Hardware vs Simulation**: Hardware preemption/dual-core exposes races; simulation round-robin masks them.
* **Solution**: **B** Deterministic chaotic scheduler (PRNG + `fairness_bound`, [ADR-0042](../../../decisions/unisim/0042-sim-execution-modes.md)); **C** Shadow memory TSan.
* **Oracle**: Known race use cases reliably trigger Faults in chaotic mode; Faults disappear upon adding critical sections.
* **Boundary**: Non-overlapping physical dual-core writes fall under C9.

#### C3.2 Task↔ISR Lockless Cross-Access
* **Problem**: Task reading multi-byte state is interrupted by half-updates from an ISR.
* **Hardware vs Simulation**: Hardware interrupts strike between arbitrary instructions; simulation polls ISRs at yield/poll points.
* **Solution**: **B** Injecting ISRs at multiple scheduling points; **C** TSan tagging ISR context accesses.
* **Oracle**: Unprotected multi-byte sharing is detected under chaotic scheduling + ISR injection.
* **Boundary**: Arbitrary instruction-level preemption requires Phase 4 deepening.

#### C3.3 Multi-Field Structure Tearing
* **Problem**: Half-updated `{x,y}` or `length+pointer` structs read prematurely.
* **Hardware vs Simulation**: Similar to C3.1/C3.2; struct tearing easily escapes.
* **Solution**: **C** Shadow memory per-field/per-object versioning; **A** Mandate seqlocks/critical sections.
* **Oracle**: Half-updated structs trigger Faults; complete critical sections pass.
* **Boundary**: Does not guarantee detection of all lock-free ABA scenarios.

#### C3.4 Publish-Subscribe Ordering Assumptions
* **Problem**: Flags updated before payloads are fully written (missing memory barriers / release ordering).
* **Hardware vs Simulation**: Unicore Wasm diminishes reordering differences; hardware multi-core / compiler reordering is aggressive.
* **Solution**: **A** Prohibit bare-flag protocols; **C** TSan / code review; hardware sampling.
* **Oracle**: Linter/review gates catch bare flag patterns; stimulated under chaotic mode where possible.
* **Boundary**: Full memory model simulation is not on daily tracks ($\rightarrow$ C12/C24).

---

### C4 — Critical Sections & Interrupt Preemption / Nesting

#### C4.1 Critical Section Gating (enter/exit)
* **Problem**: Missing `critical_exit`, nested counter corruption, sleeping within critical sections.
* **Hardware vs Simulation**: Hardware locks up or crashes; simulation asserts state.
* **Solution**: **B+C** `pal_os_critical_enter/exit` state machine assertions; ban ISR dispatch within critical sections.
* **Oracle**: Unpaired exit $\rightarrow$ Fault; pending IRQs inside critical sections deferred until exit.
* **Boundary**: Does not replicate all side effects of disabling hardware global interrupts.

#### C4.2 Scheduling Point ISR Delivery (Poll Model)
* **Problem**: Expecting instantaneous preemption when delivery occurs strictly at tick/yield polling boundaries ([01](./01-wasm-sandbox-lifecycle.md)).
* **Hardware vs Simulation**: Hardware strikes instantly; simulation polls.
* **Solution**: **B** Documented Poll semantics; Phase 4 increases insertion point density.
* **Oracle**: Edge registered at known yield point $\rightarrow$ ISR executes in next polling window; no execution in pure computation windows.
* **Boundary**: Arbitrary instruction-level preemption is ❌ (documented product boundary).

#### C4.3 Priority Nesting
* **Problem**: High-priority ISR interrupting lower-priority ISR/Task.
* **Hardware vs Simulation**: ESP-IDF/FreeRTOS supports nesting; simulation baseline is single-tier.
* **Solution**: **B** Phase 4 delivers single-tier preemption + critical sections; nesting as future enhancement; hardware fallback.
* **Oracle**: Single-tier: Preempts outside critical sections; nesting use cases marked hardware-exclusive until implemented.
* **Boundary**: Full nested priority preemption is not promised on daily tracks.

#### C4.4 FromISR / Non-ISR-Safe API Misuse
* **Problem**: Calling blocking or lock-acquiring APIs inside an ISR.
* **Hardware vs Simulation**: Hardware deadlocks or asserts; simulation may "happen to work".
* **Solution**: **A** Linter/API attributes (ISR-safe whitelist); **C** Calling non-safe APIs in ISR context $\rightarrow$ Fault.
* **Oracle**: Invoking `mutex_lock` in ISR triggers deterministic Fault.
* **Boundary**: Third-party closed-source callbacks require manual annotation.

#### C4.5 Pending Interrupt Queue Overflow
* **Problem**: Edge storms exceed `PAL_WASM_INTERRUPT_QUEUE_SIZE` (default 16) $\rightarrow$ dropped interrupts.
* **Hardware vs Simulation**: Hardware latches pending bits; simulation queue drops or requires defined policies.
* **Solution**: **C** Queue full triggers Fail-Loud (counter/Fault); **B** Configurable queue depth.
* **Oracle**: Injected edges $>$ capacity $\rightarrow$ observable overflow counters or Fault; silent dropping prohibited.
* **Boundary**: Does not model chip-specific interrupt controller internal states.

---

### C5 — Blocking / Starvation / Watchdogs

#### C5.1 STRICT_NONBLOCKING Hiding Blocking APIs at Compile Time
* **Problem**: App misusing blocking reads triggers hardware WDT resets.
* **Hardware vs Simulation**: Simulation defaults to strict mode ([ADR-0025](../../../decisions/core/0025-app-blocking-api-honesty-pragma-convention.md)).
* **Solution**: **A** `-DWINK_STRICT_NONBLOCKING=1`; blocking symbols hidden $\rightarrow$ link-time failure.
* **Oracle**: App invoking `WINK_BLOCKING` APIs fails to build for simulation.
* **Boundary**: Bringup/selftest isolated under non-strict profiles.

#### C5.2 Soft WDT (Virtual-Time Watchdog)
* **Problem**: Task infinite loops without feeding watchdog.
* **Hardware vs Simulation**: Hardware triggers hardware WDT reset; simulation enforces virtual-time soft WDT.
* **Solution**: **B+C** Unfed watchdog for $N$ virtual ms $\rightarrow$ Fault isolation.
* **Oracle**: Watchdog starvation use cases trigger Faults within threshold virtual time.
* **Boundary**: Does not reflect **physical** WDT timeouts caused by host CPU exhaustion ([07](./07-scheduler-model.md)).

#### C5.3 Ready Task Starvation
* **Problem**: High-priority / non-yielding task starves lower-priority tasks.
* **Hardware vs Simulation**: Hardware time-slices; cooperative simulation easily starves or acts overly fair (round-robin).
* **Solution**: **C** Alert on prolonged ready-but-unrun tasks; **B** Chaotic fairness boundaries.
* **Oracle**: Constructed starvation use cases trigger warnings/Faults.
* **Boundary**: Not fully equivalent to FreeRTOS priority preemption semantics (see C16).

#### C5.4 Dynamic Indirect Blocking
* **Problem**: Indirect calls via function pointers bypass static blocking linters.
* **Hardware vs Simulation**: Equally dangerous on hardware.
* **Solution**: **A** Maximize static visibility; **C** Runtime blocking probes verifying calling context.
* **Oracle**: Indirect blocking calls caught by runtime guards on probed paths.
* **Boundary**: Assembly/obfuscated code cannot be fully guaranteed.

#### C5.5 Priority Inversion
* **Problem**: Low-priority task holds lock, high-priority blocks, medium-priority starves both.
* **Hardware vs Simulation**: Requires priority inheritance/ceilings; simulation baseline may lack it.
* **Solution**: **B** Document whether inheritance is implemented; **C** Detect high-priority blocked on low-priority beyond thresholds; hardware sign-off.
* **Oracle**: Inversion use cases pass when inheritance is declared, otherwise marked 🟡/Hardware.
* **Boundary**: Full inheritance protocols implemented in phased waves.

---

### C6 — Stack / Heap / Memory Safety

#### C6.1 Static Heap Quota Exhaustion
* **Problem**: Simulation heap far exceeds SRAM, hiding OOM bugs.
* **Hardware vs Simulation**: Hardware runs out of memory early; simulation enforces quotas ([ADR-0045](../../../decisions/unisim/0045-simulation-memory-quota-and-fault-policy.md)).
* **Solution**: **B** `sim_heap[WINK_SIM_HEAP_QUOTA_BYTES]`; exhaustion returns NULL $\rightarrow$ `WINK_ERR_NO_MEM`.
* **Oracle**: Quota exhaustion routes to Fault/error handlers, matching hardware error codes.
* **Boundary**: Fragmentation profiles differ from real embedded allocators.

#### C6.2 Heap Fragmentation
* **Problem**: Repeated alloc/free causes allocation failures despite sufficient total free memory.
* **Hardware vs Simulation**: Different allocators produce different fragmentation maps.
* **Solution**: **B** Optional fragmentation stress mode; **C** Allocation failure injection.
* **Oracle**: Stress mode triggers `NO_MEM`; business logic exhibits graceful degradation.
* **Boundary**: Geometric identity with ESP-IDF heap fragmentation is not claimed.

#### C6.3 ASan / UBSan (UAF, Out-of-Bounds, Unaligned Access, Overflow)
* **Problem**: Wild pointers, UAF, and out-of-bounds access fail silently without sanitizers.
* **Hardware vs Simulation**: Hardware behavior is erratic; host builds enable sanitizers.
* **Solution**: **C** ASan + UBSan integrated into CI/Host testing.
* **Oracle**: Known bad test cases intercepted by sanitizers.
* **Boundary**: Wasm daily builds do not always support full ASan overhead.

#### C6.4 Banning Bare Malloc in App Layer
* **Problem**: Direct `malloc` calls in application layer break resource isolation models.
* **Solution**: **A** `NO-MALLOC-APP` linter rule.
* **Oracle**: Non-compliant invocations fail linter checks.
* **Boundary**: Platform allocator implementations exempted.

#### C6.5 Per-Task Stack Overflow
* **Problem**: Undersized task stacks; Wasm single-stack/fiber models diverge from FreeRTOS multi-stack architectures.
* **Hardware vs Simulation**: Hardware uses stack canaries; Wasm organizes memory differently.
* **Solution**: **C** Fiber/stack watermark checks or `STACK_OVERFLOW_CHECK`; **B** Per-task stack accounting.
* **Oracle**: Deep recursion / oversized stack frame use cases trigger overflow detection.
* **Boundary**: Byte-level alignment with Xtensa stack red zones is ❌.

#### C6.6 Buffer Reuse During Active DMA / Asynchronous Transfers
* **Problem**: Mutating buffer memory before transfer completes (overlaps C19).
* **Solution**: See **C19**; emphasizes memory ownership contracts.
* **Oracle**: Writing to active transfer buffers triggers Faults or data corruption assertions.
* **Boundary**: Depends on existence of C8 asynchronous windows.

---

### C7 — Bus Protocol Framing / CRC / Error Recovery

#### C7.1 Homologous Protocol Framing & CRC
* **Problem**: Bypassing CRC logic leaves hardware corrupted frame recovery untested.
* **Solution**: **B** Homologous DAL drivers; bad CRC injection.
* **Oracle**: Bad CRC frames route to error recovery paths without entering business success states.
* **Boundary**: Electrical bit error characteristics fall under C11/C18.

#### C7.2 ACK Timeouts & Retry Loops
* **Problem**: Timeout durations and retry counts diverge from hardware.
* **Solution**: **B** Virtual-time timeouts; packet loss `drop_permil`.
* **Oracle**: Fixed packet loss seeds reproduce exact retry counts and terminal error codes.
* **Boundary**: Cable propagation delay spectrum is unmodeled.

#### C7.3 JSON Semantic Simulation Gating
* **Problem**: Undeclared peripherals taking unauthorized semantic bypass shortcuts.
* **Solution**: **A** [ADR-0040](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md).
* **Oracle**: Undeclared peripheral shortcuts Fail-Loud.
* **Boundary**: Gates manage bypass authorization only, not electrical accuracy.

#### C7.4 Zero Residual Bypass Audit
* **Problem**: Legacy drivers retaining full `#ifdef SIMULATION` bypass blocks.
* **Solution**: **A+C** Static scanning + CI audit whitelist.
* **Oracle**: Whitelist is empty or entries possess approved exemption ADRs.
* **Boundary**: Physical quantity source hook `#ifdef` blocks are permitted.

---

### C8 — DMA / Bus Asynchronous Windows

#### C8.1 Coarse-Grained Transfer Duration Yields
* **Problem**: `pal_i2c_transfer` returning in $0\mu\text{s}$ masks concurrent buffer mutations.
* **Hardware vs Simulation**: Hardware DMA allows CPU to run other tasks concurrently.
* **Solution**: **B** Compute virtual duration from baud rate, suspend task, wake upon completion IRQ.
* **Oracle**: Another task writing to shared buffers during transfer window is detected via race assertions.
* **Boundary**: Not cycle-accurate bus timing.

#### C8.2 Completion Interrupt & Task Wakeup Ordering
* **Problem**: Incorrect assumptions regarding completion callback vs waiting task wakeup order.
* **Solution**: **B** Explicit "set completion flag/queue first, then resume" contract; chaotic mode shuffles equal-priority tasks.
* **Oracle**: Tests validate contract order; incorrect assumptions fail under chaotic scheduling.
* **Boundary**: Minor differences with silicon DMA IRQ priorities may persist.

#### C8.3 Residual Synchronous APIs
* **Problem**: Synchronous PAL completions creating gaps in C8 coverage.
* **Solution**: **B** Bus API migration checklist; **A** Tag sync-only symbols.
* **Oracle**: Target buses implement asynchronous models or carry explicit `sync-only` tags.
* **Boundary**: Transitional exemptions permitted with documentation.

---

### C9 — Multi-Core SMP True Concurrency

#### C9.1 Single Virtual Core Product Boundary
* **Problem**: Users expecting multi-core SMP emulation in browser.
* **Solution**: Follow [ADR-0014](../../../decisions/unisim/0014-sim-single-virtual-core.md)—**intentionally unmodeled**; clear product boundary.
* **Oracle**: Documented as 🚫/❌ in 08; zero claims of dual-core simulation.
* **Boundary**: Hardware sign-off.

#### C9.2 Chaotic Interleaving Approximating Multi-Core Races
* **Problem**: Unicore simulation can approximate race conditions via high-frequency task switching.
* **Solution**: **B** Phase 4 chaotic scheduler; physical dual-core hardware testing.
* **Oracle**: Unicore-stimulable races caught under chaotic mode; multi-core exclusive cases in hardware test suites.
* **Boundary**: Cache coherency and inter-core interrupts are unmodeled (C24).

---

### C10 — Fast-Loop ISRs (FOC / Hardware Timers)

#### C10.1 Virtual-Time Soft-Stepping Approximation
* **Problem**: 20kHz hard real-time ISRs unobtainable in browser Wasm.
* **Solution**: **B** [ADR-0047](../../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md) soft-stepping; plant differential equations in `wink_sim_physical`; wall-clock/rand forbidden.
* **Oracle**: Control state reproduces deterministically under fixed virtual steps; diffed against golden traces.
* **Boundary**: Does not claim $50\mu\text{s}$ hard real-time execution.

#### C10.2 PWM–ADC Hardware Trigger Degradation
* **Problem**: Hardware triggers ADC sampling at PWM center/edge; soft-stepping may exhibit phase drift.
* **Solution**: **B** Documented degradation; control logic verifiable in simulation; phase sensitivity verified on HIL.
* **Oracle**: Phase-sensitive cases enter HIL suites; simulation validates control algorithms.
* **Boundary**: 🚫 Cycle-accurate phase synchronization.

#### C10.3 DI / ISR Layering Boundaries
* **Problem**: Embedding business logic into fast loops makes code un-simulatable and untestable.
* **Solution**: **A** ADR-0047 layering; linter restricts fast-loop API surface.
* **Oracle**: Fast loops permit only whitelisted PAL calls; violations fail linter checks.
* **Boundary**: Whitelist evolves with product requirements.

---

### C11 — Electrical / Analog Circuit Dynamics

#### C11.1 SPICE / Power Integrity
* **Solution**: 🚫 [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md); hardware / dedicated SPICE tools.
* **Oracle**: Product commitments exclude this category.
* **Boundary**: Intentionally unmodeled.

#### C11.2 Tabular Degradation Engine Approximations
* **Problem**: Perfectly ideal sensors mislead PID/control tuning.
* **Solution**: **B** [06](./06-physical-degradation-engine.md) jitter, noise, warmup, and packet loss tables.
* **Oracle**: Degradation parameters alter reading distributions; ideal mode `{0}` reproduces baseline.
* **Boundary**: Tabular empirical models, not SPICE circuit simulations.

#### C11.3 ADC Quantization / VREF / Pin Capacitance
* **Solution**: 🚫 or coarse optional quantization bins; unmodeled by default.
* **Oracle**: Unit tests validate coarse bins if enabled; marked 🚫 in 08 otherwise.
* **Boundary**: Metrology-grade analog accuracy requires hardware.

---

### C12 — CPU / ABI Instruction-Level Consistency

#### C12.1 Daytime Fast-Track (Wasm / Host Native)
* **Solution**: **B** Rapid iteration; accepts potential ABI divergences from physical hardware.
* **Oracle**: Functional test feedback within seconds.
* **Boundary**: Not instruction-accurate.

#### C12.2 Nightly RISC-V 32 / Hardware Binary Dual-Track
* **Problem**: Struct padding, alignment, calling conventions, and enum widths causing "green in sim, crashes on board".
* **Solution**: **C** Phase 5 interpreter or hardware `.bin` cross-checks.
* **Oracle**: Padding-sensitive test cases fail or alert in nightly tracks.
* **Boundary**: Slow execution; not mandatory for daytime PRs.

#### C12.3 Struct Packing / Enums / Bitfields
* **Solution**: **A** Packing rules + static assertions; **C** Dual-track validation.
* **Oracle**: `_Static_assert(sizeof...)`; struct sizes match across targets or diverge via explicit branches.
* **Boundary**: Bitfield layout remains compiler-dependent; use with caution.

#### C12.4 Endianness & Unaligned Memory Access
* **Solution**: **C** UBSan; explicit protocol serialization.
* **Oracle**: Unaligned access intercepted by Host UBSan; protocol layers validated via serialization tests.
* **Boundary**: Wasm and Xtensa tolerate unaligned accesses differently.

---

### C13 — Lifecycle / Reset / Boot Sequences

**Goal**: Initialization, re-initialization, and soft reboot states align with physical hardware power-on/reset semantics, eliminating cross-run pollution.

#### C13.1 Cold Boot: BSS / Static Initialization / Peripheral Pin Defaults
* **Problem**: Reusing Worker instances retains dirty static variables and GPIO states.
* **Hardware vs Simulation**: Hardware resets to power-on defaults; Wasm re-initialization risks state leakage.
* **Solution**: **B** `INIT` path enforces `pal_wasm_reset_physical` + scheduler/heap/PRNG reset; cold boot manifest defined.
* **Oracle**: State across consecutive `INIT` cycles matches cold-boot vectors (zero residual business state).
* **Boundary**: Chip-specific strap pin defaults may be partially modeled.

#### C13.2 Soft Reboot / Reset Reason Propagation
* **Problem**: Branches inspecting `esp_reset_reason` evaluate to static constants in simulation.
* **Solution**: **B** Injectable reset reasons; **A** Documented as 🟡 if unmodeled.
* **Oracle**: Injected reset reasons (BROWN-OUT, WATCHDOG) trigger specified recovery paths.
* **Boundary**: Real power drop waveforms are 🚫.

#### C13.3 Peripheral Deinit / Re-init
* **Problem**: Re-initializing without de-initializing causes handle leaks and duplicate ISR registrations.
* **Solution**: **C** Duplicate registration assertions; resource accounting.
* **Oracle**: Duplicate initialization use cases Fail-Loud or succeed idempotently per API contracts.
* **Boundary**: Contracts must explicitly define idempotency expectations.

#### C13.4 Task / Object Lifecycles & Zombie GC
* **Problem**: Accessing task stacks/queues after `task_delete` ([07](./07-scheduler-model.md) ZOMBIE/TERMINATED).
* **Solution**: **B** Scheduler GC semantics; **C** UAF Sanitizer.
* **Oracle**: Post-delete access $\rightarrow$ Fault/ASan.
* **Boundary**: Minor asynchronous deletion timing differences vs FreeRTOS documented in compatibility tables.

---

### C14 — Fast-Forward / Co-Simulation Stepping Contracts

**Goal**: Virtual time leaps and plant step-locking preserve causality; dual stepping strictly prohibited.

#### C14.1 Clock Single-Writer Gate / Prohibiting Dual Stepping
* **Problem**: `delay` and Worker advancing clocks simultaneously $\rightarrow$ time tearing.
* **Solution**: **A+B** SSOT Red Line (§1.1); CI assertions enforce single clock writer.
* **Oracle**: Static/runtime detection of secondary clock writers $\rightarrow$ Failure.
* **Boundary**: HEADLESS internal gates documented as authorized exceptions.

#### C14.2 Fast-Forward Skipping Intermediate Edges / Half-Window Debounce
* **Problem**: Jumping to `next_wakeup` skips intermediate GPIO edges or corrupts debounce windows.
* **Solution**: **B** Drain pending pin events / physical steps before jumping, or advance to global minimum timestamp.
* **Oracle**: Intermediate edges trigger reliably under fast-forwarding; debounce windows do not distort.
* **Boundary**: Queue overflow conditions governed by C4.5/C14.4.

#### C14.3 Plant↔OS Step-Lock Drift
* **Problem**: JS plant tracking wall-clocks or mismatched $\Delta t$, drifting away from `s_virtual_us`.
* **Solution**: **B** Step-Lock: Identical `virtual_dt` drives both OS and plant; plant reading `Date.now` is prohibited.
* **Oracle**: Identical seeds produce bit-exact trajectories; wall-clock plants rejected by gates.
* **Boundary**: Visual interpolation permitted for UI rendering, but **must never** feed control decisions.

#### C14.4 Pin Event Queue Overflow / Loss
* **Problem**: Synchronous callbacks generating excessive future edges overflow event queues.
* **Solution**: **C** Queue full triggers Fault; **B** Configurable queue capacity.
* **Oracle**: Queue overflow is observable, never silent.
* **Boundary**: Queue capacity is independent of physical hardware FIFOs.

#### C14.5 Observation vs Injection Ordering Races
* **Problem**: Reading GPIO outputs while plant injects inputs without explicit barriers.
* **Solution**: **B** Enforce intra-step sequence: "Read Controls $\rightarrow$ Compute Plant $\rightarrow$ Inject Inputs".
* **Oracle**: Test harnesses violating step ordering fail or are rejected by APIs.
* **Boundary**: Multi-plant execution across parallel Workers requires additional barrier protocols.

---

### C15 — Host↔Wasm Boundary Integrity

**Goal**: Simulation platform never produces false greens; Asyncify/ABI/gating errors Fail-Loud.

#### C15.1 Asyncify Suspension Contracts
* **Problem**: `sleep` failing to return Promises / incorrect `__async` flags $\rightarrow$ silent skipping or rewind deadlocks ([ADR-0019](../../../decisions/unisim/0019-wasm-imports-override-and-asyncify-syntax.md), [01](./01-wasm-sandbox-lifecycle.md)).
* **Solution**: **A** JS library wrapper standards; **C** Invalid Asyncify state assertions.
* **Oracle**: Erroneous overrides fail CI; correct paths suspend and resume reliably.
* **Boundary**: Covers agreed import sets only.

#### C15.2 Interrupt Push$\rightarrow$Poll / Reentrancy
* **Problem**: Legacy Push models cause deterministic crashes during Asyncify sleep windows; Poll model mandatory.
* **Solution**: **B** Remove `_trigger_wasm_interrupt` export; pending queue + dispatch model.
* **Oracle**: Static checks verify absence of Push exports; reentrant interrupts do not crash.
* **Boundary**: Poll delay semantics governed by C4.2.

#### C15.3 BigInt / Pointer ABI
* **Problem**: `number` truncating uint64 timestamps; dangling pointers passed to JS.
* **Solution**: **A** `-s WASM_BIGINT=1`; end-to-end `bigint` in TypeScript; pointer lifecycle contracts.
* **Oracle**: 64-bit timestamps round-trip without loss; pointer lifecycle violations fail tests.
* **Boundary**: Third-party JS plugins must adhere to identical contracts.

#### C15.4 Semantic Bypass Leakage
* **Problem**: Workbench or tests directly invoking semantic bypasses without gating authorization.
* **Solution**: **A** ADR-0040; **C** Runtime probes.
* **Oracle**: Unauthorized bypass invocations fail immediately.
* **Boundary**: Manual memory writes via debuggers excluded.

#### C15.5 Worker Isolation & Main Thread Starvation
* **Problem**: Running Wasm+Asyncify on UI thread starves timers and exhausts memory.
* **Solution**: **A** Web Worker isolation mandatory ([01](./01-wasm-sandbox-lifecycle.md)).
* **Oracle**: Architectural tests and linting block main-thread Wasm loading.
* **Boundary**: Node hosts follow separate documented constraints.

---

### C16 — OS Synchronization Primitive Alignment

**Goal**: Mutex/queue/ringbuf/timeout semantics align with real embedded OSAL contracts, avoiding accidental return value divergences.

#### C16.1 Mutex Locking / Timeouts / Recursion
* **Problem**: Timeout error codes and recursive locking behaviors diverging from FreeRTOS.
* **Solution**: **B** Semantic compatibility tables ([07](./07-scheduler-model.md)); `timeout_fired` unit tests.
* **Oracle**: Every row in compatibility table validated via unit tests; recursion follows specifications.
* **Boundary**: Priority inheritance governed by C5.5.

#### C16.2 Queue / Ringbuf Full & Overwrite Policies
* **Problem**: Divergence between blocking on full vs dropping oldest vs rejecting writes.
* **Solution**: **B** Explicit API contracts; **C** Full/empty queue observation.
* **Oracle**: Full queue use cases match documented drop/block contracts.
* **Boundary**: Zero-copy DMA queues governed by C19.

#### C16.3 Blocking Waits & Timeout Wakeup Priority
* **Problem**: Undefined resolution when event arrival and timeout expiration coincide.
* **Solution**: **B** Defined priority rules (e.g. event arrival takes precedence); pinned via tests.
* **Oracle**: Coinciding timeout/event use cases resolve deterministically per contract.
* **Boundary**: Intentional divergences from physical hardware documented in compatibility tables.

#### C16.4 Task Notifications / Event Groups
* **Problem**: Auto-clearing bits and multi-bit wait semantics error-prone.
* **Solution**: **B** Test exposed APIs; **A** Hide unexposed primitives.
* **Oracle**: Exposed synchronization APIs covered by semantic test suites.
* **Boundary**: Unexposed APIs marked N/A in 08.

#### C16.5 Deadlock Detection
* **Problem**: Task A holding Lock 1 waiting for Lock 2 while Task B holds Lock 2 waiting for Lock 1.
* **Solution**: **C** Optional wait-for-graph cycle detection / timeout traps; **A** Lock ordering rules.
* **Oracle**: Classic deadlock use cases trigger timeouts or Faults.
* **Boundary**: Does not formally prove absence of deadlocks.

---

### C17 — Peripheral Resource Conflicts / Clock Couplings

**Goal**: Valid APIs combined into invalid resource configurations fail in simulation/codegen rather than on physical hardware.

#### C17.1 Pin Multiplexing Conflicts
* **Problem**: Multiple peripherals claiming identical GPIO pins.
* **Solution**: **A** Codegen / board JSON conflict detection; registration assertions in simulation.
* **Oracle**: Conflicting configurations Fail-Loud.
* **Boundary**: Dynamic runtime pin remapping requires separate API contracts.

#### C17.2 Timer / PWM Channel Exclusivity
* **Problem**: Hardware timer shared between PWM generation and input capture without coordination.
* **Solution**: **A** Resource allocation maps; **C** Secondary claim Faults.
* **Oracle**: Overlapping timer claims trigger simulation failures.
* **Boundary**: Hardware-supported shared modes whitelisted explicitly.

#### C17.3 APB / Peripheral Clock Mutation Side Effects
* **Problem**: Changing CPU/APB frequency alters UART baud rates and PWM periods unexpectedly.
* **Solution**: **B** Coarse simulation clock models or **🚫** explicit unmodeled declaration; hardware verification.
* **Oracle**: If modeled, frequency changes assert baud rate errors; marked 🚫 in 08 otherwise.
* **Boundary**: Full clock tree emulation is 🚫.

#### C17.4 PWM Duty Update Glitches / Phase Continuity
* **Problem**: Duty cycle updates mid-cycle causing transient glitches; instantaneous simulation writes mask glitches.
* **Solution**: **B** Optional "effective next period" model, or documented 🟡 approximation.
* **Oracle**: Updates align with period boundaries if modeled; marked approximate otherwise.
* **Boundary**: Electrical glitch dynamics are 🚫 (C11).

#### C17.5 Shared Bus Ownership Contention
* **Problem**: Multiple tasks executing `i2c_transfer` concurrently without mutex protection.
* **Solution**: **A** Bus locking rules; **C** Overlapping transfer window detection Faults.
* **Oracle**: Lockless concurrent transfers caught and reported.
* **Boundary**: Multi-master arbitration governed by C18.

---

### C18 — Bus Fault State Machines (Beyond CRC / Packet Loss)

#### C18.1 I2C NACK / Clock Stretching / Bus Lockup
* **Problem**: Testing packet drops only, ignoring NACKs, SCL held low, and recovery routines.
* **Solution**: **B** Injectable NACKs and clock stretch timeouts; recovery sequences execute homologous drivers.
* **Oracle**: Injected errors yield expected error codes; recovery sequences restore bus communication.
* **Boundary**: Electrical rising-edge timing is 🚫.

#### C18.2 UART Framing Errors / Break / FIFO Overflows
* **Problem**: Corrupted stop bits and RX FIFO overflows unverified.
* **Solution**: **B** Framing error and RX overflow injection; **C** Overflow counters.
* **Oracle**: Overflow conditions route to specified driver error/drop policies.
* **Boundary**: Sampling point phase alignment is 🚫.

#### C18.3 SPI Modes / CS Timing / Full-Duplex
* **Problem**: CPOL/CPHA configuration mismatches or early/late CS edge assumptions.
* **Solution**: **B** SPI mode parameters enforced in models; mode mismatches yield data corruption.
* **Oracle**: Incorrect SPI modes fail tests; correct modes match golden data.
* **Boundary**: Board trace flight-time delays are 🚫.

#### C18.4 Transfer Abort / Half-Packet Recovery
* **Problem**: Aborting transfers leaves state machines trapped in partial frame states.
* **Solution**: **B** Abort APIs + driver recovery routines; tests validate subsequent frames after aborts.
* **Oracle**: Subsequent transfers succeed or return explicit errors following aborts.
* **Boundary**: DMA abort specifics overlap C19.

#### C18.5 Multi-Master Arbitration
* **Solution**: **🚫** for most embedded products; **B** simplified winner-takes-all model if required.
* **Oracle**: Implemented on demand; marked 🚫 in 08 by default.
* **Boundary**: Full physical I2C arbitration timing requires hardware.

---

### C19 — DMA / Buffer Lifecycles

#### C19.1 Half-Transfer / Ping-Pong Buffer Switching
* **Problem**: Modeling full-completion only; half-buffer callbacks remain unexecuted.
* **Solution**: **B** Optional half-complete events; ping-pong buffer ownership assertions.
* **Oracle**: Half-transfer callbacks fire with accurate buffer indices and invocation counts.
* **Boundary**: Coarse-grained timing, not beat-level accurate.

#### C19.2 Buffer Mutation During Active DMA
* **Problem**: CPU modifying buffer memory while DMA transfer is in progress.
* **Solution**: **C** Transfer window write detection (with C8); **A** API buffer ownership documentation.
* **Oracle**: Mutating buffers during active transfers triggers Faults.
* **Boundary**: Requires active asynchronous transfer windows.

#### C19.3 Descriptors / Handle Leaks After Abort
* **Problem**: Assuming DMA is running or executing duplicate free calls after abort.
* **Solution**: **B** Abort state machines; **C** Double-free / UAF sanitizers.
* **Oracle**: Subsequent restarts succeed after aborts; duplicate aborts return explicit error codes.
* **Boundary**: Hardware descriptor register formats are not emulated.

#### C19.4 DMA Memory Region Constraints (Overlaps C24)
* **Problem**: Buffers allocated in non-DMA-capable memory regions.
* **Solution**: **A** Memory tag validation or 🟡 documented approximation; hardware sign-off.
* **Oracle**: Tagged allocators reject invalid memory regions; cross-referenced to C24 in 08.
* **Boundary**: ESP32 cache/DMA RAM specifics require hardware.

---

### C20 — Callback Reentrancy / Deferred Bottom-Halves

#### C20.1 Invoking Yielding DAL Inside ISRs / Callbacks
* **Problem**: Blocking sensor reads inside interrupt callbacks $\rightarrow$ reentrancy crashes or deadlocks.
* **Solution**: **A** Callback context rules; **C** Context detection Faults (overlaps C4.4).
* **Oracle**: Illegal blocking invocations inside callbacks trigger Faults.
* **Boundary**: Identical to C4.4.

#### C20.2 Sensor Callbacks Directly Driving Actuators
* **Problem**: Input callbacks triggering actuator writes, forming synchronous coupling and priority inversion.
* **Solution**: **A** Recommend queue decoupling; **B** Optional callback recursion depth tracking.
* **Oracle**: Excessive callback nesting warns; recommended decoupled patterns pass.
* **Boundary**: Synchronous coupling not completely banned, but risks documented.

#### C20.3 Workqueue / Deferred Bottom-Half Ordering
* **Problem**: Relying on specific softirq/workqueue dispatch ordering unavailable in simulation.
* **Solution**: **B** Explicit execution order if PAL provides deferred jobs; **A** Omit from public APIs otherwise.
* **Oracle**: Exposed workqueues covered by ordering tests; N/A otherwise.
* **Boundary**: Linux softirq semantics are not replicated.

---

### C21 — Time & Counter Wrap-Around

#### C21.1 `uint32` Millisecond / Tick Wrap-Around
* **Problem**: Using `now - last` without unsigned arithmetic $\rightarrow$ timer corruption upon 49.7-day rollover.
* **Solution**: **A** Unsigned difference coding standards; **C** Fast-forward testing crossing rollover boundaries.
* **Oracle**: Timeouts evaluate accurately across wrap-around points.
* **Boundary**: `s_virtual_us` is uint64; testing validates App-level uint32 variables.

#### C21.2 Relative Timeouts Crossing Fast-Forward Jumps
* **Problem**: Relative deadline calculations corrupted by fast-forward leaps.
* **Solution**: **B** Absolute `wakeup_us` scheduling; unit test validation.
* **Oracle**: Scheduled tasks wake reliably following time leaps.
* **Boundary**: Overlaps C14.

#### C21.3 Sequence Number / Ring Buffer Index Wrapping
* **Problem**: Sequence number comparisons and circular buffer modulo arithmetic errors.
* **Solution**: **C** Targeted unit tests; fuzzing seeds crossing boundary indices.
* **Oracle**: Push/pop operations remain correct across wrap-around boundaries.
* **Boundary**: Custom application sequences require dedicated test suites.

---

### C22 — Power / Low-Power / Clock Domains

#### C22.1 Light / Deep Sleep Wakeup
* **Solution**: **🚫** for most paths or minimal "sleep = suspend task + fast-forward to wakeup source"; hardware fallback.
* **Oracle**: Wakeup sources wake suspended tasks if minimal model is implemented; marked 🚫 in 08 otherwise.
* **Boundary**: Current consumption and wakeup ramp times are 🚫.

#### C22.2 Peripheral Clock Gating
* **Solution**: **🚫** or explicit stubs returning error codes on unclocked peripheral access.
* **Oracle**: Error codes asserted if stubs exist; marked 🚫 otherwise.
* **Boundary**: Full clock trees are 🚫.

#### C22.3 Brownout / Under-Voltage Reset
* **Solution**: Reset reason injection governed by C13.2; supply voltage waveforms are 🚫.
* **Oracle**: Identical to C13.2.
* **Boundary**: Analog under-voltage dynamics are 🚫.

---

### C23 — Persistence / NVS / Wear

#### C23.1 Power-Loss Write Tearing
* **Problem**: Power loss mid-write corrupting stored configurations.
* **Solution**: **B** Inject power cut mid-write; validate homologous dual-partition/CRC recovery.
* **Oracle**: Injected tearing triggers fallback to valid backup or default values.
* **Boundary**: Flash physical endurance and wear are 🚫.

#### C23.2 Key Space Exhaustion / Storage Full
* **Solution**: **B** Storage quotas; **C** Disk full error returns.
* **Oracle**: Quota exhaustion error codes align with hardware.
* **Boundary**: Wear-leveling algorithms are not modeled.

#### C23.3 Simulation "Power-Off" Semantics
* **Problem**: Unclear whether browser page refreshes retain NVS data.
* **Solution**: **A** Documented rule: Worker teardown = power cut; explicit snapshot APIs managed separately.
* **Oracle**: Clean reboot starts with empty NVS unless snapshots are loaded.
* **Boundary**: Browser IndexedDB storage quotas documented separately.

---

### C24 — Caches / Memory Attributes / DMA RAM

#### C24.1 DMA Must Reside in DMA-Capable RAM
* **Solution**: **A** Partitioned allocation APIs; simulation tag assertions or hardware tests (overlaps C19.4).
* **Oracle**: See C19.4.
* **Boundary**: Full Xtensa cache coherency modeling is 🚫.

#### C24.2 Cache Coherency Assumptions
* **Solution**: 🚫 on daily tracks; hardware / Phase 5 sampling.
* **Oracle**: Marked 🚫 in 08; hardware test suites include DMA+cache test cases.
* **Boundary**: Intentionally unmodeled.

#### C24.3 IRAM / Flash Slow-Path Placement
* **Solution**: Simulation **🟡**; link script differences documented.
* **Oracle**: Passing simulation does not guarantee IRAM placement compliance.
* **Boundary**: Hardware linker scripts and bus wait-states.

---

### C25 — Floating-Point / Numerics & Compiler UB

#### C25.1 Signed Integer Overflow / Shift UB
* **Solution**: **C** UBSan; **A** Safe arithmetic standards.
* **Oracle**: Undefined behavior intercepted by Host UBSan.
* **Boundary**: Wasm vs Xtensa overflow wrapping differences covered by C12.

#### C25.2 NaN / Inf Propagation & Flush-to-Zero
* **Solution**: **B** Control algorithm NaN safety assertions; FPU mode differences verified on hardware.
* **Oracle**: Injected NaNs route to safe states or explicit errors.
* **Boundary**: Hardware FTZ (Flush-To-Zero) modes are 🚫 exact simulation.

#### C25.3 Floating-Point Determinism
* **Problem**: Cross-platform math library variations causing plant trajectory drift.
* **Solution**: **B** Software libm or bounded tolerance comparisons; Tolerance Bands on golden traces.
* **Oracle**: Reproduces within tolerance bands; exceeds envelope $\rightarrow$ failure.
* **Boundary**: Bit-exact floating-point parity across all target architectures is not promised.

---

## 3. Deep Diagnostics & Evolution Milestones

### 3.1 Extended Simulation Distortion Dimensions

| Distortion Dimension | Simulation Current State | Real Hardware Behavior | Potential Escape Risk | Primary Categories |
|---|---|---|---|---|
| **Concurrency Model** | Single virtual core, cooperative yielding | Preemptive + Potential multi-core | Hidden lockless race conditions | C3, C9, C16 |
| **Interrupt Timing** | Tick/Yield polling boundaries | Instantaneous firing, priority nesting | Unprotected mutations outside critical sections | C4, C15 |
| **Buses & I/O** | Synchronous $0\mu\text{s}$; drops supported, weak state machines | Asynchronous DMA + comprehensive fault states | Transfer window races, unhandled bus lockups | C7, C8, C18, C19 |
| **Time Progression** | Virtual clock fast-forward leaps | Continuous wall-clock + crystal drift | Skipped edges, dual stepping, wrap-arounds | C2, C14, C21 |
| **Host Boundaries** | Asyncify / JS plants | No intermediate host layer | False greens, dropped interrupts | C14, C15 |
| **Lifecycles** | Worker instance hot-reuse | Clean power-on reset states | Residual state mistaken for cold boot | C13, C23 |
| **Resource Topology** | Overlooked mutual exclusion | Hardware resource exclusivity | Pin/timer conflicts discovered only on board | C17 |
| **Memory / ABI** | Large heap, relaxed alignment | Constrained SRAM, strict alignment | OOM / struct padding escapes | C6, C12, C24, C25 |

### 3.2 Phased Evolution Milestones

Testability statuses are maintained in [08](./08-simulation-consistency-checklist.md) as SSOT.

| Phase | Core Capabilities | Fidelity Level | Primary Categories Covered |
|---|---|---|---|
| **Phase 1 (MVP+)** | Virtual clocks, physics injection, fast-forward contract enforcement | Virtual-Time Fidelity | C1, C2, C13, C14, C15 |
| **Phase 2 (Wave A)** | Heap quotas, Sanitizers, stack watermarks | Resource Parity | C6, C25 |
| **Phase 3 (Wave B)** | Homologous drivers, asynchronous DMA, bus fault state machines | Protocol Parity | C7, C8, C18, C19 |
| **Phase 4 (Wave C)** | Chaotic scheduler, TSan, soft WDT, OS synchronization validation | Temporal Parity | C3, C4, C5, C9, C16, C20 |
| **Phase 5 (Wave D)** | Instruction / ABI dual-track validation | Instruction Parity | C12, C24 |
| **Continuous / On-Demand** | Resource conflict gates, NVS tearing, low-power contracts | — | C17, C21, C22, C23 |
| **Hardware / HIL** | Multi-core, hard real-time, SPICE, caches/DMA RAM | — | C9, C10, C11, C22, C24 |

### 3.3 CI Test Tiering Strategy (Test Tiers)

To balance rapid PR feedback with deep nightly bug hunting across C1–C25 categories and 80+ sub-scenarios:

| Test Tier | Response Target | Trigger Event | Core Categories Covered | Validation Mechanism |
|---|---|---|---|---|
| **Tier 1 (PR Gate Track)** | $< 15\text{s}$ | Every Git Push / PR | C1, C2, C5.1, C6 (ASan/Heap Quotas), C13.1 (Cold Boot), C14.1 (Single Clock Writer), C15 (Fail-Loud) | Static compilation assertions, ultra-fast HEADLESS unit tests, ASan Pass 1 |
| **Tier 2 (Nightly Chaos Track)** | $< 15\text{min}$ | Nightly CI Scheduled Job | C3 (Chaos + TSan Shadow Memory), C4 (Interrupt Preemption), C7, C8 (Asynchronous DMA), C16 (OS Semantics), C18, C19 | Multi-seed chaotic scheduler regressions, TSan concurrency detection, bus fault injection |
| **Tier 3 (Deep / HIL Track)** | On-Demand / Weekly | Pre-Release / Hardware Pipeline | C10 (FOC Motor HIL), C12 (RISC-V 32 Interpreter Binary Diff), C22 (Low Power), C24 (DMA RAM) | Instruction-level interpreter dual-track trace diffs, automated physical HIL board test regressions |

### 3.4 Sub-Scenario Maintenance Governance

1. **Adding New Sub-Scenarios**: Author all 5 fields in §2 of this specification first, then add the testability row to 08 (defaulting to ❌/🟡) with back-links.
2. **Engine Deliveries**: Update implementation tracking and the 08 status column; updating 08 without updating this specification is prohibited.
3. **Intentionally Unmodeled Domains**: Must appear simultaneously in this specification's boundary fields and [08 §Explicit Non-Goals](./08-simulation-consistency-checklist.md).
4. **Prohibited**: Repeating lengthy sub-scenario narratives inside 08.

---

## Revision History

| Date | Description |
|---|---|
| 2026-07-31 | Initial draft: C1–C12 scenario assurance and Phase milestones |
| 2026-07-31 | Upgraded to C1–C25 category + sub-scenario contracts (Problem / Hardware Divergence / Solution / Oracle / Boundary); completed fast-forwarding, host boundaries, lifecycles, OS semantics, resource conflicts, bus faults, DMA lifecycles, wrap-arounds, low-power/NVS/cache/UB escape clusters |
| 2026-07-31 | Added Oracle acceptance vocabulary compilation and Tier 1~3 CI test tiering strategy table |
