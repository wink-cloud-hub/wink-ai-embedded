# 03. Product User Journey, Information Architecture & Key UX Design

<!-- i18n-meta
source: docs/zh/design/01-system-overall/03-product-user-journey.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

This document defines Wink-AI's core user paths, page information architecture, critical decision gates, and failure recovery experiences from a product management perspective, ensuring low-level architectural capabilities translate into an intuitive, trustworthy, and low-friction user experience.

---

## 1. Product One-Liner

> Wink-AI is a safe embedded development platform where users compose business logic via AI or low-code, verify behavior in browser simulation first, and flash to physical development boards with one click.

---

## 2. Core Value Propositions

Ranked in order of priority:

1. **Safety**: AI-generated code is verified in Wasm sandboxes and fault tests before flashing is permitted.
2. **Low Barrier to Entry**: Users do not need to install cross-compilation toolchains locally.
3. **Rapid Closed-Loop**: Composition, simulation, compilation, and flashing occur seamlessly within the same browser workflow.
4. **Single-Source Logic**: The exact same application business logic runs across simulation and physical hardware.
5. **Professional Verification**: Validates exception execution paths via Golden Trace and fault injection.

---

## 3. User Personas

| Persona | Primary Goal | Critical Pain Point | Platform Solution |
|---|---|---|---|
| Educational Learners | Learn sensors and control logic | Intimidated by toolchains and driver SDKs | Pre-built templates, visual layout, instant feedback |
| Makers / Hobbyists | Rapid physical prototypes | Cumbersome wiring and flashing setups | One-click simulation and direct flashing |
| AI Codegen Users | Generate hardware logic from prompts | Worried about code safety risks | Static checks, sandboxing, exception testing |
| Professional Developers | Verify business control state machines | Concerned about excessive black-box lock-in | Full code visibility, trace export, open manifests |

---

## 4. Information Architecture

```text
Embedded Workbench (Dual-Viewport Workbench - design / simulate / diagnose modes)
├── Top Bar
│   ├── Target Board Selector (ESP32 / STM32)
│   ├── Workbench Mode Switcher (design | simulate | diagnose)
│   ├── Safety Level & Consistency Badge (S0 - S4)
│   └── Build & Flash Trigger
├─ Center Workspace (Dual-Viewport Split-Screen & Synchronization)
│   ├── Viewport A: Visual 2D Circuit Canvas (HCTR wiring, pin interaction, port validation)
│   └── Viewport B: Product World 3D (Three.js/WebGL mechanical, physics & environment model)
├── Right Panel (Context-Aware Property Inspector)
│   ├── Property Inspector (SchemaForm automatic rendering)
│   ├── Bindings Panel (Circuit pin to 3D sensor/actuator bindings)
│   └── Fault Injection Matrix (Fault injection configuration)
├── Left Drawer (Peripheral & Tool Library)
│   ├── Device Catalog (Board / Peripherals / Mechanical Assets)
│   └── AI Assistant / State Machine DSL Editor
└── Bottom Console
    ├── Trace Console (Golden Trace Spec v2 real-time timeline)
    ├── Static Check & Compiler Diagnostics (Monaco inline squigglies)
    └── Build & WebSerial / WebUSB Flash Wizard
```

---

## 5. Core Journey: From Template to Physical MCU

```text
Select Template
  ↓
Open Canvas
  ↓
Configure Peripheral Properties
  ↓
Generate / Edit Business Logic
  ↓
Run Static Safety Checks
  ↓
Execute Wasm Simulation
  ↓
Run Fault Injection Tests
  ↓
Trigger Cloud Compilation
  ↓
Inspect Firmware Manifest
  ↓
Authorize WebSerial Port
  ↓
Flash Physical Target
  ↓
Collect Hardware Trace
  ↓
Display Consistency Rating
```

---

## 6. Key Page States

### 6.1 Static Check State

| State | User View | Available Actions |
|---|---|---|
| Unchecked | "Please run safety check first" | Run Check |
| Passed | "Safety check passed, ready to simulate" | Run Simulation |
| Warning | "Potential risks detected, simulation permitted" | Inspect warnings, proceed with simulation |
| Error | "Blocking issues detected" | Navigate to code, trigger AI Fix |

### 6.2 Simulation State

| State | User View | Available Actions |
|---|---|---|
| Ready | Awaiting execution | Run |
| Running | Live canvas, trace stream | Pause / Stop / Inject Fault |
| Paused | Current state frozen | Resume / Stop |
| Faulted | Fault cause and safe-state posture | Inspect trace, trigger AI Fix |
| Watchdog Terminated | Potential infinite loop detected | View diagnostics, flashing locked |

