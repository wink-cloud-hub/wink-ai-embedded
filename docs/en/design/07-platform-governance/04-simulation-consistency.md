# 09. Virtual-Physical Consistency Verification, Golden Trace & Reproducible Experimentation Specification

<!-- i18n-meta
source: docs/zh/design/07-platform-governance/04-simulation-consistency.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

The core promise of the Wink-AI platform is "single-source business logic verified in browser simulation first, then deployed to real hardware." To make this promise credible, the platform establishes a quantitative, replayable, and comparable virtual-physical consistency verification system rather than relying on visual inspection alone.

---

## 1. Consistency Goals

1. **Input Consistency**: Simulation and hardware receive identical logical input event sequences.
2. **State Consistency**: App state machines execute identical transitions at key decision points.
3. **Output Consistency**: DAL actuator commands match within bounded timing tolerance windows.
4. **Error Consistency**: Error codes for faults, timeouts, and disconnects carry identical semantics.
5. **Reproducibility**: Any simulation or hardware run can export a trace for deterministic replay.

---

## 2. Golden Trace Concept

Golden Trace captures structured operational runtime events. Instead of recording low-level electrical waveforms, it logs semantic events relevant to business logic.

```text
Golden Trace
├── Input Events       User inputs, sensor physical inputs, fault injections
├── App Events         app_init, app_loop, state transitions, fault triggers
├── DAL Read Events    Sensor readings and status codes
├── DAL Write Events   Actuator commands and status codes
├── PAL Events         Key bus transaction summaries
└── Runtime Events     Watchdog, memory, worker lifecycle
```

---

## 3. Trace Event Structure

```json
{
  "traceVersion": 1,
  "runId": "run_20260622_000001",
  "environment": "simulation",
  "target": "wasm32-emscripten",
  "projectHash": "sha256:...",
  "deviceTreeHash": "sha256:...",
  "startedAt": "2026-06-22T00:00:00Z",
  "events": [
    {
      "seq": 1,
      "timeMs": 0,
      "type": "runtime.start",
      "payload": { "runtime": "wink-micro-os", "version": "0.1.0" }
    },
    {
      "seq": 2,
      "timeMs": 10,
      "type": "dal.read",
      "componentId": "front_radar",
      "api": "dal_ultrasonic_read",
      "status": "WINK_OK",
      "payload": { "distanceCm": 35.2 }
    },
    {
      "seq": 3,
      "timeMs": 12,
      "type": "bal.state_change",
      "payload": { "from": "RUNNING", "to": "AVOIDING" }
    },
    {
      "seq": 4,
      "timeMs": 13,
      "type": "dal.write",
      "componentId": "neck_servo",
      "api": "dal_rc_servo_set_angle",
      "status": "WINK_OK",
      "payload": { "angleDeg": 180 }
    }
  ]
}
```

---

## 4. Event Type Specification

| Type | Description | Mandatory Fields |
|---|---|---|
| `runtime.start` | Execution started | `runtime`, `version` |
| `runtime.stop` | Execution stopped | `reason` |
| `runtime.watchdog` | Watchdog event | `loopTimeMs`, `action` |
| `input.sensor` | Sensor physical input | `componentId`, `value` |
| `input.user` | User interaction | `componentId`, `action` |
| `fault.inject` | Fault injection | `componentId`, `faultType` |
| `bal.init` | `app_init` invocation | `status` |
| `bal.loop` | `app_loop` cycle | `durationMs` |
| `bal.state_change` | State machine transition | `from`, `to` |
| `bal.fault` | `app_on_fault` invocation | `faultCode` |
| `dal.read` | DAL read operation | `componentId`, `api`, `status`, `value` |
| `dal.write` | DAL write operation | `componentId`, `api`, `status`, `command` |
| `pal.transfer` | Protocol transaction | `bus`, `port`, `size`, `status` |

---

## 5. Simulation Trace Collection

