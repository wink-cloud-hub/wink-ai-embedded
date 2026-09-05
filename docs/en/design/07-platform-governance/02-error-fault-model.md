# 07. Error Model, Fault Injection & Safe Degradation Specification

<!-- i18n-meta
source: docs/zh/design/07-platform-governance/02-error-fault-model.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

Professionalism in embedded systems is proven not just when the happy path succeeds, but when anomalous conditions are identifiable, recoverable, testable, and traceable. The Wink-AI platform establishes the error model as a unified contract across App, BAL, DAL, PAL, simulation, and real hardware, eliminating implicit return values, magic numbers, or uncontrolled crashes.

---

## 1. Design Goals

1. **Unified Error Semantics**: All DAL/PAL APIs return standard status codes.
2. **Injectable Faults**: Simulation environments actively inject disconnects, timeouts, noise, and out-of-range conditions.
3. **Safety-First**: System transitions to safe actuator postures upon hazard detection rather than running untrusted logic.
4. **Constrained AI Generation**: AI-generated App code must explicitly check status codes.
5. **Virtual-Physical Parity**: Simulation faults, hardware errors, and Golden Traces share identical status fields.

---

## 2. Unified Status Codes

```c
#ifndef WINK_STATUS_H
#define WINK_STATUS_H

#include <stdint.h>

/* Blocking API Hard Isolation Attribute (ADR-0017) */
#define WINK_BLOCKING \
    __attribute__((deprecated("Blocking API forbidden in cooperative runtime; use non-blocking variant")))

typedef enum {
    WINK_OK = 0,

    /* Universal Recoverable Errors (Negative, POSIX/Linux Convention) */
    WINK_ERR_INVALID_ARG        = -1,
    WINK_ERR_TIMEOUT            = -2,
    WINK_ERR_DISCONNECTED       = -3,
    WINK_ERR_OUT_OF_RANGE       = -4,
    WINK_ERR_IO                 = -5,
    WINK_ERR_BUSY               = -6,
    WINK_ERR_UNSUPPORTED        = -7,
    WINK_ERR_CHECKSUM           = -8,
    WINK_ERR_PERMISSION         = -9,
    WINK_ERR_RESOURCE_EXHAUSTED = -10,
    WINK_ERR_NOT_INITIALIZED    = -11,
    WINK_ERR_HARDWARE           = -12,   /* Hardware / Driver failure (ESP-IDF esp_err_t != ESP_OK) */
    WINK_ERR_NO_MEM             = -13,   /* Allocation failure / Out of memory */
    WINK_ERR_EMPTY              = -14,   /* Container / Queue empty */
    WINK_ERR_FULL               = -15,   /* Container / Queue full */
    WINK_ERR_INVALID_STATE      = -16,   /* Invalid state machine transition */
    WINK_ERR_LOCKED             = -17,   /* Resource locked (Boot safe-lock / Flash lock) */
    WINK_ERR_NOT_FOUND          = -18,   /* Target not found in registry */
    WINK_ERR_CANCELED           = -19,   /* Concurrent benign cancellation */

    /* Functional Safety (Recoverable vs Fatal) */
    WINK_ERR_OVERCURRENT        = -20,   /* Overcurrent (Recoverable: Current limit & retry) */
    WINK_ERR_OVERTEMPERATURE    = -21,   /* Overtemperature (Recoverable: Throttle) */
    WINK_ERR_ALREADY_INITIALIZED = -22,  /* Duplicate init (Call sequence bug, fail-fast) */
    WINK_ERR_WATCHDOG           = -30,   /* Watchdog timeout (Fatal: Reset) */
    WINK_ERR_OVERFLOW           = -40,   /* Numerical overflow / UB (Fatal) */

    /* Recoverable Degradation (ADR-0005) */
    WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50,   /* Corrupt NVS/Config → Continue with safe defaults */
    WINK_ERR_FAILED_INIT             = -51,   /* Device init failure → Isolate device, continue system */
    WINK_ERR_PANIC              = -99,   /* Unrecoverable panic, halt required */
} wink_status_t;

#endif
```

