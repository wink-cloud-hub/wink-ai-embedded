# 14. AI DSL, State Machine AST & App Safe Codegen Pipeline Specification

<!-- i18n-meta
source: docs/zh/design/03-app-codegen/03-ai-dsl-and-codegen-pipeline.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

This document defines the secure generation pipeline from AI / low-code visual models to App C code in the Wink-AI platform. The platform does not allow AI to directly generate unconstrained C code; instead, AI generates a constrained Domain-Specific Language (DSL) or state machine AST, which is then deterministically compiled into App C, enhancing safety, explainability, replayability, and visual editability.

> **DAL Driver / Role Description SSOT (ADR-0046 Mechanics + ADR-0051 Paths)**:  
> - **Target State**: Scannable extension roots (default `wink-micro-os/codegen/drivers/*.yaml` + `roles/*.yaml`; optional App / CMake cache `WINK_CODEGEN_PATHS`) serve as the machine-readable SSOT; the `wink-tools` engine executes read-only scanning, validation, sandboxed rendering, and `list_drivers` CMake emission. Evaluation order: **Builtin → OS → env → App**.  
> - **Retained Mechanics**: `list_drivers.py` generates data-driven CMake; `app_codegen` dynamically enumerates `WINK_USE_*`; manual edits across multiple driver tables are strictly forbidden.  
> - **Migration Window**: Reading legacy `tools/codegen/drivers/*.py` remains supported until tech-design exit criteria are satisfied.  
> Decisions: [ADR-0051](../../decisions/tools/0051-scannable-codegen-extension-roots.md); Technical Design: [scannable-codegen-extension-roots-design](../../tech-designs/tools/2026-07-28-scannable-codegen-extension-roots-design.md).

---

## 1. Core Principles

1. **AI Generates Intent, Not Direct C Code Control**.
2. **DSL / AST is the Authoritative Business Logic Input**; App C is an auditable, exportable, and regenerable derivative artifact.
3. **All Hardware Access Must Route via Device Model Declared DAL APIs**.
4. **All Calls Returning `wink_status_t` Must Be Checked Explicitly**.
5. **Generation Outputs Must Map Bidirectionally to Canvas Nodes and Prompts**.

---

## 2. Overall Pipeline

```text
Natural Language / Blockly / State Machine UI
        │
        ▼
Intent Parser / AI Tool
        │
        ▼
Embedded App DSL / State Machine AST
        │
        ▼
Schema Validation
        │
        ▼
Device Model Constraint Check
        │
        ▼
Safety Rule Check
        │
        ▼
Deterministic App C Codegen
        │
        ▼
Static C Check
        │
        ▼
Wasm Simulation
        │
        ▼
Build / Flash Gate
```

---

## 3. DSL Top-Level Schema

```json
{
  "dslVersion": 1,
  "kind": "wink-app-state-machine",
  "name": "DistanceAlarmLogic",
  "devices": {
    "front_radar": {
      "modelId": "hc-sr04",
      "dalType": "dal_ultrasonic_t"
    },
    "status_led": {
      "modelId": "led",
      "dalType": "dal_led_t"
    }
  },
  "constants": [],
  "stateMachine": {},
  "faultPolicy": {},
  "loop": {}
}
```

---

## 4. Constants Definition

```json
{
  "constants": [
    {
      "name": "OBSTACLE_THRESHOLD_CM",
      "type": "float",
      "value": 20.0,
      "min": 2.0,
      "max": 400.0,
      "description": "Distance threshold for obstacle detection"
    },
    {
      "name": "MAX_SENSOR_ERROR_COUNT",
      "type": "int",
      "value": 3,
      "min": 1,
      "max": 10
    }
  ]
}
```

---

## 5. State Machine Definition

