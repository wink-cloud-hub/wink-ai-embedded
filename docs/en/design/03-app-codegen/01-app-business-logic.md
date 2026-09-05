# 02. Universal Low-Code AI Embedded Development Platform: Application Layer (App) Runtime Specification

<!-- i18n-meta
source: docs/zh/design/03-app-codegen/01-app-business-logic.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| **Associated ADRs** | ADR-0004 (Static Dispatch), ADR-0011 (State Struct), ADR-0013/0014 (Cooperative Scheduler), ADR-0018 (IRQ 3-Tier Narrowing), ADR-0048 (Actuator Semantics) |
| **Associated Technical Designs** | [app-layer-lowcode-unification-design.md](../../tech-designs/tools/app-layer-lowcode-unification-design.md); [user-surface-insulation-design.md](../../tech-designs/tools/2026-07-28-user-surface-insulation-design.md) (User Stable Surface SSOT) |
| **Associated Implementation Plans** | [2026-07-28-user-surface-phase1-plan.md](../../implementation-plans/frontend/2026-07-28-user-surface-phase1-plan.md) |
| **Associated Reviews** | [dal-control-semantic-completeness-review §10](../../reviews/core/2026-07-28-dal-control-semantic-completeness-review.md); [user-surface-phase1-plan-review.md](../../reviews/frontend/2026-07-28-user-surface-phase1-plan-review.md) |
| **Codegen Implementation** | `wink gen app` / `wink gen device-tree` (`wink-tools/tools/codegen/`, [ADR-0051](../../decisions/tools/0051-scannable-codegen-extension-roots.md) / [ADR-0059](../../decisions/tools/0059-wink-tools-cli-hybrid-verb-first-architecture.md)) |
| **Primary Reference Samples** | L1: `oled_dashboard` (Button events), `avoidance_car` (Distance events ADR-0033); QA: `devkitc_smoke` |

In low-code and AI-assisted embedded development scenarios, the primary design objective is to **shield tedious technical complexities, focusing developer attention exclusively on business workflows and control algorithms**.

This document defines the **Application Layer (App)** runtime specifications, C code automatic generation contracts, and its complete decoupling from low-level hardware logic.

> **Terminology Clarification**:
> - ✅ **App Layer**: User-authored or AI-generated one-off business logic, containing `app_init` / `app_loop` / `app_on_fault`.
> - ✅ **BAL Layer**: Business Abstraction Layer ([ADR-0023](../../decisions/core/0023-bal-business-abstraction-layer.md) / [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md)), reusable services (`wink_led_blink`, `wink_button_events`, `wink_telemetry_default`, `wink_pid`, etc.) shipped with WinkMicroOS.

---

## 1. App Layer Core Responsibilities & Design Principles

### 1.1 Core Responsibilities
* **Business Workflow Scheduling**: Defines device workflows, invokes BAL algorithm libraries, responds to event triggers, and manages inter-device collaboration (e.g., "when ultrasonic distance < 15cm, turn servo and light red LED").
* **State Machine Lifecycle Management**: Controls system initialization, the steady-state Main Loop, error states, and fail-safe recovery transitions.
* **Human/Cloud Interaction**: Processes input parameters from Web control panels or packages telemetry payloads for upstream transmission.

### 1.2 Architectural Constraints
To ensure 100% platform portability, App code must strictly follow these rules:
1. **Forbidden Hardware Headers**: App code **must never** `#include "pal_hal.h"` or invoke low-level bus APIs like `pal_gpio_write` or `pal_i2c_transfer`.
2. **Forbidden Hardcoded Pins/Channels**: All pin assignments (e.g., Pin 2, PWM Channel 0) must be handled by the static device tree. App code references only named device instance handles (`&front_radar`, `&neck_servo`).
3. **Interaction via Semantic Interfaces**: All hardware interaction occurs exclusively through read/write semantic APIs provided by DAL (Device Layer) or BAL (Algorithm Layer).
4. **Single-Source Dual-Target Compilation**: Identical App business code compiles unmodified for WebWorker (Wasm) simulation sandboxes and ESP32/STM32 physical firmware.