Wasm simulation captures trace events at:
1. `app_init()` entry and exit.
2. `app_loop()` cycle boundaries.
3. DAL API entries and exits.
4. PAL bus transaction summaries.
5. JS-side sensor inputs and UI events.
6. Fault injection triggers.
7. Worker watchdog events.

C-Side Interfaces:
```c
void wink_trace_dal_read(uint32_t component_id, uint16_t api_id, wink_status_t status, float value);
void wink_trace_dal_write(uint32_t component_id, uint16_t api_id, wink_status_t status, float command);
void wink_trace_fault(uint32_t fault_code);
void wink_trace_state_change(uint16_t from_state, uint16_t to_state);
```

---

## 6. Real Hardware Trace Collection

Hardware implementations avoid excessive overhead by:
1. Storing key events in ring buffers.
2. Emitting low-frequency batches over UART in JSON Lines.
3. Enabling trace via compile-time options.
4. Logging semantic events rather than raw GPIO waveforms.

Sample output format (`jsonl`):
```jsonl
{"seq":1,"timeMs":0,"type":"runtime.start","target":"esp32"}
{"seq":2,"timeMs":10,"type":"dal.read","componentId":"front_radar","status":"WINK_OK","distanceCm":35.1}
{"seq":3,"timeMs":13,"type":"dal.write","componentId":"neck_servo","status":"WINK_OK","angleDeg":180}
```

---

## 7. Trace Comparison Rules

| Comparison Item | Rule |
|---|---|
| Event Ordering | Causal order must match identically |
| Timestamps | Allowed target-specific jitter window (e.g., $\pm 50\text{ms}$) |
| Floating-Point Values | Bounded absolute/relative delta (e.g., $\pm 1\text{cm}$) |
| State Transitions | Must match identically |
| Error Codes | Must match identically |
| Actuator Commands | Target final setpoints must match |

Comparison output:
```json
{
  "matched": false,
  "summary": {
    "totalEvents": 120,
    "matchedEvents": 116,
    "mismatches": 4
  },
  "mismatches": [
    {
      "type": "value_delta_exceeded",
      "simulationEventSeq": 42,
      "hardwareEventSeq": 41,
      "field": "distanceCm",
      "simulationValue": 18.2,
      "hardwareValue": 23.8,
      "tolerance": 1.0
    }
  ]
}
```

---

## 8. Input Replay Mechanism

```json
{
  "replayVersion": 1,
  "events": [
    {
      "timeMs": 1000,
      "type": "input.sensor",
      "componentId": "front_radar",
      "payload": { "distanceCm": 30 }
    },
    {
      "timeMs": 2000,
      "type": "input.sensor",
      "componentId": "front_radar",
      "payload": { "distanceCm": 12 }
    },
    {
      "timeMs": 3000,
      "type": "fault.inject",
      "componentId": "front_radar",
      "payload": { "faultType": "timeout", "durationMs": 1000 }
    }
  ]
}
```

---

## 9. Consistency Levels

| Level | Meaning | Requirement |
|---|---|---|
| C0 | Unverified | Code executed without verification |
| C1 | Simulation Verified | Wasm trace error-free |
| C2 | Simulation Fault-Tested | Injected faults trigger safe states |
| C3 | Hardware Trace Matched | Real hardware trace matches simulation |
| C4 | Regression Stable | Repeated runs produce identical traces within tolerance |

Displayed badges:
```text
Consistency: C2 Simulation Fault-Tested
Consistency: C3 Hardware Trace Matched
```

---

## 10. CI/CD Integration

Official examples provide:
```text
examples/obstacle-car/
├── project.json
├── app_main.c
├── traces/
│   ├── normal-run.golden.json
│   ├── obstacle-detected.golden.json
│   └── sensor-timeout.golden.json
└── tests/
    └── trace-compare.test.ts
```

---

## 11. MVP Scope

Initial implementation records:
1. `runtime.start/stop`
2. `bal.state_change`
3. `bal.fault`
4. `dal.read`
5. `dal.write`
6. `fault.inject`