```json
{
  "stateMachine": {
    "name": "SystemState",
    "initialState": "INIT",
    "states": [
      {
        "id": "INIT",
        "onEnter": [
          { "action": "setLed", "device": "status_led", "value": "off" },
          { "action": "transition", "to": "RUNNING" }
        ]
      },
      {
        "id": "RUNNING",
        "transitions": [
          {
            "when": {
              "op": "lt",
              "left": { "var": "front_distance_cm" },
              "right": { "const": "OBSTACLE_THRESHOLD_CM" }
            },
            "to": "ALARM",
            "actions": [
              { "action": "setLed", "device": "status_led", "value": "on" }
            ]
          }
        ]
      },
      {
        "id": "ALARM",
        "transitions": [
          {
            "when": {
              "op": "gte",
              "left": { "var": "front_distance_cm" },
              "right": { "const": "OBSTACLE_THRESHOLD_CM" }
            },
            "to": "RUNNING",
            "actions": [
              { "action": "setLed", "device": "status_led", "value": "off" }
            ]
          }
        ]
      }
    ]
  }
}
```

Constraints:
1. Recommended state count ≤ 16 for MVP.
2. Deterministic transition conditions.
3. Recursive state actions are strictly forbidden.
4. Codegen compiles state machines into flat `state_var + switch-case` loops; direct function recursion (`state_A() { ...; state_B(); }`) is forbidden (Stack safety).
5. Maximum action chain depth ≤ 4.
6. Maximum expression AST nesting depth ≤ 8.

---

## 6. Loop Definition

```json
{
  "loop": {
    "periodMs": 50,
    "steps": [
      {
        "id": "read_front_distance",
        "action": "dalRead",
        "device": "front_radar",
        "api": "dal_ultrasonic_read",
        "outputs": {
          "distance_cm": "front_distance_cm"
        },
        "onError": {
          "increment": "front_radar_error_count",
          "ifGte": {
            "left": "front_radar_error_count",
            "right": "MAX_SENSOR_ERROR_COUNT",
            "then": [
              {
                "action": "fault",
                "code": "FAULT_FRONT_RADAR_UNAVAILABLE"
              }
            ]
          },
          "returnLoop": true
        }
      },
      {
        "id": "run_state_machine",
        "action": "evaluateStateMachine"
      },
      {
        "id": "delay_tick",
        "action": "delay",
        "ms": { "const": "APP_TICK_RATE_MS" }
      }
    ]
  }
}
```

---

## 7. Fault Policy Definition

```json
{
  "faultPolicy": {
    "faultCodes": [
      {
        "name": "FAULT_FRONT_RADAR_UNAVAILABLE",
        "value": 1001,
        "severity": "error"
      }
    ],
    "onFault": [
      {
        "action": "setLed",
        "device": "status_led",
        "value": "flash"
      },
      {
        "action": "traceFault",
        "code": { "arg": "fault_code" }
      }
    ]
  }
}
```

---

## 8. Action Whitelist

| Action | Description | Codegen Target |
|---|---|---|
| `dalRead` | Calls sensor DAL API | `wink_status_t status = dal_xxx_read(...)` |
| `dalWrite` | Calls actuator DAL API | `wink_status_t status = dal_xxx_set(...)` |
| `setLed` | LED semantic action | `dal_led_set_state(...)` |
| `setServoAngle` | Servo semantic action | `dal_rc_servo_set_angle(...)` |
| `displayText` | OLED text rendering | `dal_oled_draw_text(...)` |
| `transition` | State machine transition | `current_state = ...` |
| `fault` | Triggers application fault | `app_on_fault(...)` |
| `traceFault` | Emits fault trace | `wink_trace_fault(...)` |
| `evaluateStateMachine` | Executes state machine | Switch dispatch |
| `delay` | Periodic tick delay | `wink_app_delay_ms(...)` or scheduler |

---

## 9. Expression Subset

```json
{
  "op": "and",
  "args": [
    {
      "op": "gt",
      "left": { "var": "front_distance_cm" },
      "right": { "literal": 0.0 }
    },
    {
      "op": "lt",
      "left": { "var": "front_distance_cm" },
      "right": { "const": "OBSTACLE_THRESHOLD_CM" }
    }
  ]
}
```

Allowed Operators:
```text
eq, neq, lt, lte, gt, gte, and, or, not, add, sub, mul, div, clamp
```

---

## 10. Codegen Output Specification

Generated files:

```text
src/
├── app_config.h
├── app_main.c
└── app_generated_meta.json
```

`app_generated_meta.json`:

```json
{
  "dslHash": "sha256:...",
  "codegenVersion": "0.1.0",
  "generatedFiles": [
    {
      "path": "src/app_main.c",
      "hash": "sha256:..."
    }
  ],
  "sourceMap": [
    {
      "dslNodeId": "read_front_distance",
      "file": "src/app_main.c",
      "startLine": 42,
      "endLine": 58
    }
  ]
}
```

---

## 11. Generated C Code Sample

```c
#include "device_tree.h"
#include "app_config.h"
#include "wink_app.h"
#include "wink_trace.h"

static system_state_t current_state = SYSTEM_STATE_INIT;
static uint8_t front_radar_error_count = 0;

void app_init(void) {
    wink_status_t led_status = dal_led_set_state(&status_led, LED_STATE_OFF);
    if (led_status != WINK_OK) {
        app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
        return;
    }
    current_state = SYSTEM_STATE_RUNNING;
}

void app_loop(void) {
    float front_distance_cm = 0.0f;
    wink_status_t radar_status = dal_ultrasonic_read(&front_radar, &front_distance_cm);
    if (radar_status != WINK_OK) {
        front_radar_error_count++;
        if (front_radar_error_count >= MAX_SENSOR_ERROR_COUNT) {
            app_on_fault(FAULT_FRONT_RADAR_UNAVAILABLE);
        }
        wink_app_delay_ms(APP_TICK_RATE_MS);
        return;
    }

    front_radar_error_count = 0;

    switch (current_state) {
        case SYSTEM_STATE_RUNNING:
            if (front_distance_cm > 0.0f && front_distance_cm < OBSTACLE_THRESHOLD_CM) {
                current_state = SYSTEM_STATE_ALARM;
                wink_trace_state_change(SYSTEM_STATE_RUNNING, SYSTEM_STATE_ALARM);
                wink_status_t led_status = dal_led_set_state(&status_led, LED_STATE_ON);
                if (led_status != WINK_OK) {
                    app_on_fault(FAULT_STATUS_LED_UNAVAILABLE);
                    return;
                }
            }
            break;
        default:
            break;
    }

    wink_app_delay_ms(APP_TICK_RATE_MS);
}
```

---

## 12. Static Checking Rules

**DSL Layer Checks**:
1. Schema validation.
2. Device references verified.
3. DAL APIs and signatures matched.
4. State machine reachability verified.
5. Fault paths present.
6. Monitored sampling rates meet Device Model minimums.

**C Layer Checks**:
1. Forbidden PAL inclusion.
2. Forbidden `malloc` / `free`.
3. Forbidden recursion (`.clang-tidy` `misc-no-recursion`).
4. Forbidden user `while(1)`.
5. `wink_status_t` return codes checked.
6. Bounded stack: `-Wstack-usage=1536 -Werror=stack-usage`.
7. Forbidden `alloca` and Variable Length Arrays (VLAs) (`clang-analyzer-security.insecureAPI.alloca`).

---

## 13. AI Automated Remediation Workflow

```text
Diagnostic Error
  ↓
Map to Source DSL Node
  ↓
AI Proposes DSL JSON Patch
  ↓
Schema Validate Patch
  ↓
Constraint Check
  ↓
Regenerate C
  ↓
Static Check
  ↓
Simulation Replay
```

AI Remediation output format:

```json
{
  "patchType": "dsl-json-patch",
  "reason": "front_radar timeout was not handled",
  "patch": [
    {
      "op": "add",
      "path": "/loop/steps/0/onError",
      "value": {
        "action": "fault",
        "code": "FAULT_FRONT_RADAR_UNAVAILABLE"
      }
    }
  ]
}
```

---

## 14. Visual Editing & Blockly Integration

```text
Blockly Block / State Node
        ⇄
App DSL Node
        ⇄
Generated C SourceMap
```

---

## 15. MVP Implementation Scope

- **MVP-0**: `dalRead`, `setLed`, `transition`, `fault`, 2–3 states, Button LED / Distance Alarm samples, DSL $\rightarrow$ C codegen, sourceMap generation, and `wink_status_t` validation.
- **MVP-1**: Servo, OLED, complex expressions, and AI auto-remediation.
