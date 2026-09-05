# 4.6 Physical Degradation Engine & Fault Injection (WASM Wave 2 Design Backport)

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/archive/06-physical-degradation-engine.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

> **Status**: Accepted (Living Spec) · **Updated**: 2026-06-29
>
> This document is the design backport of [ADR-0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md)
> "Physical Behavior Simulation & Fault Injection Architecture" to the Wasm simulation target (`targets/wasm`),
> while incorporating the Wasm-side landing of [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md) Decision 3
> "Virtual Clock". Following this backport, this document serves as the
> Single Source of Truth (SSOT) for Wasm-side physical degradation and virtual clocks.
>
> **Numbering Note**: This document uses the available `06-` index in the directory. In the original ADR-0009 Wave 2 plan,
> the slot designated `02-physical-degradation-engine.md` was occupied by `02-virtual-peripheral-registry.md`
> (due to early documentation naming), so it has been deferred to `06-` to avoid breaking existing cross-references.

---

## 0. Applicable Scope & Out of Scope

**Applicable Scope**:
- OSAL/HAL adaptation layer for the Wasm simulation target (`wink-micro-os/targets/wasm/`).
- Target-independent algorithm library (`wink-micro-os/targets/common/src/wink_sim_physical.c`), shared
  between the host PoC (Wave 1) and the Wasm sandbox.
- Browser-side UniSim Worker bridge (`@wink-ai/unisim`).

**Out of Scope**:
- ESP32 / baremetal physical hardware targets—**Zero Compilation Contamination** (ADR-0009 §4.3): The above algorithm source files
  and symbols must not appear in hardware CMake build trees or final ELF binaries. Verification method:
  `grep -r "wink_sim_physical\|pal_wasm_physical" wink-micro-os/targets/esp32 wink-micro-os/targets/baremetal`
  must produce zero output.
- Electrical-level SPICE simulation—Covered strictly by physical hardware (see ADR-0003 §Decision 1 boundary declarations).
- Preemptive multitasking scheduling (FreeRTOS simulation)—Belongs to the ADR-0003 Decision 3 roadmap, detailed in
  [`05-simulation-consistency-and-fidelity-spec.md`](./05-simulation-consistency-and-fidelity-spec.md).

---

## 1. Overall Architecture (Hybrid Double-Domain)

```text
 ┌─────────────────────────── UI Main Thread (Vue 3) ────────────────────┐
 │  Canvas Interaction, 3D Scene, Fault Tuning Sliders                   │
 └────────────────────────────────▲──────────────────────────────────────┘
                                  │ postMessage (UI ↔ Worker)
                                  ▼
 ┌────────────────────── UniSim Worker (TS) ─────────────────────────────┐
 │  VirtualClock(bigint)  ──────►  WasmPhysicalBridge  ─────► SimWorker  │
 │     │                                │                          │     │
 │     │  Sole Clock Ownership          │ cwrap setters             │     │
 │     │                                ▼                          ▼     │
 │     └─────►  pal_wasm_advance_virtual_clock(us:bigint)   …      …     │
 └────────────────────────────────▲──────────────────────────────────────┘
                                  │ wasm exports / imports (bigint ABI)
                                  ▼
 ┌──────────────────── wasm Sandbox (C, Emscripten) ─────────────────────┐
 │                                                                       │
 │  ┌─ pal_osal_wasm.c ───────────────────────────────────────────────┐  │
 │  │  s_virtual_us  ←—— pal_wasm_advance_virtual_clock(us) (Sole)    │  │
 │  │  pal_get_us / pal_get_ms (Pure reads, zero JS calls)            │  │
 │  │  pal_delay_ms/us: Asyncify suspend only, **no active clock step**│  │
 │  └─────────────────────────────────────────────────────────────────┘  │
 │                                                                       │
 │  ┌─ pal_wasm_physical.c ──────────────────────────────────────────┐   │
 │  │  faults POD ({0} == ideal)  +  PRNG state  +  per-pin ctx[128]  │   │
 │  │  Export setters: pal_wasm_set_{bounce_us,warmup_us,...}        │   │
 │  │  Export reset: pal_wasm_reset_physical()                       │   │
 │  └────────────────────────────────────────────────────────────────┘   │
 │                                                                       │
 │  ┌─ pal_hal_wasm.c (GPIO/I2C Middleware Layer) ───────────────────┐   │
 │  │  pal_gpio_read: Transparent debounce superposition (DAL blind) │   │
 │  │  pal_i2c_transfer: PRNG hit threshold returns WINK_ERR_TIMEOUT │   │
 │  └────────────────────────────────────────────────────────────────┘   │
 │                                                                       │
 │  ┌─ targets/common/src/wink_sim_physical.c (Algorithm SSOT) ──────┐   │
 │  │  Debounce state machine / RC lowpass + Gaussian noise / Dropping│   │
 │  │  Shared between host PoC and Wasm; byte-level golden alignment │   │
 │  └────────────────────────────────────────────────────────────────┘   │
 └───────────────────────────────────────────────────────────────────────┘
```