### 6.3 Compilation State

| State | User View | Available Actions |
|---|---|---|
| Pending | Queued for build | Start Compilation |
| Building | Progress bar and live build logs | Cancel |
| Success | Firmware manifest and binary artifact | Flash / Download |
| Failed | Compilation error diagnostics | Trigger AI Fix / View full log |

---

## 7. Error Messaging Principles

All error notifications must include:

1. What happened.
2. Why it happened.
3. What the user can do next.
4. Whether the platform can automatically remediate it.

Example:

```text
Error: ECHO pin of front_radar outputs 5V, but ESP32 GPIO5 only supports 3.3V input.
Reason: Direct connection may damage the development board.
Recommendation: Insert a logic level shifter or resistor voltage divider module.
Actions: [Auto-Add Level Shifter] [Select Different Pin] [View Documentation]
```

---

## 8. AI Assistant Touchpoints

The AI assistant should not merely generate code, but actively participate in diagnostics and remediation.

| Scenario | AI Action |
|---|---|
| New Project Creation | Recommends templates and peripherals from natural language prompts |
| Wiring Conflicts | Explains root causes and proposes valid pin mappings |
| Static Check Failures | Refactors App code to comply with safety rules |
| Simulation Faults | Analyzes execution traces to pinpoint state machine bugs |
| Compilation Failures | Parses GCC error logs and applies precise code patches |
| Flashing Errors | Parses UART bootloader logs to guide manual recovery |

AI-generated code must always pass App Safe Codegen and static analysis before proceeding to flashing.

---

## 9. Build & Flash Wizard

Flashing wizard step sequence:

1. Select target hardware board.
2. Validate browser compatibility.
3. Display firmware manifest and sha256.
4. Request serial port permission.
5. Handshake with ROM bootloader.
6. Erase and write flash partitions.
7. Verify binary hash.
8. Soft-reset MCU into user runtime.
9. Optionally capture physical execution trace.

Failure recovery matrix:

| Failure Mode | Recovery Guidance |
|---|---|
| Serial port not found | Check USB cable, install CH340/CP2102 drivers, grant browser permissions |
| Bootloader handshake failed | Hold BOOT button, click EN/RST button, then retry |
| Flashing interrupted | Lower baud rate and retry |
| Hash verification failed | Re-compile or re-flash binary |
| Browser unsupported | Download firmware binary and flash via CLI toolchain |

---

## 10. Architectural Trust & Professionalism Design

To convey enterprise-grade reliability, the UI explicitly displays:

1. Device Model schema version.
2. Firmware sha256 hash.
3. WinkMicroOS runtime version.
4. Consistency grade (C1/C2/C3).
5. Golden Trace export button.
6. Fault injection verification outcomes.
7. Target board hardware pin matrix.

Example Health Status Card:

```text
Project Health
├── Static Check: Passed
├── Simulation: Passed
├── Fault Test: Sensor timeout handled
├── Consistency: C2
├── Target: ESP32 DevKit V1
├── Runtime: WinkMicroOS 0.1.0
└── Firmware: sha256:xxxx
```

---

## 11. Onboarding Templates

The MVP provides 3 standard starter templates:

1. **Blink LED**: Shortest path to verify GPIO and flashing pipeline.
2. **Servo Radar Obstacle Avoidance**: Showcases PAL physical source bypass, state machines, and fault handling.
3. **OLED Dashboard**: Showcases I2C protocol bypass and display framebuffer rendering.

Each template includes:

1. Objective description.
2. Circuit canvas topology.
3. Annotated code snippets.
4. Interactive simulation widgets.
5. One-click fault test triggers.
6. Physical hardware flashing guide.

---

## 12. Capability Boundary Messaging

Recommended Copy:

```text
Wink-AI provides behavior-level high-fidelity simulation, helping you verify business logic, peripheral interactions, and exception handling before flashing to physical hardware. It does not replace specialized electrical SPICE simulation, oscilloscopes, or final hardware validation benches.
```

Forbidden Claims:

```text
100% simulated real hardware.
Completely eliminates physical testing.
Supports every microcontroller on the market.
```

---

## 13. MVP Acceptance Criteria

Upon MVP completion, a user must be able to independently:

1. Create an ESP32 project from a template.
2. Adjust sensor threshold properties.
3. Run simulation and observe actuator responses.
4. Inject sensor timeouts and observe fault handling.
5. Trigger cloud builds and generate firmware binaries.
6. Flash ESP32 hardware via Chrome/Edge WebSerial.
7. Inspect firmware manifests and execution traces.
