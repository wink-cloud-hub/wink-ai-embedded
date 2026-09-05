# 13. Embedded Project Manifest & Registry Lock Specification

<!-- i18n-meta
source: docs/zh/design/03-app-codegen/02-project-manifest-schema.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

This document defines the unified embedded project file format for Wink-AI. The Project Manifest is the shared contract between frontend canvas, AI generation, Wasm simulation, device tree generation, cloud compilation, flashing, and Golden Trace, establishing clean project boundaries for master monorepo integration.

---

## 1. Design Goals

1. **Single Project Entry Point**: One Manifest describes target boards, devices, wiring, business logic, simulation configs, build profiles, and verification states.
2. **Reproducibility**: Locks model versions and sha256 hashes via Registry Lock, ensuring historical projects can be re-simulated, re-compiled, and trace-compared identically.
3. **Migratability**: Manages schema evolution via `schemaVersion`, `projectVersion`, and migration records.
4. **Auditability**: All AI generation, codegen outputs, build artifacts, and safety gates have explicit provenance and hashes.
5. **Main Repo Friendly**: The host application stores only the Manifest and summary metadata without needing to understand low-level embedded schemas.

---

## 2. Directory Layout

A standard embedded project directory comprises:

```text
embedded-project/
├── wink-project.json              # Project Manifest
├── device-registry.lock.json      # Device Model / Board Model Lockfile
├── src/
│   ├── app.dsl.json               # Recommended: AI/Low-code generated DSL
│   ├── app_main.c                 # Generated App C code
│   ├── app_config.h
│   ├── device_tree.c
│   └── device_tree.h
├── traces/
│   ├── normal.golden.json
│   └── fault-timeout.golden.json
├── builds/
│   └── build-manifest.json
└── assets/
    └── thumbnail.png
```

`src/app_main.c`, `device_tree.c`, and `device_tree.h` are derived artifacts. Authoritative inputs are `wink-project.json`, `device-registry.lock.json`, and `src/app.dsl.json`.

---

## 3. Project Manifest Top-Level Schema

```json
{
  "schemaVersion": 1,
  "projectVersion": "0.1.0",
  "projectType": "wink-embedded",
  "id": "proj_distance_alarm",
  "name": "Distance Alarm",
  "description": "ESP32 ultrasonic distance alarm demo",
  "createdAt": "2026-06-22T00:00:00Z",
  "updatedAt": "2026-06-22T00:00:00Z",
  "target": {
    "boardId": "esp32-devkit-v1",
    "boardVersion": "1.0.0",
    "palTarget": "targets/esp32",
    "toolchain": "esp-idf"
  },
  "devices": [],
  "connections": [],
  "logic": {},
  "simulation": {},
  "safety": {},
  "build": {},
  "trace": {},
  "metadata": {}
}
```

---

## 4. Target Definition

```json
{
  "target": {
    "boardId": "esp32-devkit-v1",
    "boardVersion": "1.0.0",
    "boardModelHash": "sha256:...",
    "palTarget": "targets/esp32",
    "runtimeVersion": "0.1.0",
    "toolchain": "esp-idf",
    "flashProtocol": "webserial-esptool"
  }
}
```

Constraints:
1. `boardModelHash` must match `device-registry.lock.json`.
2. `palTarget` is declared strictly by the Board Model.
3. `flashProtocol` configures flashing wizard capabilities.

---

## 5. Devices Definition

```json
{
  "devices": [
    {
      "componentId": "front_radar",
      "modelId": "hc-sr04",
      "modelVersion": "1.0.0",
      "modelHash": "sha256:...",
      "displayName": "Front Radar",
      "category": "sensor",
      "position": { "x": 320, "y": 180 },
      "rotation": 0,
      "properties": {
        "maxDistanceCm": 400,
        "sampleIntervalMs": 60
      },
      "codegen": {
        "instanceName": "front_radar",
        "dalType": "dal_ultrasonic_t",
        "role": "distance_sensor"
      }
    }
  ]
}
```

---

## 6. Connections Definition

```json
{
  "connections": [
    {
      "id": "conn_001",
      "from": {
        "componentId": "front_radar",
        "pin": "TRIG"
      },
      "to": {
        "componentId": "esp32-devkit-v1",
        "pin": "GPIO4"
      },
      "netId": "net_trig",
      "validation": {
        "status": "passed",
        "warnings": []
      }
    }
  ]
}
```

---

## 7. Logic Definition

```json
{
  "logic": {
    "sourceType": "dsl",
    "dslPath": "src/app.dsl.json",
    "generatedCPath": "src/app_main.c",
    "codegenVersion": "0.1.0",
    "sourceHash": "sha256:...",
    "generatedHash": "sha256:...",
    "entrypoints": {
      "init": "app_init",
      "loop": "app_loop",
      "fault": "app_on_fault"
    },
    "generator": {
      "type": "ai",
      "model": "configured-by-host",
      "promptHash": "sha256:..."
    }
  }
}
```