---

## 2. Low-Code / AI-Generated C Code Architecture

In visual composition editors or AI engines, block diagrams, state machines, or natural language prompts are translated into standardized C source trees:

```text
generated_app/
├── app_config.h              # Business parameters and macro definitions
├── device_tree.h             # Generated logical device instance declarations
├── device_tree.c             # Generated static device parameter allocations
└── app_main.c                # Core state machine & main loop implementation
```

### 2.1 Lifecycle Contract
Generated business code provides 3 standard lifecycle callbacks invoked by the runtime:

```c
/**
 * @brief System initialization entry point (Called once by OS after boot)
 */
void app_init(void);

/**
 * @brief Periodic main loop function (Called in while(1) loop within OS task)
 */
void app_loop(void);

/**
 * @brief Safety protection callback invoked upon critical exceptions
 */
void app_on_fault(uint32_t fault_code);
```

### 2.2 Codegen Input: `wink-app.json` (Static Physical Declaration)

Device instances and physical pin maps are driven by `wink-app.json`, parsed during builds to produce `device_tree.c/h` and `app_options.cmake`.

🚨 **Layering Contract (ADR-0023): JSON describes static physical topology only, containing zero business state or startup behavior.**  
All automated service startups (`services`), state variables (`state_variables`), and callback registrations (`callbacks`) have been removed from the schema. Services must be explicitly launched inside user C code.

Sample JSON Format (L1, Default Path):

```json
{
  "app_name": "devkitc_smoke",
  "board": "esp32_devkitc",
  "devices": {
    "board_led":   { "type": "led",      "pin": 2,  "active_high": true },
    "boot_button": { "type": "button",   "pin": 0,  "active_low": true,
                     "long_press_ms": 3000, "isr_counter": true,
                     "auto_poll_ms": 10 },
    "smoke_ultrasonic": { "type": "ultrasonic", "trig_pin": 18, "echo_pin": 19,
                     "use_rmt": true }
  }
}
```

| Field | Type | Description |
|---|---|---|
| `app_name` | string | Target build name, mapped to CMake target |
| `board` | string | Board preset (References `boards/<board>.json`) |
| `devices` | map<name, DeviceSpec> | Static device definitions specifying driver types and pin properties |

#### 2.2.1 Progressive Disclosure: `advanced` (L2, ADR-0034)

L1 contains semantic fields only. Expert optional fields reside **strictly** within the `advanced` object:

```json
"boot_button": {
  "type": "button",
  "pin": 0,
  "active_low": true,
  "auto_poll_ms": 10,
  "advanced": { "pull": "none" }
},
"neck_servo": {
  "type": "rc_servo",
  "pwm_channel": 0,
  "min_pulse_ms": 0.5,
  "max_pulse_ms": 2.5,
  "advanced": {
    "resolution_bits": 10,
    "clock_requirement": "stable_required"
  }
}
```

| `advanced` Key | Applicable Type | Valid Values | Default |
|---|---|---|---|
| `pull` | button | `auto` \| `up` \| `down` \| `none` | Omitted → C `AUTO` (Matches standard pull behavior) |
| `resolution_bits` | servo | Positive integer | Omitted → 0 → Platform default 13-bit |
| `clock_requirement` | servo | `auto` \| `stable_required` | Omitted → AUTO |

---

### 2.3 Device Tree Generation Contract (`device_tree.h/c` & Exported Macros)

#### 1. Capacity Limits Macros
Codegen counts device instances from `wink-app.json`, exporting `WINK_APP_MAX_<DEV>_INSTANCES` in `device_tree.h`:
```c
#define WINK_APP_MAX_LED_INSTANCES          1u
#define WINK_APP_MAX_BUTTON_INSTANCES       1u
#define WINK_APP_MAX_ULTRASONIC_INSTANCES   1u
#define WINK_APP_MAX_SERVO_INSTANCES        0u
```
When instances equal `0u`, corresponding BAL services compile as empty stubs with `WINK_UNAVAILABLE_MSG`, **achieving zero RAM waste**.

