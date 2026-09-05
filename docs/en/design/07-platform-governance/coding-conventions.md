# Coding Conventions & Pragma Rule Matrix

<!-- i18n-meta
source: docs/zh/design/07-platform-governance/coding-conventions.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

> **Source of Truth**: Platform conventions.  
> Blocking pragma matrix: [ADR-0025](../../decisions/core/0025-app-blocking-api-honesty-pragma-convention.md).  
> Role/BAL operation naming (A/B/C): [ADR-0032](../../decisions/core/0032-bal-role-operation-naming-classes.md) → **§3 below**.  
> BAL domain / filename / dependencies / CI: [06-bal-layer.md](../02-wink-micro-os/06-bal-layer.md) (SSOT) ← [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md) → **§4 below**.

---

## 1. Pragma Rule Matrix for Blocking APIs

To ensure honesty in blocking API usage and prevent silencing real bugs, compiler warning suppressions (pragmas) must strictly follow the matrix below.

| Location | Blocking Allowed? | Pragma Usage | Required Comments & Usage |
|:---|:---:|:---|:---|
| **BAL `.c` Entire TU**<br>(e.g., `wink_ultrasonic_poll.c` calling `dal_ultrasonic_request_measurement`) | ✅ Yes (Legitimate) | File-level `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` | Wrap implementation section **after** includes.<br>Must document with:<br>`/* ADR-0017 BAL-exception: periodic MAY_BLOCK path calls WINK_BLOCKING API */` |
| **BAL `.c` LIGHT Path**<br>(e.g., `wink_button_events.c`, `wink_led_blink.c`) | ❌ No | **No pragma allowed** | If there are blocking calls, they must be refactored or moved to `MAY_BLOCK`. |
| **Runtime `.c` Internal**<br>(e.g., `wink_selftest.c`, `wink_runtime.c`) | ✅ Yes | `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` | For internal runtime blocking paths. |
| **`app_init_status()` / `app_on_fault_status()`**<br>(e.g., `wink_selftest_run`, I2C scan) | ✅ Yes | Block-level `WINK_INIT_BLOCKING_REGION_BEGIN/END` | Wrap only the specific blocking diagnostic lines.<br>Must document with:<br>`/* ADR-0017 init-phase exception: selftest runs during synchronous boot */` |
| **App Event Callbacks**<br>(e.g., `on_button_click`, event handlers) | ❌ No | **Prohibited** | Blocking here is a bug. Do not suppress warnings. |
| **`app_loop()`** | ❌ No | **Prohibited** | Must use BAL helper for asynchronous execution. |
| **Bringup/Selftest Instrumentation**<br>(e.g., `wink_sim_ultrasonic_echo`) | ✅ Yes | `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` | Must be isolated inside `runtime/selftest/` and wrapped with `#ifndef WINK_STRICT_NONBLOCKING`. |

---

## 2. Platform CI Gates & Lint Checks

To prevent architectural degradation, CI gates enforce the following boundaries:
1. **BAL Layer Boundary**:
   - Grep constraint: `bal/include/**/*.h` must **never** contain `#include "pal_*.h"` or `#include <pal_*.h>` (except `pal_log.h` because log macros do not leak OSAL/HAL types).
   - Additional BAL gates (math bans DAL, bans `*_helper.h`/`*_controller.h`, bans public `sonar`, PUBLIC includes restricted to `bal/include`, `src/` mirrored): see [06-bal-layer.md §6](../02-wink-micro-os/06-bal-layer.md).
2. **App Layer Pragma Restriction**:
   - Grep constraint: `samples/*/app_callbacks.c` (and `wink-micro-app/*/app_callbacks.c`) must **never** contain raw `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"`. Block-level suppressions must only use the semantic macros `WINK_INIT_BLOCKING_REGION_BEGIN/END`.
3. **Simulation Strict Mode**:
   - The WASM simulation target must compile with `-DWINK_STRICT_NONBLOCKING=1` by default. Under this mode, any blocking APIs are removed from headers to cause compile-time failures on invalid blocking calls.

---

## 3. App Role / BAL Operation Naming Classes (A · B · C)

