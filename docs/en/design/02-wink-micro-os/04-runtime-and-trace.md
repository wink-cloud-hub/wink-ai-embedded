# 3.4 Runtime Lifecycle & Golden Trace Contract

<!-- i18n-meta
source: docs/zh/design/02-wink-micro-os/04-runtime-and-trace.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

> ✅ **Architectural Decision Backport**: This document formally lands [ADR-0007](../../decisions/core/0007-cooperative-loop-execution-model.md) (Cooperative Execution Model) and [ADR-0010](../../decisions/core/0010-boot-safe-lock-recovery-threshold.md) (Boot Safe-lock Recovery Threshold) as the Single Source of Truth.

This document realizes the specification in [03-directory-architecture.md](./03-directory-architecture.md) §7: API contracts, callback injection models, and target entry wiring for the two first-class peer layers: runtime and trace.

---

## 1. runtime: Callback-Injected Main Loop

### 1.1 Design Motivation
Historically, `main()` and the OS main loop were buried inside `targets/wasm/pal_hal_wasm.c` without a unified home. The Ports & Adapters (A*) architecture refactors this into a target-agnostic `runtime` layer, where target `*_entry.c` files only serve to launch the runtime.

### 1.2 Callback Injection (Zero extern app_*)
Apps register lifecycle hooks via `wink_app_callbacks_t`. The runtime library holds **no strong extern dependencies** on external `app_*` symbols, achieving binary-level decoupling (simplifying unit tests and mocking).

```c
typedef struct {
    /* New Status-Returning Signatures (Recommended) */
    wink_status_t (*init_status)(void);
    wink_status_t (*on_fault_status)(uint32_t fault_code);
    void          (*on_boot)(void);

    /* Legacy Signatures (Deprecated, kept for compatibility) */
    void          (*init)(void);
    void          (*on_fault)(uint32_t fault_code);

    /* Core execution loop callback */
    void          (*loop)(void);
} wink_app_callbacks_t;

void wink_app_delay_ms(uint32_t ms);
wink_status_t wink_runtime_run(const wink_app_callbacks_t *callbacks, uint32_t max_ticks);
```

- `init_status`/`on_fault_status`/`loop`/`on_boot` etc. are permitted to be NULL.
- `max_ticks`: Host/testing passes a finite value to avoid infinite loops; physical hardware/Wasm passes `0` for indefinite execution.
- If `init_status` returns an error status or `init` fails, the runtime initiates the Fault lifecycle immediately.

### 1.3 Wiring Pipeline
```text
wasm_entry.c::main() / esp32_entry.c::app_main() / host sample main()
        │  Instantiates wink_app_callbacks_t (from App wink_app_get_callbacks())
        └─► wink_runtime_run(&cb, 0 / N)
                   ├─ cb.init()  (once)
                   └─ while(1){ cb.loop(); wink_app_delay_ms(TICK); }
```

---

## 2. trace: Golden Trace First-Class Peer

### 2.1 Positioning
An independent top-level peer layer (not a runtime sub-feature, see 03-directory-architecture.md §3 Screaming Architecture). A cross-cutting foundational service consumed by runtime and App.

### 2.2 Isolation Contract (§6.1 Constraint 2)
DAL/PAL drivers are **forbidden** from directly calling `wink_trace_*`; they return `wink_status_t` only. Fault interception and trace emission converge in App callbacks (`on_fault`) or runtime.

### 2.3 API
```c
#define WINK_TRACE_CAPACITY 32   /* Static ring buffer, zero dynamic allocation */
void wink_trace_reset(void);              /* TASK Context */
void wink_trace_fault(uint32_t code);     /* TASK Context (pal_os_critical_enter/exit) */
void wink_trace_fault_from_isr(uint32_t code); /* ISR Context (pal_os_critical_enter_isr/exit_isr), ADR-0016 */
uint32_t wink_trace_count(void);          /* TASK Context (Diagnostics/query) */
uint32_t wink_trace_last(void);           /* TASK Context; returns 0 if empty */
```

### 2.4 Concurrency Contract (ADR-0016 Task/ISR Dual Entry)

- **`wink_trace_fault` = TASK Context**; `wink_trace_fault_from_isr` = ISR Context; both share the same static ring buffer, with task/ISR mutual exclusion guaranteed by PAL global mux (`portENTER_CRITICAL_ISR` and `portENTER_CRITICAL` share `s_global_mux` on ESP32).
- **`reset/count/last` remain TASK-only**—diagnostic interfaces offer no ISR variants.
- **Synchronous calls to `wink_runtime_fault` are prohibited in ISRs**: ISRs execute static logging only (`wink_trace_fault_from_isr`), while Safe-off shutdown chains are deferred to the TASK layer (main loop tick reclamation window).
- Under Host/Wasm single-threaded simulation, `pal_os_set_sim_isr_context(true/false)` wraps ISR boundaries; buffer/head/count states are verified to be **bit-for-bit identical** via `test_wink_trace_isr_equivalence`.

---

