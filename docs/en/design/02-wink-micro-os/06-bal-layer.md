# 06. BAL (Business Abstraction Layer) Design Specification

<!-- i18n-meta
source: docs/zh/design/02-wink-micro-os/06-bal-layer.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

> **This document is the authoritative living specification (SSOT) for BAL directory structure, naming conventions, dependencies, implementation patterns, and CI quality gates.**  
> Decision History: [ADR-0023](../../decisions/core/0023-bal-business-abstraction-layer.md) (Layer Establishment), [ADR-0032](../../decisions/core/0032-bal-role-operation-naming-classes.md) (A/B/C Verb Classes), [ADR-0037](../../decisions/core/0037-bal-domain-partition-and-closed-loop-motor.md) (3 Domains + Closed Loops), [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md) (Naming Hard Cutover), [ADR-0047](../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md) (FOC ISR Invocation of BAL Fast Loops).  
> Operational verb specifics are documented in [coding-conventions.md §3](../07-platform-governance/coding-conventions.md).

---

## 0. Goals & Non-Goals

| | Content |
|---|---|
| **Goals** | Use a fixed decision tree to determine component placement, naming, dependencies, and implementations; mechanically verifiable via CI. |
| **Non-Goals** | Does not alter DAL/PAL contracts; does not introduce vtables; does not expose PAL types in BAL public headers. |
| **Migration Strategy** | Hard cutover (ADR-0038): No legacy public symbols or `_helper` / `_controller` filenames remain in the repository. |

---

## 1. System Layer Position

```text
App  →  BAL  →  { DAL, runtime }
              ↘ math does not depend on DAL/runtime
```

- BAL public headers **must not** `#include` any `pal_*.h` (Sole exemption: `pal_log.h` for logging macros only).
- BAL `.c` sources may include PAL headers for critical sections, timestamps, or logging without leaking PAL types into public headers.
- Static dispatch (ADR-0004): Plain Old Data (POD) structs + named APIs; device abstractions avoid `ops` tables or vtables.

---

## 2. 3-Domain Directory Tree (Frozen)

```text
wink-micro-os/bal/
├── include/
│   ├── wink_bal_opts.h              # Cross-domain shared scheduling options (Sole root-level public header)
│   ├── input/                       # Physical Augmentation · Input
│   ├── output/                      # Physical Augmentation · Output
│   ├── sensor/                      # Physical Augmentation · Sensor
│   ├── actuator/                    # Physical Augmentation · Actuator (Single device, no cross-device loops)
│   ├── display/                     # Physical Augmentation · Display
│   ├── comm/                        # Physical Augmentation · Communication / Telemetry
│   ├── math/                        # Pure Algorithms (Hardware-agnostic, scheduling-agnostic)
│   └── control/                     # Domain Control (Cross-device closed loops / orchestration)
└── src/                             # Must mirror include subdirectories
    ├── input|output|sensor|actuator|display|comm|math|control/
    └── …
```

### 2.1 Domain Responsibilities & Dependencies

| Domain | Responsibility | Permitted Dependencies | Forbidden |
|---|---|---|---|
| **Physical Augmentation** | Augments behavior of a **single** primary DAL instance (periodic polling, sweeping, event dispatch) | That primary DAL, runtime, `wink_bal_opts` | Forming cross-device closed loops; exposing system-level goals like chassis velocity as primary APIs |
| **math** | Pure computational / stateful algorithms (PID, filters, kinematics) | `wink_status.h`, Standard C | Any `dal_*`, `runtime`, `pal_*`, `wink_bal_opts` |
| **control** | Control targets + feedback loops or multi-actuator orchestration | `math`, multiple DALs, runtime, `wink_bal_opts` | Public headers including `pal_*` (except `pal_log.h`) |

### 2.2 `actuator/` vs `control/` (Hard Rules)

| Criterion | Directory |
|---|---|
| Augments only **one** actuator DAL; no sensor feedback loop | `actuator/` |
| Actuator + sensor feedback (or orchestrating ≥2 actuators) targeting physical objectives (speed, pose, v/ω) | `control/` |