**Dual-Domain Division of Responsibilities**:
- **JS Domain**: Ideal physical states (button pressed, distance 32cm, temperature 25°C) + fault configurations + clock control;
  dispatched via messaging protocols without micro-timing simulation.
- **C/Wasm Domain**: Signal degradation, debouncing, packet drops, and noise processed **locally** in algorithms; clock reads
  execute in pure local memory with zero cross-boundary overhead.

---

## 2. Virtual Clock SSOT Architecture (ADR-0003 Decision 3 Landing)

### 2.1 Design Principles

| Principle | Implementation |
|---|---|
| **Single Source of Truth** | `targets/wasm/pal_osal_wasm.c::s_virtual_us` (uint64_t) is the sole owner of Wasm-side clocks, zero-initialized in BSS. |
| **Sole Write Entry Point** | `pal_wasm_advance_virtual_clock(uint64_t us)`, exported via `EMSCRIPTEN_KEEPALIVE`. |
| **Sole Writer Entity** | JS Worker (`SimWorker.STEP_CLOCK`). All other execution paths (including `pal_delay_ms/us`) are **forbidden** from calling this function. |
| **Read Entry Points** | `pal_get_us()` / `pal_get_ms()` — Pure memory access, zero JS round-trips. |
| **Type Contract** | uint64_t $\leftrightarrow$ JS `bigint`, CMake `-s WASM_BIGINT=1`; end-to-end `bigint` in TypeScript, banning implicit `number` conversions. |
| **Overflow Protection** | uint64_t natural rollover $> 580$ years; unreachable under simulation workloads. |

### 2.2 Architectural Red Line (Statically Verifiable)

> **Inviolable Rule**: The function bodies of `pal_delay_ms()` and `pal_delay_us()` are **strictly forbidden**
> from calling `pal_wasm_advance_virtual_clock()`. The causal chain of time progression is "JS Worker steps the clock
> prior to resuming the Wasm coroutine $\rightarrow$ Wasm resumes and observes the new time", unidirectional and singular.
> If C-side delay functions step the clock internally, "dual stepping / causal inversion" occurs, corrupting reproducibility.

**Grep Verification Script** (Recommended for CI integration):

```bash
# Extract pal_delay_ms and pal_delay_us bodies, asserting zero advance calls
awk '/^void pal_delay_(ms|us)/,/^}/' \
    wink-micro-os/targets/wasm/pal_osal_wasm.c \
  | grep "pal_wasm_advance_virtual_clock" \
  && { echo "SSOT violation"; exit 1; } || echo "SSOT clean"
```

### 2.3 JS Mirror (`VirtualClock.ts`)

The JS side maintains an equivalent `bigint` counter used for:
1. Scheduling (determining `us` delta for `STEP_CLOCK`).
2. Timeline replay UI (avoiding high-frequency cross-Wasm reads).

The JS and Wasm counters **do not reconcile continuously** (Wasm remains the arbiter), but because all writes
originate from the Worker, they remain synchronized. Forced synchronization at any instant is achieved via
`pal_wasm_reset_physical()` + `VirtualClock.reset()`.

---

## 3. Degradation Engine (`pal_wasm_physical.c`)

### 3.1 Global State Layout (BSS)

```c
/* targets/wasm/pal_wasm_physical.c */
#define WASM_SIM_MAX_PINS 128        /* Covers ESP32-S3(49) / Cortex-M(<100) */

static wink_sim_faults_t        s_faults;      /* {0} == ideal direct-pass */
static uint32_t                 s_prng;        /* Global PRNG (§4.2) */
static wink_sim_bounce_ctx_t    s_pin_ctx[WASM_SIM_MAX_PINS];
```

- **Zero Dynamic Memory**: All state resides in BSS with zero `malloc`; initial values rely on C11 §6.7.9 p10 zero-initialization guarantees.
- **`faults = {0}` Equals Ideal**: Pass-through behavior, representing the zero-overhead default state when fault injection is disabled,
  and serving as the built-in fallback for §8.

### 3.2 Exported APIs (C $\rightarrow$ JS)

All setters are tagged with `EMSCRIPTEN_KEEPALIVE` and invoked from JS via `cwrap`.