#### 2. Config Constants Macros
Exported parameter macros avoid magic numbers in C code:
```c
#define BOOT_BUTTON_AUTO_POLL_MS   10u
#define BOOT_BUTTON_LONG_PRESS_MS  3000u
```

#### 3. Automatic WINK_MAX_PERIODIC Calculation
Calculates periodic task slot capacity:
$$\text{WINK\_MAX\_PERIODIC} = \sum \text{instances} + 4$$

#### 4. Bus-Owner Static Initialization Sequencing
For shared I2C/SPI buses:
- `wink_device_tree_init()` calls `pal_i2c_bus_init()` first, then initializes devices via `dal_xxx_init()` in topological order.
- `wink_device_tree_deinit()` calls `dal_xxx_deinit()` in reverse order before destroying the bus.

---

### 2.4 Removal of `app_support.c`

To preserve the principle that "JSON describes static physics; all behavior lives in C code", the legacy generated `app_support.c` has been **completely removed**. Applications explicitly invoke services (e.g., `wink_button_enable_events()`) inside `app_init_status()`.

---

### 2.5 `wink_button_events` Context Constraints (Important ⚠️)

`wink_button_enable_events(&btn, &cfg)` executes in Runtime tick sync callbacks, imposing **ISR-like constraints**:
- ⛔ **Forbidden**: Calling yielding/blocking APIs (`pal_os_delay_ms`, `pal_os_mutex_lock`, heavy `printf`).
- ⛔ **Forbidden**: Computations exceeding ~100µs.
- ✅ **Permitted**: Setting volatile flags, posting to lock-free ring buffers, calling non-blocking DAL reads.

---

### 2.6 BAL API Error Handling Tiers

| Tier | Marker | Target APIs | Caller Responsibility |
|---|---|---|---|
| **Fatal** | `WINK_WARN_UNUSED_RESULT` | `_init`, `wink_runtime_spawn_*`, `wink_actuator_register`, `wink_button_enable_events` | Mandatory `WINK_CHECK` or explicit handling |
| **Normal** | `WINK_WARN_UNUSED_RESULT` | `_request_measurement`, `_start`, `wink_telemetry_default_start`, `dal_button_on_event` | Must receive return value; explicit ignore permitted |
| **Fire-and-forget** | None | `wink_led_blink_start/stop`, `wink_trace_*` | Direct invocation; failures logged via `LOG_D` |

---

### 2.7 Role-Interface-Based Codegen

To eliminate raw C pointer syntax (`&`) and decouple App logic from specific driver chipsets (e.g., `ssd1306`), WinkMicroOS introduces **Compile-Time Abstract Role Interfaces**:

#### 1. Concept & Mechanics
Default driver roles in `wink-app.json`:
- `led` $\rightarrow$ `binary_indicator`
- `button` $\rightarrow$ `binary_sensor`
- `ultrasonic` $\rightarrow$ `distance_sensor`
- `ssd1306` $\rightarrow$ `text_display`
- `rc_servo` $\rightarrow$ `angular_actuator`
- `dc_motor` $\rightarrow$ `open_loop_actuator`
- `encoder` $\rightarrow$ `pulse_counter`

#### 2. Standard Roles & Capability Verbs