Rule of thumb: **Single-device open-loop / convenience augmentation → actuator; tracking targets, feedback loops, cross-device → control.**

### 2.3 New Component Decision Tree

```text
Q1: Does it depend on any DAL or Runtime?
    ├─ NO  → math/
    └─ YES → Q2: Does it span ≥2 DALs, or form an "actuator+sensor" closed loop, or orchestrate multiple sessions?
              ├─ YES → control/
              └─ NO  → Physical Augmentation: choose input|output|sensor|actuator|display|comm by primary DAL class
```

---

## 3. File & Symbol Naming Conventions

### 3.1 Filename Rules

| Domain / Role | Filename Pattern | Example |
|---|---|---|
| math | `wink_<algo>.h` | `wink_pid.h` |
| control | `wink_<capability>.h` | `wink_closed_loop_dc_motor.h`, `wink_chassis.h` |
| Physical · Class A Activity | `wink_<device>_<activity>.h` | `wink_led_blink.h`, `wink_rc_servo_sweep.h`, `wink_ultrasonic_poll.h` |
| Physical · Class B Event | `wink_<device>_<noun>_events.h` | `wink_button_events.h`, `wink_ultrasonic_distance_events.h` |
| Physical · Named Service | `wink_<service>_<qualifier>.h` | `wink_telemetry_default.h` |

**Forbidden** (Within public tree):
- Filename suffixes `_helper`, `_controller`
- `sonar` in public symbols or paths (Standardized to `ultrasonic`)
- Filename stem diverging from public API prefix

**Mandatory**: `wink_chassis.h` → `wink_chassis_start`; symbols must be greppable directly from paths.

### 3.2 API Verbs (ADR-0032)

| Class | Verbs | Typical Usage |
|---|---|---|
| A Activity | `start` / `stop` (+ `_ex`) | Periodic sessions, closed-loop sessions |
| B Capability | `enable_*` / `disable_*` | Event queue dispatching |
| C Action | `set` / `get` / `request` / ... | Setpoints, one-off read/write |
| Predicates | `is_` / `has_` / `can_` | Read-only queries |

Control components default to **A + C**.

### 3.3 Lexicon Standards

- Device names align with DAL: `button`, `led`, `ultrasonic`, `rc_servo`, `dc_motor`, `encoder` (Generic `motor` is banned as a control/DAL prefix; see ADR-0048 / ADR-0049 / ADR-0050).
- Capacity Macros: `WINK_<CAPABILITY>_MAX` (Derived from `WINK_APP_MAX_<DAL>_INSTANCES`).
- Shared Options: `wink_bal_opts_t` (Header `wink_bal_opts.h`).

### 3.4 Include Syntax

```c
#include "math/wink_pid.h"
#include "control/wink_chassis.h"
#include "sensor/wink_ultrasonic_poll.h"
#include "wink_bal_opts.h"
```

CMake: `wink_bal` PUBLIC includes add **only** `bal/include`, never subdirectories directly.

---

## 4. Implementation Patterns (Mandatory)

### 4.1 Class A / Periodic Session Components

1. **Opaque Handles**: Public headers must not expose `wink_periodic_handle_t` or internal slot fields.
2. **Static Slot Pools**: Fixed `static` arrays inside `.c` files; keyed by primary DAL pointer (Chassis: Left motor).
3. **Dual API Tracks**: `_start` / `_start_ex(const wink_bal_opts_t *opts)`.
4. **Config / State Separation**: External `*_config_t` (can be `static const`); runtime state hidden in slots.
5. **Unit Contracts**: Doxygen headers **must freeze units** (e.g., Closed-loop motor `counts/s`; chassis `m/s` + `rad/s`).
6. **Stop Equals Safe**: `_stop` must leave actuators in safe states (zero duty / safe-off).

### 4.2 Control Specific Mandates (Inherited from ADR-0037)