> ✅ **Accepted — [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md)**  
> Error code convention enforces: `0 = Success`, `Negative = Error`.
>
> 🔁 **Extended — [ADR-0005](../../decisions/core/0005-degraded-status-segment.md)**: Adds `-50s` recoverable degradation segment (`WINK_ERR_CONFIG_CORRUPT_DEGRADED = -50`, `WINK_ERR_FAILED_INIT = -51`).

Status Code Rules:
1. `WINK_OK` must equal 0.
2. Fallible DAL/PAL APIs avoid implicit returns (`-1.0f`, `NULL`, `false`).
3. Sensor readings return via output pointer parameters.
4. Actuator controls return status codes indicating write success.
5. `WINK_ERR_PANIC` is reserved strictly for unrecoverable halts.

---

## 3. DAL API Error Return Convention

Recommended:
```c
wink_status_t dal_ultrasonic_read(dal_ultrasonic_t *dev, float *distance_cm);
wink_status_t dal_rc_servo_set_angle(dal_rc_servo_t *dev, float angle_deg);
wink_status_t dal_oled_draw_buffer(dal_oled_t *dev, const uint8_t *buffer, uint32_t len);
```

Non-Recommended Anti-Patterns:
```c
float dal_ultrasonic_get_distance(dal_ultrasonic_t *dev);
bool dal_rc_servo_set_angle(dal_rc_servo_t *dev, float angle_deg);
```

Rationale:

| Unrecommended Pattern | Underlying Issue |
|---|---|
| `float -1.0f` | Cannot distinguish disconnection, timeout, out-of-range, or uninitialized states |
| `bool false` | Lacks root cause diagnostic information |
| `NULL` | Only suitable for pointers, not numeric telemetry |

---

## 4. Standard Fault Classification

| Fault | Status Code | Simulation Source | Hardware Source | Default Policy |
|---|---|---|---|---|
| Invalid Arg | `WINK_ERR_INVALID_ARG` | AI Generation Error | Caller Parameter Error | Halt execution |
| Timeout | `WINK_ERR_TIMEOUT` | Injected Latency | Bus / Sensor Unresponsive | Use last valid / Enter safe state |
| Disconnected | `WINK_ERR_DISCONNECTED` | Broken Net / Pin Float | GPIO / I2C NACK | Enter safe state |
| Out of Range | `WINK_ERR_OUT_OF_RANGE` | Slider Overflow | Sensor Out of Bounds | Clamp or alarm |
| Busy | `WINK_ERR_BUSY` | Bus Contention | I2C / SPI Unreleased | Yield / Retry |
| Unsupported | `WINK_ERR_UNSUPPORTED` | Missing Target Feature | PAL Target Stub | Block before build |
| Checksum | `WINK_ERR_CHECKSUM` | Virtual Corruption | Bus CRC Error | Retry / Alarm |
| Resource Exhausted | `WINK_ERR_RESOURCE_EXHAUSTED` | Exceeded PWM Channels | Heap / Handle Exhaustion | Block deployment |

---

## 5. BAL Error Handling Constraints

1. Return values of DAL APIs returning `wink_status_t` must be checked.
2. Failed sensor reads must not use uninitialized outputs.
3. Actuator failures must transition to observable error paths.
4. Consecutive $N$ failures must trigger `app_on_fault()`.
5. `app_on_fault()` must perform safe shutdowns and trace logging only.

```c
void app_loop(void) {
    float distance_cm = 0.0f;
    wink_status_t status = dal_ultrasonic_read(&front_radar, &distance_cm);

    if (status != WINK_OK) {
        sensor_error_count++;
        if (sensor_error_count >= MAX_SENSOR_ERROR_COUNT) {
            app_on_fault(FAULT_FRONT_RADAR_UNAVAILABLE);
        }
        wink_app_delay_ms(APP_TICK_RATE_MS);
        return;
    }

    sensor_error_count = 0;

    if (distance_cm > 0.0f && distance_cm < OBSTACLE_THRESHOLD_CM) {
        wink_status_t servo_status = dal_rc_servo_set_angle(&neck_servo, 180.0f);
        if (servo_status != WINK_OK) {
            app_on_fault(FAULT_SERVO_CONTROL_FAILED);
            return;
        }
    }

    wink_app_delay_ms(APP_TICK_RATE_MS);
}
```

---

## 6. Fault Injection Model