| Role (`role`) | Verb (`verb`) | Error Tier | Generated C Signature | Description |
|---|---|---|---|---|
| **`binary_indicator`** | `activate` | Fire-and-forget | `void {name}_activate(void)` | Turn indicator on |
| | `deactivate` | Fire-and-forget | `void {name}_deactivate(void)` | Turn indicator off |
| | `toggle` | Fire-and-forget | `void {name}_toggle(void)` | Toggle indicator state |
| **`binary_sensor`** | `is_active` | Convenience (bool) | `bool {name}_is_active(void)` | Returns boolean status |
| | `is_active_status` | Normal | `wink_status_t {name}_is_active_status(bool*)` | Contract-honest status API |
| | `was_active` | Convenience (bool) | `bool {name}_was_active(void)` | Rising edge detection |
| | `was_active_status` | Normal | `wink_status_t {name}_was_active_status(bool*)` | Rising edge status API |
| | `start_auto_poll` | Fatal | `wink_status_t {name}_start_auto_poll(uint32_t)`| Starts soft timer polling task |
| | `stop_auto_poll` | Fire-and-forget | `void {name}_stop_auto_poll(void)` | Stops soft timer polling |
| **`distance_sensor`** | `request_measurement` | Normal | `wink_status_t {name}_request_measurement(void)` | Non-blocking distance request |
| | `read_distance` | Convenience (float) | `float {name}_read_distance(void)` | Returns distance cm (or `-1.0f` on error) |
| | `read_distance_status` | Normal | `wink_status_t {name}_read_distance_status(float*)` | Returns measurement status code |
| **`text_display`** | `clear` | Fire-and-forget | `void {name}_clear(void)` | Clears display framebuffer |
| | `draw_text` | Fire-and-forget | `void {name}_draw_text(uint16_t, uint8_t, const char*)` | Draws text to framebuffer |
| | `flush` | Fire-and-forget | `void {name}_flush(void)` | Flushes framebuffer to physical display |
| **`angular_actuator`** | `set_angle` | Fire-and-forget | `void {name}_set_angle(float)` | Sets servo angle in degrees |
| **`open_loop_actuator`** | `set_speed` | Normal | `wink_status_t {name}_set_speed(float)` | Sets duty cycle / open-loop speed |
| | `coast` | Normal | `wink_status_t {name}_coast(void)` | Coasts motor (all low) |
| | `brake` | Normal | `wink_status_t {name}_brake(void)` | Short-circuit active braking |
| | `safe_off` | Normal | `wink_status_t {name}_safe_off(void)` | Safe shutdown |
| **`pulse_counter`** | `get_count` | Normal | `wink_status_t {name}_get_count(int32_t*)` | Raw pulse count (no CPR) |
| | `reset` | Normal | `wink_status_t {name}_reset(void)` | Resets counter to zero |

#### 3. Development Guidelines & Escape Hatch
- ✅ **Recommended**: Use generated `{instance_name}_{verb}` interfaces for chip-agnostic, warning-free application logic.
- ✅ **Escape Hatch**: `device_tree.h` exports device handles (`extern dal_led_t board_led;`). Direct `dal_*` calls trigger a lint warning unless allowed via allowlists.

#### 4. User Stable Surface vs Driver Surface (Phase 1 Contract)

| Surface | Contents | Managed By | Insulation Goal |
|---|---|---|---|
| **User Stable Surface** | App C $\rightarrow$ `{name}_{verb}`; JSON `role` & stable knobs | User / Low-Code | Driver signature changes only require re-codegen |
| **Driver Surface** | JSON `type`, pins/buses, `drive_mode`, `decode_mode` | Per App | Normal pin changes are expected |
| **Escape Hatch** | `&instance` + `dal_*` / `#include "dal_*.h"` | Experts | lint warn + allowlist |
| **BAL-Backed** | Event verbs calling BAL components internally | App uses Role verbs | Changes documented via BAL changelogs |

**Phase 1 Semantic Contracts:**

| `type` / Role | Semantic Guarantee |
|---|---|
| `dc_motor` / `open_loop_actuator` | Defaults `drive_mode = in_in` (PWM + IN_A/IN_B); `safe_off` per [ADR-0048](../../decisions/core/0048-actuator-control-semantic-naming.md) |
| `encoder` / `pulse_counter` | Defaults `decode_mode = x1_rising`; `invert=true` swaps A/B polarity; no CPR conversion |
| `rc_servo` / `angular_actuator` | `pulse_ms = min_pulse + (angle / effective_max_angle) * (max_pulse - min_pulse)` |
| `ssd1306` / `text_display` | Preserves chip type `ssd1306` |