1. **Fail-Safe**: Feedback loss → braking + `wink_trace_fault` (`WINK_FAULT_MOTOR_FEEDBACK_LOSS`).
2. **Measured dt**: Uses measured high-precision timestamp deltas rather than assuming idealized tick intervals.
3. **Setpoint Concurrency**: App writes / Periodic reads must be protected by critical sections (preventing float tearing).
4. **Compositional Rollback**: Multi-actuator startup failures must roll back already-started sessions cleanly.

### 4.3 Math Specific Mandates

1. Host unit testable without hardware mocks.
2. Invalid inputs return `wink_status_t` (Recommended), never conflating errors with zero outputs undocumented.
3. PID: Derivative-on-measurement + explicit anti-windup clamping (see ADR-0037).

### 4.4 DAL Pruning & Control Components

Control components depending on specific DALs (`wink_closed_loop_dc_motor`, `wink_chassis`) must compile as stubs (`MAX==0` path, returning `WINK_ERR_UNSUPPORTED`) when corresponding `WINK_USE_DC_MOTOR` / `WINK_USE_ENCODER` flags are disabled.

> **DC Safe-Off (ADR-0048)**: `dal_dc_motor_safe_off` binds to **brake**. When single-direction pins (`dir_pin_b < 0`) prevent active braking, DAL returns `WINK_ERR_UNSUPPORTED`, causing closed-loop shutdown paths to fall back to `coast`.

### 4.5 Minimum Host Unit Testing Scenario Matrix for Control

| Scenario | Minimum Requirement |
|---|---|
| Invalid parameters / Lifecycle | Validated |
| Feedback loss fail-safe | Virtual-time driven timeout with safe-off braking / coast fallback |
| Injectable feedback tracking | Encoder count injection + `dal_encoder_get_count` read path with output convergence |
| Saturation / Anti-windup | Asserts integrator halts climbing during output clamping |
| Encoder count overflow / wrap-around | Reads via `dal_encoder_get_count` without int32 velocity spikes |
| Time injection | Uses virtual monotonic time advances (`sim_set_mono_time_us` / `sim_advance_mono_time_us`) |

Users only need to declare devices in `wink-app.json`; no manual `-DWINK_USE_*` definitions required ([ADR-0039](../../decisions/core/0039-dal-dual-mode-auto-pruning.md)):

| Condition | Behavior |
|---|---|
| **With** `wink-app.json` | Only declared DAL drivers set `WINK_USE_*=ON`, others OFF; codegen `app_options.cmake` writes all 9 macros |
| **Without** JSON (e.g. Arduino) | All 9 drivers ON; configure emits WARNING. Production firmware should provide JSON to reduce binary bloat |

Shared logic resides in `wink-micro-os/cmake/wink_dal_drivers.cmake`; ESP32 / Host / Binary SDK / wasm single App consume it identically. Target-specific hardcoding of driver baselines is forbidden.

---

### 4.6 Fast-Loop & Control Function Constraints Invokable from ISRs (ADR-0047)

> **Scope**: Native SimpleFOC fast loops only. ISR host lives in **DAL/target trampolines**, and ISR registration / `pal_hwtimer` symbols are **strictly forbidden** from BAL public headers.

ADR-0047 **permits** periodic control ISRs to invoke BAL pure fast-loop functions (Clarke/Park/SVPWM/current loop), provided:

| # | Constraint | Description |
|---|---|---|
| 1 | **No Blocking** | Prohibits `pal_delay_*`, blocking mutex locks, busy-waits, synchronous I/O |
| 2 | **No `pal_log`** | Prohibits logging macros in fast-loop execution paths |
| 3 | **Bounded Stack** | Prohibits large local buffers or recursion; stack budget annotated in trampolines |
| 4 | **Explicit Shared State Only** | Exchanges with slow loop (~50Hz) occur exclusively via documented shared buffers or atomic variables |
| 5 | **Locked Numerical Types** | Fast loop prefers **fixed-point (Q15/Q31)**; float requires explicit Xtensa FPU context handling |
| 6 | **No `pal_*` in Public Headers** | Fast-loop public APIs must not expose hardware PAL dependencies |