## 3. Fail-Safe / WDT / Actuator Registry (Phase 5, P0-4)

### 3.1 Actuator Registry
The runtime maintains a static registry (`wink_actuator_registry.h`, capacity 16, zero dynamic allocation). Actuator DAL drivers register their **semantically correct safe-off callbacks** during init; fault/panic/boot-lock paths invoke `wink_actuator_safe_off_all` to shut down all actuators (iterating completely even if an individual item fails).

```c
typedef wink_status_t (*wink_actuator_safe_off_fn)(void *ctx);
wink_status_t wink_actuator_register(wink_actuator_safe_off_fn fn, void *ctx);  /* Idempotent */
void wink_actuator_safe_off_all(void);   /* Non-blocking guarantee */
```

> ⚠️ Safe-off semantics: `duty=0` equals limp/safe for servos, but may mean coast rather than brake for DC motors. Each actuator must register its own semantically correct shutdown.
> ⚠️ Default hardware-level safe states (Hi-Z weak pull-down, actuator enable defaults, power gating) **must be guaranteed by board circuitry**—software cannot handle CPU lockups or WDT resets; the registry completes software closed-loop protection only.

### 3.2 3-Phase Fault Model & Initialization Rollback Contract (ADR-0024)

The system adopts a **3-Phase Fault Model** with strictly bounded execution times and context constraints:

```text
[Fault Detected / init_status Non-OK]
                  │
                  ▼
┌──────────────────────────────────────────────┐
│ Phase 1: Safe-Off (Hard Real-Time Shutdown)  │  Time: ≤100µs, Context: Task/ISR
│ - Calls wink_actuator_safe_off_all()         │  Allowed: Non-blocking write, GPIO reset
│ - Halts all physical actuator outputs        │  Prohibited: Mutex, print, task create/stop
└─────────────────┬────────────────────────────┘
                  │
                  ▼
┌──────────────────────────────────────────────┐
│ Phase 2: App Callback & Graceful Stop        │  Time: ≤500ms, Context: Fault Task
│ - Calls callbacks->on_fault_status/on_fault  │  Allowed: Log, Telemetry, sequence stops
│ - Graceful business logic shutdown           │  Prohibited: Infinite loops, blocking >50ms
└─────────────────┬────────────────────────────┘
                  │
                  ▼
┌──────────────────────────────────────────────┐
│ Phase 3: Hardware Watchdog Reset             │  Time: ~1000ms, Context: Hardware WDT
│ - Hardware-level total system reboot         │  Purpose: Restart from clean BSS
└──────────────────────────────────────────────┘
```

#### 3.2.1 Fault Phase Rules
1. **Phase 1 · Safe-Off (Hard Real-Time Fast Shutdown)**:
   - **Trigger Context**: Task or ISR detects fault and calls `wink_runtime_fault` synchronously.
   - **Time Budget**: Hard limit **≤100µs**.
   - **Operational Limits**: Only lock-free, non-blocking hardware operations are allowed (`pal_gpio_write`, `pal_gpio_reset_pin`). **Strictly forbids** APIs that may block (Mutex/Sem, heap allocations, `printf`/`LOG`, or task creation/deletion).
   - **Objective**: Fastest physical shutdown of all actuators via `wink_actuator_safe_off_all`.
2. **Phase 2 · App Callback (Application Graceful Shutdown)**:
   - **Trigger Context**: Dedicated Fault Task context (Medium-priority control task).
   - **Time Budget**: Limit **≤500ms**.
   - **Operational Limits**: Allowed to invoke callbacks (`on_fault_status`), write flash logs, transmit telemetry alerts, and gracefully stop BAL services.
3. **Phase 3 · WDT Reset (Hardware Reboot)**:
   - **Objective**: If an unrecoverable fault occurs or Phase 2 hangs, the hardware watchdog forces a reset within ~1s, resetting all corrupted software state.

#### 3.2.2 Init Failure "Creator Rolls Back" Contract

To prevent system hangs or resource leaks when peripheral initialization fails in `device_tree_init()`:
1. **Complete Local Rollback**: Any DAL `init` claiming multi-step resources (GPIO/PWM) must release all previously claimed resources internally before exiting upon any intermediate failure.
2. **Deterministic System Action**: When `device_tree_init()` encounters a device failure, the runtime immediately executes Phase 1 to shut down all registered actuators and triggers an init fault. The runtime does not attempt to guess complex bus dependencies to deinit other peripherals; WDT hardware reset serves as the final cleanup fallback.

### 3.3 PAL WDT / Reset Reasons (`pal_osal.h`)
```c
typedef enum { PAL_RESET_REASON_UNKNOWN, PAL_RESET_REASON_POWER_ON,
               PAL_RESET_REASON_WATCHDOG, PAL_RESET_REASON_PANIC } pal_reset_reason_t;
pal_reset_reason_t pal_get_reset_reason(void);
wink_status_t pal_watchdog_init(uint32_t timeout_ms);
wink_status_t pal_watchdog_feed(void);
```