##### IN/IN Truth Table

`dir_pin_a` = A, `dir_pin_b` = B:

```text
dir_a  dir_b | state
  0      0   | coast
  1      0   | forward
  0      1   | reverse
  1      1   | brake (short)
```

---

## 3. Representative App Layer Business Logic Sample

### 3.1 Core Logic: `app_main.c` (State Struct Pattern)

> **⚠️ State Struct Pattern (ADR-0011):**  
> All persistent state variables must use `WINK_PT_STATE_*` macros; local `static` variables are forbidden.

```c
#include "device_tree.h"
#include "app_config.h"
#include "wink_app.h"

// System operational states
typedef enum {
    SYSTEM_STATE_INIT,      // Initializing
    SYSTEM_STATE_RUNNING,   // Cruising forward
    SYSTEM_STATE_AVOIDING,  // Obstacle avoidance maneuver
    SYSTEM_STATE_EMERGENCY  // Emergency braking state
} system_state_t;

WINK_PT_STATE_BEGIN(app_main)
    system_state_t task_001_current_state;
    uint32_t       task_001_last_scan_tick;
    float          task_001_last_distance;
WINK_PT_STATE_END()

static struct app_main_state g_app_state;
static wink_pt_t g_app_pt;

void app_init(void) {
    WINK_PT_INIT(&g_app_pt);
    memset(&g_app_state, 0, sizeof(g_app_state));
    g_app_state._magic = 0x50545354UL;

    if (dal_led_set_state(&status_led, LED_STATE_OFF) != WINK_OK) {
        app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
        return;
    }
    
    if (dal_rc_servo_set_angle(&neck_servo, 90.0f) != WINK_OK) {
        app_on_fault(FAULT_SERVO_CONTROL_FAILED);
        return;
    }
    
    g_app_state.task_001_current_state = SYSTEM_STATE_RUNNING;
}

void app_loop(void) {
    WINK_PT_STATE_USE(app_main);
    WINK_PT_BEGIN(&g_app_pt);

    while (1) {
        float distance = 0.0f;
        wink_status_t distance_status = dal_ultrasonic_read(&front_radar, &distance);
        if (distance_status != WINK_OK) {
            app_on_fault(FAULT_FRONT_RADAR_UNAVAILABLE);
            WINK_PT_DELAY_MS(&g_app_pt, APP_TICK_RATE_MS);
            continue;
        }

        switch (state->task_001_current_state) {
            case SYSTEM_STATE_RUNNING:
                if (distance > 0.0f && distance < OBSTACLE_THRESHOLD_CM) {
                    state->task_001_current_state = SYSTEM_STATE_AVOIDING;
                    if (dal_led_set_state(&status_led, LED_STATE_ON) != WINK_OK) {
                        app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
                        break;
                    }
                    if (dal_rc_servo_set_angle(&neck_servo, 180.0f) != WINK_OK) {
                        app_on_fault(FAULT_SERVO_CONTROL_FAILED);
                        break;
                    }
                }
                break;
                
            case SYSTEM_STATE_AVOIDING:
                if (distance >= OBSTACLE_THRESHOLD_CM) {
                    state->task_001_current_state = SYSTEM_STATE_RUNNING;
                    if (dal_led_set_state(&status_led, LED_STATE_OFF) != WINK_OK) {
                        app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
                        break;
                    }
                    if (dal_rc_servo_set_angle(&neck_servo, 90.0f) != WINK_OK) {
                        app_on_fault(FAULT_SERVO_CONTROL_FAILED);
                        break;
                    }
                } else if (distance > 0.0f && distance < EMERGENCY_THRESHOLD_CM) {
                    state->task_001_current_state = SYSTEM_STATE_EMERGENCY;
                    app_on_fault(FAULT_OBSTACLE_COLLISION);
                }
                break;
                
            case SYSTEM_STATE_EMERGENCY:
                if (dal_led_set_state(&status_led, LED_STATE_FLASHING) != WINK_OK) {
                    app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
                    break;
                }
                if (distance > OBSTACLE_THRESHOLD_CM) {
                    state->task_001_current_state = SYSTEM_STATE_RUNNING;
                    if (dal_led_set_state(&status_led, LED_STATE_OFF) != WINK_OK) {
                        app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
                        break;
                    }
                    if (dal_rc_servo_set_angle(&neck_servo, 90.0f) != WINK_OK) {
                        app_on_fault(FAULT_SERVO_CONTROL_FAILED);
                        break;
                    }
                }
                break;
                
            default:
                break;
        }

        WINK_PT_DELAY_MS(&g_app_pt, APP_TICK_RATE_MS);
    }

    WINK_PT_END(&g_app_pt);
}

void app_on_fault(uint32_t fault_code) {
    (void)dal_rc_servo_set_angle(&neck_servo, 90.0f);
    (void)dal_led_set_state(&status_led, LED_STATE_ON);
    wink_trace_fault(fault_code);
}
```