```json
{
  "componentId": "front_radar",
  "faults": [
    {
      "type": "timeout",
      "enabled": true,
      "startAtMs": 5000,
      "durationMs": 3000
    },
    {
      "type": "noise",
      "enabled": true,
      "stddev": 1.2
    },
    {
      "type": "disconnect",
      "enabled": false
    }
  ]
}
```

Fault injection entrypoints:

| Entrypoint | Use Case |
|---|---|
| Property Inspector | Manual exception testing by users |
| Automated Test Suites | CI verification of App safety paths |
| AI Test Generation | Dynamic scenario generation |
| Golden Trace | Deterministic replay sequences |

---

## 7. Safe Degradation Strategy

| Code Segment | Category | Degradation Principle |
|---|---|---|
| `WINK_OK(0)` | Normal | Continue normal execution |
| `-1..-11` Universal | Recoverable | Retry / Use last valid / Clamp; log event, do not halt |
| `-50s` (Degraded / Failed Init) | Recoverable Degradation | Isolate device or fallback to safe defaults; system continues running |
| `-20s` (Overcurrent / Temp) | Safety Recoverable | Throttle power; escalate to fatal if sustained |
| `-30s` (Watchdog) | Fatal | Trigger system reboot |
| `-40s` (Overflow / UB) | Fatal | Halt untrusted compute |
| `-99` (Panic) | Unrecoverable | Immediate halt and trace, await hardware reset |

Actuator Fail-Safe States:

| Actuator | Safe Posture |
|---|---|
| LED | Red warning or flashing alert |
| Servo | Return to center (90°) or hold position |
| Motor | Cease PWM output / Short brake |
| Relay | Disconnect output |
| Heater | Power off heating elements |
| Pump | Halt pumping |

```c
void app_on_fault(uint32_t fault_code) {
    dal_motor_stop(&left_motor);
    dal_motor_stop(&right_motor);
    dal_rc_servo_set_angle(&neck_servo, 90.0f);
    dal_led_set_state(&status_led, LED_STATE_FLASHING);
    wink_trace_fault(fault_code);
}
```

---

## 8. Error Observability

```c
typedef struct {
    uint64_t timestamp_ms;
    uint32_t component_id;
    uint16_t api_id;
    wink_status_t status;
    uint32_t fault_code;
} wink_error_event_t;
```

Event output channels:

| Environment | Output Channel |
|---|---|
| Web Simulation | Worker postMessage to UI console |
| Hardware Debugging | UART trace stream |
| CI Testing | Golden Trace file output |
| Cloud Analytics | Structured cloud error telemetry |

---

## 9. Compile-Time Static Checking

Rejects bare unchecked calls:
```c
dal_ultrasonic_read(&front_radar, &distance_cm);
```

Mandates:
```c
wink_status_t status = dal_ultrasonic_read(&front_radar, &distance_cm);
if (status != WINK_OK) {
    app_on_fault(FAULT_FRONT_RADAR_UNAVAILABLE);
    return;
}
```

---

## 10. Product Experience Recommendations

The simulation workbench provides an **Anomaly Testing** panel:
1. Disconnect pins.
2. Inject sensor timeouts.
3. Add ADC Gaussian noise.
4. Simulate I2C bus collisions.
5. Simulate servo stalls.
6. Verify App fail-safe transitions.

---

## 11. AI Codegen Error Code Semantics SSOT

### 11.1 Universal Recoverable Segment (`-1..-17`)