### 3.4 Boot Safe-Lock (Consecutive Reset Count + Recovery, ADR-0010)
`wink_runtime_run` reads `pal_get_reset_reason()` and persistent abnormal boot counters (`pal_get/set_abnormal_boot_count`, stored in `RTC_NOINIT` on ESP32):
- `POWERON` → Counter cleared (Clean cold boot);
- `WATCHDOG`/`PANIC` → Counter incremented:
  - **≥ `WINK_BOOT_LOCK_THRESHOLD` (3)** → **System Locked**: Triggers `wink_runtime_fault` (trace 8001 + safe-off + on_fault) and returns `WINK_ERR_LOCKED`, skipping `cb.init()/cb.loop()`.
  - **< Threshold** → **Recovery Permitted**: Runs `cb.init()/cb.loop()` normally (isolated resets recover automatically without tracing).
- When tick loop runs for `WINK_BOOT_HEALTHY_TICKS` (200 ticks, ≈2s) → Counter cleared (Confirms stable runtime).

### 3.5 Soft Timer Scheduler (ADR-0007, Validated on Hardware)
ADR-0007 replaces single monolithic 10ms polling with a **Static Soft Timer Scheduler**: components register callbacks at distinct frequencies (1ms motor PID, 20ms ultrasonic, 500ms telemetry), dispatched on Tick boundaries.

```c
typedef enum { WINK_TIMER_ONESHOT, WINK_TIMER_PERIODIC } wink_timer_mode_t;
typedef wink_status_t (*wink_soft_timer_callback_t)(void* arg);
wink_status_t wink_soft_timer_init(void);
int32_t       wink_soft_timer_create(cb, arg, mode, period_ms);
wink_status_t wink_soft_timer_start(int32_t handle);
wink_status_t wink_soft_timer_stop(int32_t handle);
void          wink_soft_timer_dispatch(void);
```
- **Zero Dynamic Allocation**: `WINK_MAX_SOFT_TIMERS` slots allocated statically.
- **Tick Alignment**: `period_ms` must be an integer multiple of `WINK_RUNTIME_TICK_MS`.
- **Per-Callback WCET**: Each callback is timed independently to prevent false 8002 alarms.

---

## 4. Multi-Core Physical Isolation & Inter-Core Escape Communication (ADR-0007)

For dual-core MCUs (ESP32), ADR-0007 enforces **asymmetric physical core isolation**, forbidding automatic coroutine scheduling across multiple cores (preventing multithreaded lock contention that breaks Wasm simulation parity):

| Core | Role | Description |
|---|---|---|
| **Core 0 (System Comm Core)** | Wi-Fi / BT / lwIP stack, OS background | Isolates network interrupt jitter to Core 0 |
| **Core 1 (Application Control Core)** | Exclusively runs `wink_runtime_run` cooperative main loop | Extremely deterministic timing, minimal jitter |

- **Core Affinity API**: `pal_task_create(..., pal_core_id_t core_id, ...)` (`PAL_CORE_0` / `PAL_CORE_1` / `PAL_CORE_ANY`); runtime pins to Core 1.
- **Inter-Core Escape Hatch**: Heavy background compute (AI vision) spawns background tasks on Core 0, communicating **exclusively** via OSAL non-blocking lock-free ring buffers:
```c
typedef struct pal_ringbuf* pal_ringbuf_handle_t;
pal_ringbuf_handle_t pal_ringbuf_create(uint32_t size);
wink_status_t pal_ringbuf_push(pal_ringbuf_handle_t rb, const void* data, uint32_t size);
wink_status_t pal_ringbuf_pop(pal_ringbuf_handle_t rb, void* data, uint32_t size);
```
  Core 1 polls this queue lock-free at Tick boundaries without cross-core blocking synchronization.

### 4.4 MPU / Linux Multi-Core Deployment & Concurrency Isolation

On MPU/Linux platforms (ARM Cortex-A, x86), WinkOS runs as a lightweight control sandbox in user space:

#### 4.4.1 In-Process Concurrency (Device-Level Multitasking)
* **Thread Isolation**: The WinkOS main loop runs as a high-priority POSIX thread pinned to a dedicated CPU core (`pthread_setaffinity_np`). Background compute tasks spawned via `pal_os_task_create` run as normal Linux threads.
* **Communication Limits**: Background threads and the main loop **never share mutexes**. All exchanges occur through lock-free `pal_os_ringbuf` queues, matching the ESP32 memory model.

#### 4.4.2 Multi-Process Sandbox Isolation (System-Level Distributed)
* **Process Isolation**: Multiple independent WinkOS binary processes execute on Linux, each running a minimal, single-threaded cooperative entity with independent virtual memory spaces.
* **IPC**: Inter-process communication utilizes Unix Domain Sockets or MQTT. In browser simulation, each process instantiates as an independent Wasm module for distributed multi-node simulation.

---

## 5. Roadmap

- Golden Trace hardware vs simulation replay/compare in CI.
- `WINK_PT_*` stackless protothreads macro syntax wrappers.
- High-speed hard real-time closed-loop FOC / Fast PID scheduling integrations.