| Symbol | Parameter $\leftrightarrow$ JS Type | Purpose |
|---|---|---|
| `pal_wasm_advance_virtual_clock` | `uint64_t` $\leftrightarrow$ `bigint` | Sole clock write entry point (§2) |
| `pal_wasm_set_bounce_us` | `uint32_t` $\leftrightarrow$ `number` | GPIO contact bounce duration |
| `pal_wasm_set_warmup_us` | `uint32_t` $\leftrightarrow$ `number` | Sensor thermal warmup delay |
| `pal_wasm_set_sample_interval_us` | `uint32_t` $\leftrightarrow$ `number` | Minimum sampling interval constraint |
| `pal_wasm_set_adc_noise_v` | `float` $\leftrightarrow$ `number` | ADC Gaussian noise amplitude |
| `pal_wasm_set_rc_tau_s` | `float` $\leftrightarrow$ `number` | RC first-order filter time constant |
| `pal_wasm_set_i2c_drop_permil` | `uint16_t` $\leftrightarrow$ `number` | I2C packet drop threshold (parts per thousand) |
| `pal_wasm_set_prng_seed` | `uint32_t` $\leftrightarrow$ `number` | PRNG seed (deterministic entry point) |
| `pal_wasm_get_prng_state` | Returns `uint32_t` | Current PRNG state (for regression assertions) |
| `pal_wasm_reset_physical` | — | Resets faults / PRNG / per-pin ctx |

### 3.3 Memory Safety: WASM_SIM_MAX_PINS Boundary Checks

All per-pin access points (HAL middleware `pal_gpio_read` and setters) must enforce:

```c
if ((unsigned)pin >= WASM_SIM_MAX_PINS) {
    /* Out-of-bounds: HAL treats as "no degradation on pin" (pass-through); setter acts as no-op */
    return /* NULL or default */;
}
```

Intent: When malicious or debugging JS supplies out-of-bounds pin numbers, **zero BSS out-of-bounds writes occur**, and simulation behavior remains observable.

---

## 4. Fault Injection Layering (Mandatory Discipline, ADR-0009 §3.0)

| Layer | Processing Location | Fault Category | Transparent to Upper Layers? |
|---|---|---|---|
| **L1 PinManager Middleware** | `pal_hal_wasm.c` | GPIO disconnection, bounce, pull-up/down failure, Hi-Z | ✅ |
| **L2 Bus Controller Middleware** | `pal_hal_wasm.c::pal_i2c_transfer` etc. | I2C ACK loss, SPI bitflips, bus timeouts | ✅ |
| **L3 Peripheral Driver Stubs (Business)** | `dal/*_sim.c` | Sensor out-of-range, motor stall, EEPROM bad blocks | ❌ (Explicit) |

**Prohibited Anti-Patterns**:
- ❌ DAL drivers manually simulating disconnections inside `attachEvents/read/write`.
- ❌ Individual peripherals implementing bespoke bounce / noise / packet drop logic.
- ❌ DAL invoking `pinManager.setDriverLevel()` to simulate faults.

### 4.1 Contact Bounce Model (Forced Toggle on Sampling)

> **Design Revision (Wave 1 Validation)**: The `(now/1000)%2` model from ADR-0009 §3.1 silently failed under
> the default `WINK_RUNTIME_TICK_MS=10` sampling period (quotient incremented by 10 per tick, remaining constantly even).
> It has been updated to "Forced Toggle on Every Sample" (`bounce_flip ^= 1`), providing period-independent, rigorous,
> and 100% deterministic bouncing. RC noise and bus packet drops continue using PRNG.

### 4.2 Global PRNG Architecture (Intentional Design)

The single global PRNG instance `s_prng` is an **architectural choice, not a defect**:
- ADR-0009 §4.1 "Single seed 100% reproducibility" contract requires a one-to-one mapping from seed to complete trajectory.
- Independent per-peripheral PRNGs cause seed explosion and dramatically increase reproduction costs.
- Future per-peripheral isolation needs will evolve into "derived sub-streams (`seed = hash(global_seed, peripheral_id)`)".

---

## 5. Cross-Language Contract (JS $\leftrightarrow$ Wasm)

### 5.1 BigInt ABI

CMake linking must enable `-s WASM_BIGINT=1`, otherwise:
- C-side `uint64_t` parameters / return values are truncated into two `i32` values.
- If JS accidentally passes a `number` to a `bigint` export, Emscripten throws a `TypeError`,
  acting as a runtime defense line alongside TypeScript compile-time type checking.

### 5.2 Worker Message Protocol (`SimWorker.ts`)

