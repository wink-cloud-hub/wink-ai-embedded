# 02. MVP Product Roadmap, Capability Boundaries & Phased Delivery Plan

<!-- i18n-meta
source: docs/zh/design/01-system-overall/02-mvp-roadmap.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

Wink-AI's long-term vision is an integrated platform for low-code AI embedded development, high-performance browser simulation, and one-click physical MCU deployment. To prevent overextended architecture from hindering implementation, the MVP phase must focus on the shortest closed loop: enabling users to create a viable ESP32 project in the browser, complete simulation, verify exceptions, compile in the cloud, and flash via WebSerial.

---

## 1. Product Positioning

Wink-AI MVP Core Positioning:

> A safe "simulate first, flash later" development platform for AI-generated embedded applications.

Core Target Users:

| User Segment | Core Needs | MVP Value Proposition |
|---|---|---|
| Embedded Beginners | Cannot configure toolchains | Develop and flash entirely within the browser |
| Makers / Educators | Rapid hardware demos | Drag-and-drop peripherals, simulate, verify on hardware |
| AI Codegen Users | Worried about code damaging hardware | Wasm sandbox and fault testing prior to flashing |
| Prototyping Teams | Rapidly verify control logic | Behavior-level simulation lowers trial-and-error costs |

---

## 2. MVP North Star Metrics

1. User creates project to running Web simulation: < 3 minutes.
2. User passes simulation to successful ESP32 flashing: < 5 minutes.
3. Official sample simulation success rate: > 95%.
4. WebSerial flashing success rate: > 90%.
5. AI-generated App static checks intercept hazardous code: 100%.

---

## 3. MVP Scope

### 3.1 Must-Have Capabilities

| Module | Scope |
|---|---|
| Target Boards | ESP32 DevKit V1 / STM32F4 / Host Wasm |
| Peripherals | LED, Button, RC/Industrial Servo, HC-SR04, SSD1306 OLED, FOC Motor, Closed-loop DC Motor, Latching Relay, ADC Analog Sensors |
| Buses & Abstractions | GPIO, PWM (Router), I2C (v6 compatible), ADC Subsystem, RMT |
| Simulation | Wasm Worker (@wink-ai/unisim), Asyncify, PAL Physical Source Bypass, Protocol Bypass |
| Code Generation | App Templates, device_tree generation (`wink gen`) |
| Safety | App static analysis, Worker watchdog, error status codes (`wink_status_t`) |
| Compilation | Cloud / Local Docker & `wink build` CLI |
| Flashing | Chrome/Edge WebSerial/WebUSB flashing (`wink flash`) |
| Verification | Golden Trace basic events & differential comparison |

### 3.2 Postponed Capabilities

| Capability | Reason for Postponement |
|---|---|
| RP2040 | Requires standalone toolchain and UF2 workflow |
| Multi-Board Communication | State synchronization and clock models are complex |
| ngspice Circuit Simulation | Inconsistent with behavior-level MVP value |
| Complex 3D Robotic Arms | High modeling and asset creation costs |
| Wi-Fi / BLE Cloud Connectivity | Complex security and network provisioning |
| Custom User C Libraries | High security audit overhead |

---

## 4. Phased Roadmap

### Phase 0: Architectural Skeleton

Goal: Validate minimum viable runtime pipeline.

Deliverables:

1. App/BAL/DAL/PAL foundation directories.
2. `wink_status_t`.
3. LED/Button/Servo DAL.
4. Wasm Worker loader and heartbeat.
5. Simple project JSON schema.
6. Minimal `device_tree` generator.

Acceptance Criteria:

```text
User drags LED + Button, clicks Simulate, pressing button toggles LED state.
```

### Phase 1: Behavior-Level Simulation Loop

Goal: Prove feasibility of PAL physical source bypass routing.

Deliverables:

1. HC-SR04 ECHO edge pulse injection and PAL bypass.
2. Servo output simulation.
3. Obstacle distance slider/input panel.
4. `app_loop` watchdog.
5. Fault injection: timeout/disconnect.
6. Golden Trace basic recording.

Acceptance Criteria:

```text
Obstacle-avoidance robot demo actuates servo based on distance changes in simulation; enters fault state upon sensor timeout.
```

### Phase 2: Protocol-Level Bypass

Goal: Validate I2C / OLED transaction-level simulation.

Deliverables:

1. ✅ `pal_i2c_transfer` Wasm interception (`js_pal_i2c_transfer` validated SSD1306 path).
2. ✅ SSD1306 virtual display (`dal_ssd1306`: framebuffer + 6x8 font + paged flush, Protocol Bypass Level 2).
3. ✅ I2C address conflict detection (`PAL_RESOURCE_I2C_ADDR`, `(port, 7-bit addr)` granularity, device-owner claim).
4. ✅ Trace recording `pal.transfer` summary (owned by JS Worker side, `js_pal_i2c_transfer` payload with bus/port/size/status).