| Value | Name | Semantics | Typical Trigger | Recommended Recovery | Valid `WINK_PT_EXIT` Condition |
|---|---|---|---|---|---|
| 0 | `WINK_OK` | Success | Normal path | Continue | ❌ No |
| -1 | `WINK_ERR_INVALID_ARG` | Invalid argument | NULL pointer, bad enum | **Caller bug**: fix caller | ❌ No |
| -2 | `WINK_ERR_TIMEOUT` | Timeout | Missing I2C ACK, pulse timeout | Retry $\le N$ times; escalate to `app_on_fault` | ❌ No |
| -3 | `WINK_ERR_DISCONNECTED` | Disconnected | Missing device, float pin | Enter fail-safe immediately | ✅ Yes |
| -4 | `WINK_ERR_OUT_OF_RANGE` | Out of range | ADC / geometric bounds | Clamp to boundary | ❌ No |
| -5 | `WINK_ERR_IO` | Generic I/O | Bus failure | Retry $\le 2$ times | ❌ No |
| -6 | `WINK_ERR_BUSY` | Resource busy | Bus arbitration, `WAIT_UNTIL` | **Yield**: `WINK_PT_YIELD` to next tick | ❌ No |
| -7 | `WINK_ERR_UNSUPPORTED` | Unsupported | Missing platform capability | **Build-time bug**: Log + fault | ✅ Yes |
| -8 | `WINK_ERR_CHECKSUM` | Checksum error | CRC mismatch | Retry $\le 2$ times | ❌ No |
| -9 | `WINK_ERR_PERMISSION` | Permission denied | Sandbox violation | Stop operation; `app_on_fault` | ✅ Yes |
| -10 | `WINK_ERR_RESOURCE_EXHAUSTED` | Out of resources | Full handle table | Capacity error; `app_on_fault` | ✅ Yes |
| -11 | `WINK_ERR_NOT_INITIALIZED` | Not initialized | Uninitialized call | Call order bug: fix caller | ✅ Yes |
| -12 | `WINK_ERR_HARDWARE` | Driver error | `esp_err_t != ESP_OK` | Retry $\le 2$ times; `app_on_fault` | ❌ No |
| -13 | `WINK_ERR_NO_MEM` | Out of memory | Allocator exhaustion | Forbidden in runtime; `app_on_fault` | ✅ Yes |
| -14 | `WINK_ERR_EMPTY` | Queue empty | No event pending | Yield to next tick | ❌ No |
| -15 | `WINK_ERR_FULL` | Queue full | Trace buffer full | Expand or drop oldest/newest | ❌ No |
| -16 | `WINK_ERR_INVALID_STATE` | Invalid state | Illegal transition | Sequence bug: fix caller | ✅ Yes |
| -17 | `WINK_ERR_LOCKED` | Resource locked | Safe-lock active (ADR-0010) | Follow safe-lock recovery | ✅ Yes |

### 11.2 Safety Recoverable Segment (`-20..-22`)

| Value | Name | Semantics | Typical Trigger | Recommended Recovery | Valid `WINK_PT_EXIT` Condition |
|---|---|---|---|---|---|
| -20 | `WINK_ERR_OVERCURRENT` | Overcurrent | Motor stall, short circuit | Throttle current or shut off | ❌ No |
| -21 | `WINK_ERR_OVERTEMPERATURE` | Overtemperature | Thermal threshold | Reduce duty cycle / throttle | ❌ No |
| -22 | `WINK_ERR_ALREADY_INITIALIZED` | Duplicate init | Double init on DAL | Sequence bug: fix caller | ✅ Yes |

### 11.3 Recoverable Degradation Segment (`-50..-51`, ADR-0005)

| Value | Name | Semantics | Typical Trigger | Recommended Recovery | Valid `WINK_PT_EXIT` Condition |
|---|---|---|---|---|---|
| -50 | `WINK_ERR_CONFIG_CORRUPT_DEGRADED` | Config corrupt | NVS CRC error | Load safe defaults; continue | ❌ No |
| -51 | `WINK_ERR_FAILED_INIT` | Single init failed | One device failed | Isolate failed device; continue | ✅ Yes (If PT depends on it) |

### 11.4 Fatal Segment (`-30..-40`, `-99`)

| Value | Name | Semantics | Typical Trigger | Recommended Recovery | Valid `WINK_PT_EXIT` Condition |
|---|---|---|---|---|---|
| -30 | `WINK_ERR_WATCHDOG` | Watchdog timeout | WDT triggered | Reboot & safe-lock | ❌ No |
| -40 | `WINK_ERR_OVERFLOW` | Numerical overflow | Integer overflow / UB | Halt untrusted calculation | ❌ No |
| -99 | `WINK_ERR_PANIC` | Unrecoverable panic | Invariant assert | `wink_trace_fault` + halt | ❌ No |

### 11.5 Generator Constraints

1. Check every fallible API call immediately.
2. Branch according to error code segments (Recoverable vs Degraded vs Fatal).
3. Use `WINK_PT_EXIT` only when marked ✅.
4. `WINK_ERR_BUSY` represents coroutine yield, not a failure.