Manual C Mode:

```json
{
  "logic": {
    "sourceType": "manual-c",
    "sourcePath": "src/app_main.c",
    "sourceHash": "sha256:..."
  }
}
```

---

## 8. Simulation Definition

```json
{
  "simulation": {
    "preferredRouting": "dal-value-bypass",
    "wasmTarget": "wasm32-emscripten",
    "asyncify": true,
    "workerLimits": {
      "initialMemoryMb": 16,
      "maxMemoryMb": 64,
      "heartbeatMs": 100,
      "loopTimeSliceMs": 20,
      "maxTraceEvents": 10000
    },
    "faultScenarios": [
      {
        "id": "sensor_timeout",
        "name": "Sensor Timeout",
        "events": [
          {
            "timeMs": 3000,
            "componentId": "front_radar",
            "type": "timeout",
            "durationMs": 1000
          }
        ]
      }
    ]
  }
}
```

---

## 9. Safety Definition

```json
{
  "safety": {
    "level": "S2",
    "staticCheck": {
      "status": "passed",
      "checkedAt": "2026-06-22T00:00:00Z",
      "diagnostics": [],
      "rulesetVersion": "0.1.0"
    },
    "simulationGate": {
      "normalRunPassed": true,
      "faultTestsPassed": true,
      "lastRunId": "run_001"
    },
    "flashAllowed": false
  }
}
```

---

## 10. Build Definition

```json
{
  "build": {
    "lastBuildId": "build_001",
    "status": "success",
    "buildProfile": "debug-trace",
    "artifact": {
      "name": "firmware.bin",
      "size": 1048576,
      "sha256": "sha256:..."
    },
    "manifestPath": "builds/build-manifest.json"
  }
}
```

---

## 11. Trace Definition

```json
{
  "trace": {
    "consistencyLevel": "C2",
    "lastSimulationRunId": "run_sim_001",
    "lastHardwareRunId": null,
    "goldenTraces": [
      {
        "id": "normal",
        "path": "traces/normal.golden.json",
        "hash": "sha256:..."
      }
    ]
  }
}
```

---

## 12. Registry Lock

`device-registry.lock.json` locks board, peripheral, bus, DAL API, codegen template, and simulation model dependencies:

```json
{
  "lockVersion": 1,
  "registryVersion": "0.1.0",
  "registryHash": "sha256:...",
  "generatedAt": "2026-06-22T00:00:00Z",
  "models": [
    {
      "type": "board",
      "id": "esp32-devkit-v1",
      "version": "1.0.0",
      "hash": "sha256:..."
    },
    {
      "type": "peripheral",
      "id": "hc-sr04",
      "version": "1.0.0",
      "hash": "sha256:..."
    }
  ],
  "templates": [
    {
      "id": "device-tree/hc-sr04.c.mustache",
      "version": "1.0.0",
      "hash": "sha256:..."
    }
  ],
  "migrations": []
}
```

---

## 13. Summary Metadata

```json
{
  "metadata": {
    "thumbnail": "assets/thumbnail.png",
    "tags": ["esp32", "sensor", "education"],
    "summary": {
      "deviceCount": 3,
      "connectionCount": 6,
      "target": "ESP32 DevKit V1",
      "safetyLevel": "S2",
      "consistencyLevel": "C2",
      "lastBuildStatus": "success"
    }
  }
}
```

---

## 14. Manifest Validation Rules

Pre-simulation and pre-compilation validation requires:
1. Valid `schemaVersion`.
2. `projectType` equals `wink-embedded`.
3. Board and peripheral models match Registry Lock hashes.
4. Pin connections pass hardware constraints.
5. `sourceHash` matches actual DSL/C files.
6. Safety level matches actual gate execution.

---

## 15. Version Migration Strategy

```text
Load Manifest
  ↓
Check schemaVersion
  ↓
Load Registry Lock
  ↓
Execute Manifest Migration if needed
  ↓
Execute Registry Migration if needed
  ↓
Revalidate Devices/Connections/Logic
  ↓
Emit Migration Report
```

Migration report example:

```json
{
  "fromSchemaVersion": 1,
  "toSchemaVersion": 2,
  "changes": [
    {
      "type": "addField",
      "path": "simulation.workerLimits.maxTraceEvents",
      "value": 10000
    }
  ],
  "requiresUserConfirmation": false
}
```

---

## 16. MVP Minimum Viable Fields

1. `schemaVersion`
2. `projectType`
3. `id/name`
4. `target`
5. `devices`
6. `connections`
7. `logic.sourceType`
8. `simulation.workerLimits`
9. `safety.level/staticCheck`
10. `trace.consistencyLevel`
11. `device-registry.lock.json`