> **Authoritative Living Specification (SSOT)**. Decision: [ADR-0032](../../decisions/core/0032-bal-role-operation-naming-classes.md).  
> App Role verb list: [01-app-business-logic.md](../03-app-codegen/01-app-business-logic.md) §2.7.  
> Button event driving: [ADR-0031](../../decisions/core/0031-button-event-drive-config.md). Event queue contract: [ADR-0022](../../decisions/core/0022-event-queue-mbox-async-primitives.md).

### 3.0 Rule of Thumb

```text
Primary delivery = Posting to event queue; consumed in app_on_event
    → B: enable_* / disable_*
Otherwise
    → Has background session/periodic activity → A: start / stop
    → Completed in one-shot synchronous call   → C: set / get / request / is / …
```

Classification targets **operations**, not entire device categories. A device may expose **A + B + C** simultaneously (e.g., Servo: `set_angle` + `sweep_start`; Button: `is_active` + `enable_events`).

### 3.1 3-Class Definitions

| Class | Name | Verbs | Primary Mental Model | Relationship with `wink_event` Queue |
|:--|:-----|:-----|:-----------|:---------------------|
| **A** | Activity | `start` / `stop` (Optional `_start_ex`) | "Run a background activity"; stops when stopped | **Usually bypasses** queue |
| **B** | Capability | `enable_*` / `disable_*` | "Enable an event emission capability"; consumed elsewhere | **Primary path posts into** queue → `app_on_event` |
| **C** | Action | `set_*` / `get_*` / `request_*` / `is_*` / `read_*` / `clear` / `draw_*`… | "Perform once right now"; no continuous session | Unrelated to channel state |

**3.1.1 Predicate Convention (P Class)**: Query functions returning boolean values uniformly use prefixes `is_*` / `has_*` / `can_*` (e.g., `wink_button_events_is_debouncing`, `wink_button_has_irq_backend`). Grandfathered names `*_supported`, `*_ready` are preserved.

### 3.2 Cross-Layer Isomorphism (Role ↔ BAL)

Identical operations must share **identical verbs**:

```c
/* Role — App / AI Primary Surface */
user_button_enable_events();

/* BAL — Codegen static inline forwarding; forbidden from changing to *_start */
wink_button_enable_events(&user_button, &cfg);
wink_button_disable_events(&user_button);
```

| Layer | Form | Caller |
|:---|:-----|:-------|
| Role | `{instance}_{verb}()` | App |
| BAL | `wink_<type>_{verb}(dev, …)` | Role Wrapper / L2 |
| Legacy Compatibility | `wink_button_events_start/stop` | `WINK_DEPRECATED` thin wrapper |

### 3.3 Example Matrix (Role Verbs · BAL Symbols)

| Peripheral | A (Activity) | B (Capability → Queue) | C (Action) |
|:-----|:----------|:-----------------|:----------|
| LED | `blink_start` ↔ `wink_led_blink_start` | — (LED is an indicator, not an event source) | `activate` ↔ `dal_led_on` |
| Ultrasonic | `proximity_start` ↔ `wink_ultrasonic_poll_start` | **`enable_distance_events` ↔ `wink_ultrasonic_enable_distance_events`** (ADR-0033) | `request_measurement` / `read_distance` |
| Servo | `sweep_start` ↔ `wink_rc_servo_sweep_start` | (Optional) `enable_fault_events` (future) | `set_angle` ↔ `wink_rc_servo_set_angle` |
| Button | — (A not required) | **`enable_events` ↔ `wink_button_enable_events`** | `is_active` / `was_active` |
| Telemetry | — ↔ `wink_telemetry_default_start` | `enable_fault_events` (future) | — |

### 3.4 Decision Tree

```text
Q1: Is primary delivery posting to wink_event for consumption in app_on_event?
    │
    ├─ YES → B: enable_<capability> / disable_<capability>
    │         (Edges: enable_events; Measurements: enable_distance_events …)
    │
    └─ NO  → Q2: After stopping, does a background task continuously run?
              │
              ├─ YES (Periodic blink, periodic measure, background telemetry) → A: start / stop
              │
              └─ NO  → C: set / get / request / is / read / clear / draw …
```

