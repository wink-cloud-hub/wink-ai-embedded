# 08. AI-Generated Code Security Sandbox, Wasm Isolation & Cloud Build Security Specification

<!-- i18n-meta
source: docs/zh/design/07-platform-governance/03-security-sandbox.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

The Wink-AI platform enables users to automatically generate C business logic via low-code interfaces and AI, compiling and executing builds in the browser and cloud. This capability must operate within strict security boundaries: untrusted code must never compromise host systems, cloud infrastructure, or physical hardware.

---

## 1. Security Boundary Overview

```text
[ AI / Low-Code Input ]
        │
        ▼
[ App Safe Codegen Static Constraints ]
        │
        ▼
[ Wasm Worker Sandbox Simulation ] ──► [ Watchdog / Resource Limit ]
        │
        ▼
[ Cloud Isolated Compilation Container ] ──► [ Artifact Manifest / Hash ]
        │
        ▼
[ WebSerial/WebUSB User Authorized Flashing ]
        │
        ▼
[ Hardware Runtime Fault Guard ]
```

Core Principles:
1. AI-generated code is untrusted by default.
2. Simulation must precede hardware deployment.
3. Build services must execute in isolated sandboxes.
4. Flashing requires explicit user authorization.
5. Hardware execution maintains fail-safe protection paths.

---

## 2. App Safe Codegen Constraints

### 2.1 Forbidden Capabilities

| Capability | Rule | Rationale |
|---|---|---|
| Dynamic Memory | Forbids `malloc/free/realloc/calloc` | Prevents fragmentation, memory leaks, and non-deterministic failures |
| Raw Pointer Math | Forbids complex pointer arithmetic | Mitigates buffer overflows |
| File I/O | Forbids `fopen/fread/fwrite` | MCUs lack universal filesystems; unexposed in Wasm |
| Network I/O | Forbids raw socket APIs | Prevents unauthorized network communication |
| Recursion | Forbids recursive functions | Prevents uncontrolled stack exhaustion |
| Infinite Loops | Forbids user `while(1)` | Prevents thread/worker lockups |
| Direct PAL | Forbids including `pal_hal.h` | Decouples App logic from hardware registers |
| Inline Assembly | Forbids `asm` | Non-portable and un-auditable |
| Packed / Raw Memcpy Serialization | Forbids `__attribute__((packed))` / `#pragma pack`; forbids `memcpy` of PODs to wire/flash | Prevents alignment crashes on ARM/Xtensa |

### 2.2 Recommended Code Structure

```c
void app_init(void);
void app_loop(void);
void app_on_fault(uint32_t fault_code);
```

`app_loop()` must return in bounded time; periodic scheduling is managed by WinkMicroOS.

---

## 3. Static Checking Rules

| Check Item | Severity |
|---|---|
| Inclusion of forbidden headers | error |
| Invocation of forbidden functions | error |
| Ignoring `wink_status_t` return values | error |
| Infinite loops in `app_loop()` | error |
| Recursive calls | error |
| Stack arrays exceeding threshold | warning / error |
| Floating point comparisons without epsilon ranges | warning |
| Unhandled fault codes | warning |

Diagnostic output structure:

```json
{
  "passed": false,
  "diagnostics": [
    {
      "severity": "error",
      "file": "app_main.c",
      "line": 42,
      "column": 5,
      "rule": "APP_NO_DIRECT_PAL",
      "message": "Direct calls to pal_gpio_write are forbidden in App layer. Use DAL APIs."
    }
  ]
}
```

---

## 4. Wasm Worker Sandbox Runtime Limits

Asyncify yields execution during delays, but cannot preempt malicious infinite loops. A watchdog protocol in the Wasm worker enforces preemption.

### 4.1 Worker Lifecycle States

```text
created -> compiling -> running -> paused -> stopped
                         │
                         ├── faulted
                         └── terminated_by_watchdog
```

### 4.2 Resource Limits

| Resource | MVP Value | Action upon Breach |
|---|---:|---|
| Wasm Memory | 16MB initial, 64MB cap | Terminate |
| Single `app_loop` Timeslice | 20ms | Warning; terminate on consecutive breaches |
| Worker Heartbeat | 100ms | Main thread detects missed heartbeats |
| IPC Message Rate | 1,000 msg/s per channel | Rate limiting & throttling |
| Trace Buffer | 10,000 entries | Overwrite oldest in ring buffer |
| Max Simulation Duration | User configurable | Pause simulation |

### 4.3 Watchdog Protocol