| Request | Fields | Wasm Invocation |
|---|---|---|
| `INIT` | — | Binds Module + resets `VirtualClock` + `pal_wasm_reset_physical` |
| `SET_FAULTS` | `faults: SimFaultsConfig` | Batch-invokes all `pal_wasm_set_*` setters |
| `STEP_CLOCK` | `us: bigint` | `pal_wasm_advance_virtual_clock(us)` |
| `SET_GPIO_IDEAL` | `pin, level` | Writes Wasm-side ideal pin level (HAL applies degradation on read) |
| `READ_GPIO_DEGRADED` | `pin` | `pal_gpio_read(pin)` (includes bounce) |
| `TEST_I2C_TRANSFER` | `port, devAddr, writeBuf, readLen` | `pal_i2c_transfer()` (includes packet drop) |

Each message carries an `id: number` to correlate responses (allowing frontend `await` round-trips).

---

## 6. Test Matrix & SSOT Static Assertions

| Layer | Test Case | Tooling |
|---|---|---|
| L0 Compilation | Successful compilation across wasm / host / esp32 / baremetal targets; `tsc --noEmit` zero warnings | CMake + Ninja + tsc |
| L0.5 Static Architecture | **`pal_delay_ms` body contains zero calls to `pal_wasm_advance_virtual_clock`** (§2.2 grep); `-s WASM_BIGINT=1` present in link options; `WASM_SIM_MAX_PINS` boundary checks present | grep script |
| L1 Unit Tests (C) | Algorithm library golden vectors (`test/common/test_physical_golden.h`) byte-identical across host and Wasm; virtual clock monotonicity; fault setter loopbacks; pin boundary checks (0 / 127 / 128 / UINT16_MAX) | Unity (host + Wasm Node runtime) |
| L1 Unit Tests (TS) | `VirtualClock` bigint boundaries and negative value rejection; `WasmPhysicalBridge` setter invocation order; `SimWorker` message dispatch | Jest |
| L2 Integration | Button debounce end-to-end (`test_button_debounce_e2e_wasm.c` $\leftrightarrow$ host `test_button_debounce_e2e.c` byte-level parity) | Unity |
| L3 Determinism | Identical seed + identical inputs $\rightarrow$ byte-identical output across 1000 consecutive runs with zero drift | Custom script |

---

## 7. Zero Compilation Contamination Verification (Continuous)

```bash
# Physical hardware directories must never reference the degradation engine
grep -r "wink_sim_physical\|pal_wasm_physical" \
    wink-micro-os/targets/esp32 \
    wink-micro-os/targets/baremetal
# Expected: zero output
```

The algorithm library `targets/common/src/wink_sim_physical.c` is compiled solely into `pal_host` and `pal_wasm`
CMake OBJECT libraries; esp32 / baremetal CMakeLists explicitly enumerate source files, preventing accidental inclusion.

---

## 8. Fallbacks & Rollback Strategies

1. **Runtime Fallback**: `SET_FAULTS` dispatches an all-zero configuration (`SimFaultsConfig` equivalent to `{0}`) $\rightarrow$ all
   degradation algorithms bypass after threshold checks, acting as direct pass-throughs without recompilation.
2. **Git Rollback**: `git revert` all Wave 2 commits $\rightarrow$ returns to baseline; algorithm library sources are retained
   (adopted in host pilot), while Wasm-side setters and middleware disappear, reverting behavior prior to Wave 1.
3. **Compile-Time Pruning**: Remove `pal_wasm_physical.c` from `pal_wasm` CMake and revert degradation superposition
   in `pal_gpio_read` / `pal_i2c_transfer` $\rightarrow$ degradation pathways disappear completely.

---

## 9. Reference Links

- [ADR-0009](../../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md) (Hybrid dual-domain architecture, §4.1 determinism guard, §4.3 zero compilation contamination)
- [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md) (Decision 3: Virtual clock landing)
- [ADR-0002](../../../decisions/unisim/0002-dual-target-compilation.md) (Dual-target homologous compilation, wasm32 / xtensa compatibility)
- [Wave 1 Plan](../../../implementation-plans/unisim/2026-06-28-adr-0009-host-pilot-physical-sim-wave1-plan.md)
- [Wave 2 Plan](../../../implementation-plans/unisim/2026-06-29-adr-0009-wasm-physical-sim-wave2-plan.md)
- Source Code Entry Points:
  - C: `targets/wasm/pal_osal_wasm.c`, `pal_wasm_physical.c`, `pal_hal_wasm.c`, `wasm_bridge.h`
  - Algorithm Library SSOT: `targets/common/src/wink_sim_physical.c`
  - TS: `@wink-ai/unisim` (VirtualClock, WasmPhysicalBridge, SimWorker)