| Situation | Naming Policy |
|:---|:---|
| Class A activity occasionally posts an event | Main API uses **A `start`**; separate B `enable_*_events` provided when listening to completion events becomes primary path |
| Two naming sets coexisting on same path | Forbidden; legacy name deprecated and deleted |
| Calling button events `*_start` to match blink | Forbidden (Q1) |
| Changing blink to `enable_blink` to match Role | Forbidden (Q2 / A) |

### 3.5 Hard Rules

1. New Role/BAL APIs must declare classification as **A / B / C**.
2. Role ↔ BAL share **identical operations and identical verbs**.
3. Event queue producers $\rightarrow$ **B**; periodic session activities $\rightarrow$ **A**; one-shot actions $\rightarrow$ **C**.
4. BAL public names governed by [06-bal-layer.md](../02-wink-micro-os/06-bal-layer.md).
5. Avoid `register_*` unless the signature takes an explicit handler callback.
6. Ban inverted syntax `events_enable`; mandate `enable_events` / `enable_<noun>_events`.
7. Predicates must use `is_*` / `has_*` / `can_*`.
8. Classifications reflect the App perspective, not underlying driver implementations.

### 3.6 Layer Boundaries

- **DAL** (`dal_*`): Governed by [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md); this section binds **App Role + BAL**.
- **PAL** (e.g., `pal_gpio_enable_interrupt`): Platform hardware primitives may use `enable` independently.

---

## 4. BAL Domain Partitioning & File Naming (Pointer)

> **SSOT**: [06-bal-layer.md](../02-wink-micro-os/06-bal-layer.md). Decisions: [ADR-0037](../../decisions/core/0037-bal-domain-partition-and-closed-loop-motor.md) (3 Domains), [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md) (Hard Cutover).

```text
Zero DAL/Runtime dependencies → math/
Cross-device closed loops or orchestration → control/
Single DAL augmentation → input|output|sensor|actuator|display|comm (Matches primary DAL)

Files: Ban *_helper / *_controller; stem = API prefix; vocabulary matches DAL (ultrasonic)
Verbs: Follow §3 A/B/C
```

---

## 5. C++ Subset & Arduino Compat Sandbox Guidelines

To support Arduino sketches and third-party libraries (e.g., Adafruit) while preserving kernel efficiency, C++ usage must adhere to the following rules (ADR-0035 / ADR-0036).

### 5.1 Compiler Flags & Feature Exclusions
* **`-fno-exceptions`**: Exceptions and `try-catch` are prohibited.
* **`-fno-rtti`**: Run-Time Type Information disabled (`dynamic_cast` and `typeid` prohibited).
* **`-fno-threadsafe-statics`**: Thread-safe local static variables disabled to avoid implicit mutex allocation.
* **`-nostdlib++`**: No C++ standard library (`libstdc++`/`libc++`) linked.

### 5.2 Target and Include Isolation
* **Leaf Target**: `wink_arduino_compat` is a leaf target; kernel targets never link against it.
* **Include Paths**: Arduino Core headers are never exposed to kernel include paths.
* **Forbidden Headers**: Kernel sources (`pal/`, `dal/`, `osal/`) must never include `<Arduino.h>`, `<Wire.h>`, `<SPI.h>`, or C++ standard headers.

### 5.3 Memory Allocation (Arena Heap)
* **Sandbox Heap**: C++ allocations (`new`/`delete`) run on a dedicated static arena `arduino_arena_heap` (32KB default on ESP32).
* **OOM Fail-Fast**: Heap exhaustion triggers `pal_panic(WINK_ERR_OUT_OF_MEMORY)` immediately.
* **Custom Placement New**:
  ```cpp
  inline void* operator new(size_t, void* __p) noexcept { return __p; }
  inline void* operator new[](size_t, void* __p) noexcept { return __p; }
  ```

### 5.4 Bus Concurrency on Shared Buses (I2C/SPI)
* **Mutex Protection**: Shared buses accessed by the Arduino Task and the Wink Event Loop are protected by PAL mutexes (`pal_i2c_lock`/`unlock`).
* **Locking Hierarchy**: Class overrides obtain PAL bus locks during transactions and release them upon completion.