---

## 3.2 Architectural Benefits of State Struct Pattern

| Feature | Static Local Variables | `WINK_PT_STATE` Struct |
|---|---|---|
| **Cross-Yield Safety** | ✅ Safe (Static storage) | ✅ Safe |
| **Multi-Instance Re-entrancy** | ❌ Shared across instances | ✅ Independent per instance |
| **Web Simulation Debugging** | ❌ Unenumerable | ✅ Inspects full state tree |
| **Sleep / Wakeup Persistence** | ❌ Unserializable | ✅ Direct dump/restore |
| **Fault Diagnostics** | ❌ Opaque in crashes | ✅ Snapshot for post-mortem analysis |
| **Unit Testing** | ❌ Test state pollution | ✅ Clean isolated test state |

---

## 4. Low-Code to App C Codegen Mapping (Blockly / DSL)

```text
[ Read "front_radar" distance into "dist" ]
                    │
                    ▼ (Codegen)
float dist = 0.0f;
wink_status_t status = dal_ultrasonic_read(&front_radar, &dist);
```

```text
[ Persistent variable "current_state" ]
                    │
                    ▼ (Codegen)
WINK_PT_STATE_BEGIN(app_main)
    system_state_t task_001_current_state;
WINK_PT_STATE_END()

state->task_001_current_state = SYSTEM_STATE_RUNNING;
```

```text
[ Set "neck_servo" angle to 120° ]
                    │
                    ▼ (Codegen)
dal_rc_servo_set_angle(&neck_servo, 120.0f);
```

```text
[ State Transition: Cruise -> Avoid (Condition: Distance < 20.0) ]
                    │
                    ▼ (Codegen)
if (distance < 20.0f) { state->task_001_current_state = SYSTEM_STATE_AVOIDING; }
```

### 4.1 Codegen Namespace Rules

| Element | Naming Format | Example |
|---|---|---|
| State Struct Name | `{task_name}_state` | `app_main_state` |
| Member Variable | `task_{NNN}_{var_name}` | `task_001_current_state` |
| Coroutine Control Block | `g_{task_name}_pt` | `g_app_main_pt` |

---

### 4.2 Architectural Summary

1. **Absolute Syntax Safety**: High-risk register and bus logic are sealed inside DAL/PAL. AI generators assemble clean application C code that cannot corrupt low-level firmware.
2. **Mocking & Testability**: Test suites mock `device_tree.h` interfaces on the host PC to execute 100% automated CI unit testing.
3. **Native Multi-Instance Support**: State pointers allow identical coroutines to run across multiple robot instances simultaneously.
4. **Diagnostic Introspection**: Simulation runtimes inspect state structs directly in the browser to visualize live state machines.