Acceptance Criteria:

```text
App calls OLED display APIs, Web Canvas renders text or graphics.
OLED Dashboard sample host e2e test passes (Button -> LED + I2C flush + non-empty framebuffer + no fault).
```

### Phase 3: Cloud Build & WebSerial Flashing

Goal: Achieve closed-loop physical deployment.

Deliverables:

1. ESP-IDF Docker compilation image.
2. Build manifest generator.
3. Firmware sha256 checksums.
4. WebSerial ESP32 bootloader handshake.
5. Flashing progress bar and failure recovery guidance.

Acceptance Criteria:

```text
Same obstacle-avoidance sample compiles to ESP32 binary upon passing simulation, and runs successfully via in-browser flashing.
```

### Phase 4: Consistency Verification

Goal: Prove behavioral equivalence between virtual and physical worlds.

Deliverables:

1. Physical MCU UART trace logger.
2. Trace comparison utility.
3. C1/C2/C3 consistency rating matrix.
4. Official sample golden traces.

Acceptance Criteria:

```text
Obstacle avoidance simulation trace matches physical MCU trace state transitions and actuator commands.
```

---

## 5. Official Sample Projects

MVP maintains 3 high-quality reference samples:

| Sample | Coverage Scope |
|---|---|
| Button LED | GPIO, Pin-level, basic codegen |
| Servo Radar | PAL physical source bypass, fault injection, state machines |
| OLED Dashboard | I2C Protocol Bypass, display rendering |

Each sample must provide:

1. Project topology definition.
2. App source code.
3. Device Model manifest.
4. Simulation trace.
5. Screenshot or expected behavior documentation.
6. Hardware deployment guide.

---

## 6. User Journeys

> 💡 **Complete UI/UX & Information Architecture Spec**: See [03-product-user-journey.md](./03-product-user-journey.md).

### 6.1 Beginner Users

```text
Select Template -> Enter Canvas -> Adjust Parameters -> Click Simulate -> View Results -> Click One-Click Flash -> Select Serial Port in Browser -> Flashing Succeeded
```

Key Experience:

1. Does not require installing ESP-IDF.
2. Does not require understanding GPIO multiplexing details.
3. Error messages must be actionable.

### 6.2 AI Codegen Users

```text
Enter Natural Language Prompt -> AI Generates App/Topology Suggestions -> Static Safety Checks -> Simulation Execution -> Fault Testing -> Compile & Flash
```

Key Experience:

1. Displays rule-based diagnostics upon AI generation failure.
2. AI never generates direct PAL calls.
3. Platform recommends adding exception handling logic.

### 6.3 Professional Developers

```text
Import Project -> Inspect Generated C Code -> Adjust Device Model Properties -> Run Trace Diff -> Download Firmware or Flash Online
```

Key Experience:

1. Full visibility into generated C code.
2. Trace export capability.
3. Downloadable firmware binary and build manifest.

---

## 7. Non-Goals

The MVP does not promise:

1. Electrical-level SPICE accuracy.
2. Replacement for oscilloscopes, logic analyzers, or physical benches.
3. Universal support for all MCUs and development boards.
4. 100% first-pass correctness for AI-generated code.
5. Complex multi-body dynamic physics simulations.
6. Flashing mechanisms outside the browser.

Marketing copy must use "behavior-level high-fidelity simulation" and avoid claims of "100% real hardware replacement".

---

## 8. Risks & Mitigations

| Risk | Mitigation Strategy |
|---|---|
| Incomplete WebSerial browser compatibility | Provide firmware binary download and desktop companion route |
| Hazardous AI-generated C code | App Safe Codegen + static linting + Wasm sandbox |
| Discrepancy between simulation and hardware | Golden Trace + consistency grading system |
| High device registry maintenance overhead | Unified Device Model Registry schema |
| High cloud compilation overhead | Pre-compiled runtime archives, incremental linking, cache isolation |
| User wiring mistakes | Canvas static validation, voltage/pin conflict warnings |

---

## 9. Success Criteria

MVP Compliance Definition:

1. All 3 official samples pass simulation end-to-end.
2. At least 1 sample successfully flashes to ESP32 hardware.
3. Sensor timeout/disconnect triggers fault handler.
4. Worker infinite loop is terminated by watchdog.
5. Build output includes manifest and sha256 checksum.
6. README clearly defines capability boundaries and browser compatibility.