---

## 5. Target State Inventory (Post-Hard Cutover)

| Path | Primary API |
|---|---|
| `wink_bal_opts.h` | `wink_bal_opts_t` |
| `output/wink_led_blink.h` | `wink_led_blink_start` / `_stop` |
| `input/wink_button_events.h` | `wink_button_enable_events` / `_disable_events` |
| `sensor/wink_ultrasonic_poll.h` | `wink_ultrasonic_poll_start` / `_stop` |
| `sensor/wink_ultrasonic_distance_events.h` | `wink_ultrasonic_enable_distance_events` |
| `actuator/wink_rc_servo_sweep.h` | `wink_rc_servo_sweep_start` / `_stop`; `wink_rc_servo_set_angle` |
| `comm/wink_telemetry_default.h` | `wink_telemetry_default_start` / `_stop` |
| `math/wink_pid.h` | `wink_pid_init` / `_update` / `_reset` |
| `math/wink_diff_drive_kinematics.h` | `wink_diff_drive_to_*` |
| `control/wink_closed_loop_dc_motor.h` | `wink_closed_loop_dc_motor_start` / `_set_speed` (DAL DC Motor only; ADR-0049) |
| `control/wink_chassis.h` | `wink_chassis_start` / `_set_velocity` (DC + Encoder backend) |

---

## 6. CI Quality Gates

Text-scanning rules are validated by **`wink lint --pack layering`** ([ADR-0043](../../decisions/tools/0043-yaml-driven-layer-lint.md)); linkage and layout rules remain in `bal/CMakeLists.txt`.

| ID | Rule | Enforced By |
|---|---|---|
| BAL-INC-1 / BAL-HDR-NO-PAL | `bal/include/**/*.h` must not `#include` `pal_*.h` (`pal_log.h` exempt) | `wink lint` |
| BAL-MATH-1 | `bal/include/math/**/*.h` must not contain `dal_`, `wink_runtime`, `wink_periodic`, `pal_` | `wink lint` |
| BAL-NAME-1 | `bal/include/**` must not contain `*_helper.h`, `*_controller.h` | `wink lint` |
| BAL-NAME-2 | `bal/include/**/*.h` must not contain `sonar` identifier | `wink lint` |
| BAL-INC-2 | `wink_bal` PUBLIC include directories contain only `.../bal/include` | CMake |
| BAL-SRC-1 | Every `include/<domain>/*.h` has its implementation in `src/<domain>/` (Mirrored) | CMake |

---

## 7. Review Checklist for New / Modified BAL Components

- [ ] Domain verified via §2.3 Decision Tree; `actuator` vs `control` placed correctly
- [ ] Filename conforms to §3.1; API prefix matches file stem
- [ ] A/B/C verb class verified (§3.2 / coding-conventions §3)
- [ ] Includes use domain prefixes; zero PAL leaks in public headers
- [ ] Class A: Opaque slot + `_start_ex` + documented units + safe stop
- [ ] Control: Fail-safe / measured dt / setpoint critical sections
- [ ] Math: Zero DAL dependencies; unit testable via pure math
- [ ] Tests and CMake targets connected; zero legacy naming
- [ ] ISR fast loop: Satisfies §4.6 checklist entirely (ADR-0047)

---

## 8. Cross-Document References

| Document | Relationship |
|---|---|
| ADR-0023 | Layering, slots, dual-track, forbidden PAL headers |
| ADR-0032 | A/B/C verb conventions |
| ADR-0037 | 3 Domains and closed-loop safety |
| ADR-0049 | Closed-loop capability naming `wink_closed_loop_dc_motor` |
| ADR-0047 | FOC foreground/background partitioning; ISR fast-loop constraints (§4.6) |
| `03-directory-architecture.md` | Kernel skeleton; BAL rules governed by this document |
| `coding-conventions.md` | §3 Verbs; §5 Points to this document |