Worker sends periodic heartbeats:
```typescript
postMessage({
  type: 'runtime_heartbeat',
  timestampMs: performance.now(),
  loopCount,
  memoryBytes,
  state: 'running'
});
```

Main thread terminates on timeout:
```typescript
if (Date.now() - lastHeartbeatAt > 500) {
  worker.terminate();
  reportRuntimeFault('WASM_WORKER_HEARTBEAT_TIMEOUT');
}
```

---

## 5. Cloud Compilation Service Isolation

### 5.1 Container Security Policies

| Item | Requirement |
|---|---|
| Lifecycle | Ephemeral container / sandbox per build |
| Network | Outbound networking disabled |
| Filesystem | Read-only toolchain mount, temporary scratch workspace |
| User Privileges | Non-root execution |
| Resource Limits | Enforced cgroup CPU, memory, and timeout bounds |
| Secrets | No credentials mounted |

### 5.2 Build Request Schema

```json
{
  "projectId": "proj_123",
  "target": "esp32-devkit-v1",
  "sourceBundleHash": "sha256:...",
  "deviceModelManifest": {
    "schemaVersion": 1,
    "models": ["esp32-devkit-v1@1.0.0", "hc-sr04@1.0.0"]
  },
  "buildOptions": {
    "optimization": "Os",
    "enableTrace": true
  }
}
```

### 5.3 Build Response Schema

```json
{
  "buildId": "build_20260622_000001",
  "status": "success",
  "target": "esp32-devkit-v1",
  "durationMs": 4200,
  "artifacts": [
    {
      "name": "firmware.bin",
      "type": "esp32-merged-bin",
      "size": 1048576,
      "sha256": "..."
    }
  ],
  "warnings": [],
  "manifest": {
    "winkRuntimeVersion": "0.1.0",
    "palTarget": "targets/esp32",
    "sourceBundleHash": "sha256:...",
    "deviceTreeHash": "sha256:..."
  }
}
```

---

## 6. Firmware Artifact Trust Chain

Artifact manifests verify:
1. Source bundle hash.
2. Device Model Registry version and hash.
3. WinkMicroOS runtime version.
4. PAL target version.
5. Compilation options and flags.
6. Artifact sha256 checksum.
7. Build timestamp.

Displayed to user prior to flashing:
```text
Target Board: ESP32 DevKit V1
Firmware Size: 1.0 MB
Build Duration: 4.2s
Source Hash: sha256:xxxx
Device Models: hc-sr04@1.0.0, servo-sg90@1.0.0
```

---

## 7. WebSerial / WebUSB Security & Compatibility

### 7.1 User Authorization Principles
1. Browsers enforce native user prompt selection.
2. Silent background hardware access is prohibited.
3. Explicit target boards, firmware hashes, and risk prompts are displayed.
4. Recovery instructions provided on failures.

### 7.2 Compatibility Matrix

| Capability | Chrome / Edge | Firefox | Safari |
|---|---|---|---|
| WebSerial | Supported | Unsupported | Unsupported |
| WebUSB | Supported | Unsupported | Partial / Unsupported |
| ESP32 WebSerial Flashing | Supported | Unsupported | Unsupported |
| STM32 WebUSB DFU | Supported | Unsupported | Unstable |

---

## 8. Real Hardware Runtime Safety

WinkMicroOS hardware runtime integrates:
1. `app_loop()` watchdog.
2. Fault code tracing.
3. Actuator fail-safe states.
4. UART diagnostic logs.
5. Startup self-tests.
6. Device tree manifest validation.

```text
Boot -> pal_init -> device_tree_validate -> dal_init_all -> app_init -> app_loop scheduler
                         │
                         └── Fail -> app_on_fault / Safe Halt
```

---

## 9. Safety Level Definitions

| Level | State | Permitted Operations |
|---|---|---|
| S0 | Unchecked | Simulation and compilation blocked |
| S1 | Static Checks Passed | Wasm simulation allowed |
| S2 | Simulation Tests Passed | Cloud compilation allowed |
| S3 | Build Succeeded & Manifest Valid | Authorized user flashing allowed |
| S4 | Hardware Trace Verified | Verified configuration |

---

## 10. MVP Mandatory Security Capabilities

1. Static scanning for forbidden functions.
2. `wink_status_t` return checking.
3. Wasm Worker heartbeat watchdog.
4. Container CPU / Memory / Timeout constraints.
5. Firmware artifact sha256 manifest.
6. Pre-flash user confirmation modal.
7. Simulation fault injection supporting timeout and disconnect.
