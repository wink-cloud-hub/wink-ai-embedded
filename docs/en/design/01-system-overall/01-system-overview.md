# 01. Universal Low-Code AI Embedded Development Platform: System-Level Overall Architecture Design

<!-- i18n-meta
source: docs/zh/design/01-system-overall/01-system-overview.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

> **Core Vision**: Wink-AI is a low-code development, behavior-level simulation, and physical deployment platform for AI-generated embedded applications. Users compose business logic via drag-and-drop visual components or AI generation, execute safe sandbox verification, fault injection testing, and Golden Trace consistency tracking in the browser via WebAssembly; once verified, the logic is compiled in cloud-isolated environments and flashed to physical development boards via user-authorized WebSerial/WebUSB.

---

## 1. Core Platform Pain Points & Solutions

Traditional embedded development faces the following bottlenecks:

1. **Heavy Hardware Dependencies & High Barrier to Entry**: Developers or AI generators must understand registers, pin multiplexing, electrical timings, and vendor SDKs, making business logic difficult to reuse and verify.
2. **Safety Risks in AI-Generated Code**: AI-generated C code may contain infinite loops, null pointers, out-of-bounds accesses, incorrect state machines, or hazardous control logic, presenting extreme risks if flashed directly to physical hardware.
3. **Low Web Simulation Performance for Microscopic Signals**: Cycle-by-cycle simulation of GPIO, I2C, and UART waveforms creates excessive high-frequency JS/Wasm communication, resulting in unacceptable browser performance.
4. **Lack of Evidence Chain for Virtual-Physical Consistency**: Visual simulation alone cannot prove that physical behavior matches simulated behavior; structured tracing, replay, and differential comparison are required.
5. **Fragmented Toolchains & Flashing Workflows**: Requiring users to install ESP-IDF, ARM GCC, USB drivers, and flashing tools introduces severe onboarding friction.

Wink-AI's Solutions:

* **App/BAL/DAL/PAL 4-Layer Decoupling**: User business logic, reusable algorithm libraries, device semantics, and platform capabilities are strictly separated.
* **Device Model Registry Single Source of Truth**: Unifies peripheral models, properties, pin maps, DAL APIs, simulation strategies, hardware constraints, and code generation rules.
* **5-Channel Data-Plane Routing (Channel-Routed Bypass)**: Pin-level (Channel 1), PWM Modulation (Channel 1b), Protocol Bus (Channel 2), Analog Signal (Channel 3), and Buffer Payload (Channel 4) split traffic by scenario; bypasses are fully sunk into the PAL platform layer, keeping DAL and App code 100% identical between simulation and physical MCU.
* **Safety Sandbox Chain**: App Safe Codegen, static linting, Wasm Worker watchdog, isolated compilation containers, and firmware manifests.

> **Terminology Clarification**:
> - ✅ **App Layer**: User code or AI-generated one-off business logic (`app_init/app_loop/app_on_fault`).
> - ✅ **BAL Layer**: Business Abstraction Layer, encompassing **Physical Augmentation** (`input` / `output` / `sensor` / `actuator` / `display` / `comm`), **`math` pure algorithms**, and **`control` closed-loop orchestration** domains, forming the core component library of the `wink-micro-os` kernel (`wink-micro-os/bal/`).
> - ⚠️ **Historical Context**: In early documentation, "DAL Bypass" referred to replacing DAL drivers with `#ifdef SIMULATION`. This has been phased out. Active UniSim 3.0 mandates that **all bypasses sink into the PAL platform layer (PAL Physical Source Bypass)**, ensuring DAL and App are 100% single-source dual-target.
* **Golden Trace Consistency Verification**: Records key semantic events across simulation and hardware, supporting trace replay, diffing, and CI regressions.

---

## 2. System Layered Architecture

```mermaid
graph TD
    Input[AI / Low-Code Input] --> SafeCodegen[wink CLI Codegen / Static Checks]
    SafeCodegen --> App[Application Layer App]

    Registry[Device Model Registry] --> SafeCodegen
    Registry --> DeviceTree[device_tree Generation]
    Registry --> WebSchema[SchemaForm / Canvas Validation]
    Registry --> SimModel[Simulation Model / Fault Model]

    App -->|Calls Business Abstraction| BAL[Business Abstraction Layer BAL]
    BAL -->|Device Semantic API| DAL[Device Abstraction Layer DAL]
    DeviceTree --> DAL

    subgraph WinkMicroOS[WinkMicroOS Runtime (Single-Source C Code)]
        BAL
        DAL -->|Bus & System API| PAL[Platform Abstraction Layer PAL]
        Trace[Golden Trace Runtime]
    end

    PAL -.->|PAL Wasm Target / Channel Bypass| WasmBridge[Wasm-JS Bridge]
    PAL -.->|Static Target Binding| Target[Target PAL: ESP32 / STM32]

    subgraph Monorepo[Wink-AI Monorepo Frontend & Sim]
        WasmBridge --> Worker[@wink-ai/unisim Worker]
        Worker --> Watchdog[Watchdog / Resource Limit]
        Worker --> UniSim[UniSim Virtual Peripherals]
        UniSim --> UI[@wink-ai/embedded-frontend Canvas]
    end

    subgraph CloudBuild[wink CLI / Cloud Build]
        BuildContainer[Isolated Build Environment] --> Firmware[Firmware + Manifest + sha256]
    end

    Target --> Hardware[Physical MCU]
    Firmware --> Flash[WebSerial / WebUSB Flash]
    Flash --> Hardware
    Trace --> Compare[Trace Replay / Compare]
```

---

### 2.1 Heterogeneous MCU Simulation Four-Tier Taxonomy

To simultaneously support **AI code generation across heterogeneous chips**, **zero-code migration of existing open-source codebases (Arduino/C51)**, and **ultra-low-cost domestic 8-bit OTP microcontrollers (Yiwu electronics, Yuyao/Cixi home appliances)**, the simulation platform establishes a four-tier architecture taxonomy (see [ADR-0064](../../decisions/unisim/0064-chip-simulation-four-tier-taxonomy.md)):

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        Tier 1: AI-Native Unified OS Architecture                       │
│     wink-micro-os runs on both Sim & HW; user code uses Role-Action semantics (ESP32)  │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                     Tier 2: Zero-Code API / HAL Interception Proxy                     │
│  Wasm intercepts calls in Sim; HW runs native unmodified source (2-1: C51 / 2-2: HAL) │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                   Tier 3: 1:1 Instruction-Level Micro-ISA Emulation                    │
│    Sim interprets raw binary via virtual CPU; HW flashes native hex (PDK, FMD, etc.)   │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                       Tier 4: Hybrid Collaborative Emulation Architecture              │
│       Timing-critical ISA engine + High-throughput C++ Proxy Peripherals (Hybrid)      │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

#### Four-Tier Simulation Comparison & Selection Guide

| Tier | Core Principle | Representative Chips & Ecosystems | Key Strengths & Use Cases | Code Fidelity & Invasiveness |
| :--- | :--- | :--- | :--- | :--- |
| **Tier 1: AI-Native Unified OS** | `wink-micro-os` runs on both simulation and real hardware; PAL bypasses physical sources | ESP32, STM32F4, RP2040, Linux | **Optimal for AI codegen**: Business logic written in `Role-Action` physical semantics; zero code changes when switching MCUs | 100% code parity between sim & HW |
| **Tier 2: Zero-Code Proxy** | Host intercepts standard APIs / HALs and stubs forwarding to UniSim bus | 2-1: C51 family<br>2-2: Arduino, STM32 HAL | **Legacy & open-source compatibility**: Runs user C/C++/Arduino code in Web with zero source modifications | Zero invasiveness; HW runs untouched source |
| **Tier 3: 1:1 ISA Emulation** | UniSim embeds dedicated 8/16-bit ISA virtual machines / interpreters | Padauk (PDK), FMD, Nyquest, Puya | **Mass-market ultra-low-cost MCUs**: Tailored for Yiwu toys / Cixi appliances with ROM < 2KB, no libc, strict cycle timings | Zero invasiveness; HW flashes raw vendor Hex/Bin |
| **Tier 4: Hybrid Collaborative** | Core timing runs via ISA engine + High-bandwidth peripherals via C++ Proxy | Heterogeneous multi-core toy chips, complex industrial controllers | **Cycle accuracy + high performance**: Solves full-ISA emulation bottlenecks while enabling high-framerate rendering | Zero invasiveness / minimal glue overhead |

> 📌 **Architectural Status**: Current MVP (Phase 0~1) implements **Tier 1** as the high-confidence sandbox for AI-generated applications; **Tier 2** serves as the ecosystem compatibility layer in Phase 2; **Tier 3 & Tier 4** represent strategic technical assets targeting billion-dollar domestic consumer electronics product lines.

---

## 3. Cross-Repository 5-Core Pillars

According to platform trade secret isolation specifications and monorepo physical separation architecture, the system operates through 5 coordinated core modules. External modules outside this repository (such as `embedded-frontend` and `unisim`) strictly adhere to the **Black-Box Contract Insulation Principle**: **Describing only module responsibilities, usage scenarios, external API / DTO / CLI contracts, and input/output artifacts without exposing private main-repo algorithms or proprietary implementation details**.

### 3.1 Cross-Repo 5-Core Pillars Quick-Reference Matrix

| Module Name | Physical Location & Path | Black-Box Core Responsibility | Typical Usage & Invocation | Interface & Contract Format | Proprietary & Code Isolation Boundary |
|---|---|---|---|---|---|
| **`embedded-frontend`** | Monorepo<br>`wink-ai/packages/embedded-frontend/` | Embedded Web Workbench UI: 2D circuit topology canvas (HCTR), 3D product world mechanical/physics rendering, Pinia state tree, build/flash wizard | Browser developer operations, or consumed by Wink-AI master project via iframe / route mounting | `wink-app.json` Manifest, `SimTraceSpecV2`, WebSocket / Wasm message DTOs | Black-Box Contract: Defines UI interactions and Manifest DTOs, hiding proprietary rendering optimizations and commercial editor logic |
| **`unisim`** | Monorepo<br>`wink-ai/packages/unisim/` | Unified WebAssembly behavior-level high-fidelity simulation engine: microsecond `VirtualClock`, 4-value logic arbitration (0/1/Z/X), interrupt/PWM/fault injection Worker | Loaded by `embedded-frontend` inside Web Worker, or executed in Headless mode via `wink test` CLI | `SimWorker` communication protocol, Wasm-JS Bridge C-ABI (`wasm_bridge.h`) | Black-Box Contract: Defines engine runtime interfaces and ABI specifications, hiding internal high-performance state machines and bytecode optimizations |
| **`wink-tools`** | This Repo<br>`wink-ai-embedded/wink-tools/` | Unified embedded CLI & developer toolchain: encompasses code generation (`wink gen`), static linting (`wink lint`), Headless simulation testing (`wink test`), multi-target builds (`wink build`), packaging & flashing (`wink pack`/`wink esp32`) | Developer terminal execution, CI/CD automated pipelines, Web backend build worker pipeline invocation | `wink <verb>` verb command set, JSON Telemetry Structured Envelope | Open-source/core CLI toolchain in this repo, exposing complete Python implementation and driver description YAML roots |
| **`wink-micro-os`** | This Repo<br>`wink-ai-embedded/wink-micro-os/` | C-language lightweight embedded SDK kernel: PAL/DAL/BAL 3-tier abstraction, cooperative runtime scheduler, `wink_status_t`, Golden Trace runtime | Linked by `wink-micro-app`, built via `wink build` into ESP32/STM32 firmware or Wasm simulation bytecode | C public header surface (`pal.h`/`dal_*.h`/`wink_bal_opts.h`), CMake Targets | Open-source/core C SDK kernel in this repo, exposing complete low-level driver abstractions and main scheduling loop |
| **`wink-micro-app`** | This Repo<br>`wink-ai-embedded/wink-micro-app/` | Embedded application project specification: Manifest (`wink-app.json`), handwritten/AI-generated App C code (`app_main.c`), and generated device tree (`device_tree.c`) | Logical project created by app developers or AI tools, serving as top-level input for compilation and simulation | `wink-app.json` Schema v1/v2, `app_init` / `app_loop` callback contract | Open-source/project template in this repo, exposing App lifecycle specification and standard sample library |

---

### 3.2 Detailed Module Usage & Integration Guide

#### 1. `embedded-frontend` (Frontend Workbench)
* **Responsibility**: Provides a professional embedded IDE experience supporting 2D circuit wiring, 3D mechanical physics synchronized rendering, property inspection, AI assistant interactions, and one-click build/flash wizard.
* **How to Use**:
  - Standalone Dev Mode: Run `bun run dev` under `wink-ai/packages/embedded-frontend/` to launch Vite dev server.
  - Host Integration Mode: Embedded into main project via `<iframe src="/simulator/?projectId=xxx">`, exchanging Project Manifest data via `window.postMessage`.

#### 2. `unisim` (Wasm Simulation Engine)
* **Responsibility**: Simulates MCU core and virtual peripherals at behavior level inside browser sandbox, supporting microsecond virtual clock, channel arbitration, interrupts, and fault injection.
* **How to Use**:
  - Frontend Rendering Mode: Instantiated by `embedded-frontend`'s `SimWorker` with binary communication channels.
  - Headless CI Mode: Invoked via `wink test` or Node.js environment calling `headless-sim-runner` to run automated spec assertions.

#### 3. `wink-tools` (Unified Dev & Build CLI)
* **Responsibility**: Serves as core orchestrator for embedded development, integrating code generation, static analysis, Headless simulation testing, cloud/local build packaging, and ESP32/STM32 flashing.
* **How to Use**:
  - CLI commands:
    ```bash
    wink doctor                       # Diagnose local environment and toolchains
    wink gen app                      # Generate app_main.c / device_tree.c from Manifest
    wink lint --strict                # Execute P-stack / packed ban / brace static analysis
    wink build -t esp32               # Compile target firmware
    wink test                         # Run Headless simulation tests and consistency assertions
    ```

#### 4. `wink-micro-os` (C SDK Kernel)
* **Responsibility**: Shields chip register and bus discrepancies, providing App with stable, unified device APIs (DAL) and business abstractions (BAL), ensuring identical execution between simulation and hardware.
* **How to Use**:
  - Included in application `CMakeLists.txt` via `add_subdirectory(wink-micro-os)`, linking `libpal`, `libdal`, and `libbal` as needed.

#### 5. `wink-micro-app` (Application Project Specification)
* **Responsibility**: Defines standard physical boundary for a single embedded project, containing authoritative manifest `wink-app.json` and generated business logic source code.
* **How to Use**:
  - Place `wink-app.json` in `wink-micro-app/` directory and run `wink build` to compile the executable.

---

## 4. Layer Responsibilities

| Layer | Core Responsibility | Primary Artifacts | Audience |
|---|---|---|---|
| AI/Low-Code | Generates business intent, state machines, peripheral topology | DSL, Blockly, App draft | End users, AI Assistants |
| App | Describes business state machines and control policies, touches no hardware buses | `app_init/app_loop/app_on_fault` | App Developers |
| BAL | Encapsulates physical augmentation, algorithms, and closed-loop control (Augmentation / math / control domains) | `wink_bal_opts.h`, `wink_xxx_*` | Algorithm / Component Developers |
| DAL | Provides device semantic APIs, shields registers, buses, and timings | `dal_xxx_read/set` | Driver Maintainers |
| PAL | Abstracts GPIO/PWM/I2C/SPI/ADC/OSAL | `pal_hal.h`, `pal_osal.h` | Platform Adapters |
| runtime | Cooperative main loop, App lifecycle scheduling (callback injection) | `wink_runtime_run`, `wink_app_callbacks_t` | Platform Adapters / App Developers |
| trace | Golden Trace fault/event recording (cross-cutting foundational service) | `wink_trace_fault` | Test Engineers |
| Device Model Registry | Unifies peripheral, board, simulation, fault, and codegen metadata | JSON Schema, Model Library | Architects, Ecosystem Developers |
| UniSim | Browser virtual peripherals, canvas, protocol parsing, fault injection | TS Runtime, SchemaForm | Frontend Engineers |
| Cloud Build | Isolated builds, caching, artifact signing, manifests | `.bin/.hex`, build log | DevOps / Platform Engineers |
| Trace System | Records, replays, and compares simulation vs hardware behaviors | Golden Trace | Test Engineers |

### 3.1 Mapping to Classic Embedded 4-Layer Architecture (Ops Table Polymorphism)

> This section clarifies the relationship between the App/BAL/DAL/PAL 4-layer structure and the classic 4-layer architecture of `embedded-best-practice` (Application Layer / Abstraction Ops Table / Implementation Layer / Registration Layer + Platform Layer). This platform performs a **paradigm reconstruction rather than 1:1 mapping**, declared here to prevent implementers from misunderstanding it as a "brand new 4-layer ops architecture".
>
> See [2026-06-22 Architecture Review §2.1](../../reviews/core/2026-06-22-architecture-review.md) and [`02-wink-micro-os/01-dal-device-abstraction.md §2.1`](../02-wink-micro-os/01-dal-device-abstraction.md).

PAL adopts CMake static direct calls (matching Platform Layer conventions); DAL drops runtime ops table polymorphism in favor of named flat APIs + compile-time routing (trading dynamic extensibility for AI generability and simulation performance); registration layer responsibilities are handled by `device_tree` code generation.

| Classic 4-Layer Mechanism | Platform Realization | Relationship |
|---|---|---|
| Application Layer (Handles only, oblivious to subclasses) | App (Includes only `device_tree.h`, calls only `bal_xxx` / `dal_xxx`) | ✅ Identical contract, zero App modification on hardware change |
| Abstraction Layer ops table polymorphism (`me->ops->on(me)`) | DAL named flat APIs | ⚠️ Paradigm reconstruction |
| `container_of` subclass inference | None (DAL has no parent-child struct hierarchy) | ⚠️ Intentionally omitted |
| Implementation layer populates ops table | Independent `.c` per device + static dispatch | ⚠️ Compile-time routing replaces runtime dispatch |
| Registration Layer (`board_init` / `MODULE_INIT`) | `device_tree.c` code generation | 🔄 Replaced with generative approach |
| Platform Layer static direct calls | PAL CMake static binding | ✅ Identical (Aligned with workspace HAL static dispatch policy) |

**Rationale for Trade-off**: Within MVP scope, a device model typically has a single hardware realization, obviating the need for runtime polymorphism; named APIs are far more AI-friendly and statically verifiable; static dispatch has zero runtime overhead, boosting Wasm simulation performance and binary footprint. **The trade-off** is giving up unified dynamic device polymorphism—adding a new peripheral requires a dedicated API rather than filling out an ops table.

---

## 5. Dual-Mode Execution Core Mechanisms

### 4.1 Web Simulation Mode

1. After user generates App and topology, the platform runs App static safety checks.
2. Device Model Registry generates `device_tree.c/h`, SchemaForm properties, simulation registry info, and fault models.
3. App, BAL, DAL, and PAL Wasm targets are compiled to `wasm32`.
4. Frontend spawns Web Worker running Wasm, while the main thread manages UI rendering.
5. Asyncify handles blocking delays like `pal_delay_ms`, while a watchdog prevents infinite loops and runaway resource consumption.
6. UniSim interacts with Wasm via 5 data-plane channels (Pin / PWM / Protocol Bus / Analog / Buffer) and PAL platform bypasses.
7. Simulation execution writes to Golden Trace for replay, fault testing, and CI regression.

### 4.2 Hardware Deployment Mode

1. After simulation and required fault tests pass, user selects target board.
2. Cloud build service launches corresponding toolchain inside an isolated container.
3. Compiler links App, BAL, DAL, PAL target, and device tree to generate firmware.
4. Returns firmware binary, sha256 checksum, build manifest, and compilation logs.
5. Browser requests user authorization via WebSerial/WebUSB and executes flashing.
6. Hardware runs WinkMicroOS and can stream UART trace events for comparison against simulation traces.

---

## 6. Simulation Fidelity Boundaries

Wink-AI's primary goal is **behavior-level high-fidelity simulation**, not full electrical-level simulation.

| Simulation Tier | MVP Focus | Description |
|---|---|---|
| Behavior-Level Simulation | Yes | Verifies business state machines, sensor semantic values, actuator commands |
| Protocol-Level Simulation | Yes | Verifies I2C/UART/SPI payload-level exchanges |
| Logic-Level Simulation | Partial | Supports LED, Button, simple GPIO levels |
| Electrical-Level / SPICE Simulation | No | Does not simulate currents, impedance, noise, power integrity |
| Instruction-Level MCU Simulation | No | Does not execute QEMU/AVR/RP2040 instruction emulators as primary path |

Product messaging must avoid claiming "100% replacement for real hardware testing". Recommended phrasing:

> Wink-AI provides behavior-level high-fidelity simulation, helping users verify business logic, peripheral interactions, and exception handling before flashing to physical hardware.

---

## 7. Safety & Trust Chain

```text
S0 Unchecked Code
 ↓ Static checks pass
S1 Simulatable
 ↓ Wasm sandbox + watchdog pass
S2 Compilable
 ↓ Isolated container compilation + complete manifest
S3 Flashable
 ↓ Physical trace verified
S4 Verified Configuration
```

Key Constraints:

1. AI-generated code is untrusted by default.
2. BAL is forbidden from calling PAL directly.
3. Ignoring `wink_status_t` return values is a blocking compilation/lint error.
4. Wasm Worker must support heartbeat monitoring and forced termination.
5. Cloud compilation containers must not mount secrets or have default internet access.
6. Flashing must be explicitly authorized by the user through browser dialogs.

---

## 8. Core Commercial & Technical Value

1. **Lower Embedded Prototyping Cost**: Users verify control logic and peripheral interactions in the browser before moving to hardware.
2. **Controllable & Auditable AI-Generated Code**: Static checks, sandboxing, fault injection, and tracing elevate AI code from "generatable" to "verifiable".
3. **Unified Virtual & Physical Execution Paths**: Single-source App execution with BAL/DAL/PAL layered isolation reduces platform migration friction.
4. **Performance-First Browser Simulation**: Protocol and PAL physical source bypasses eliminate microscopic waveform overhead.
5. **Complete Closed-Loop Product Experience**: From requirements, canvas, simulation, compilation, and flashing to hardware trace comparison, forming an end-to-end workflow.

---

## 9. MVP Focus Scope

Phase 1 focuses on:

| Scope | Content |
|---|---|
| Target Boards | ESP32 DevKit V1 / STM32F4 |
| Peripherals | LED, Button, Servo, HC-SR04, SSD1306 OLED |
| Buses | GPIO, PWM, I2C |
| Simulation | Wasm Worker, Asyncify, PAL physical source bypass, Protocol Bypass |
| Safety | App static analysis, Worker watchdog, error status codes |
| Deployment | Cloud ESP-IDF compilation, Chrome/Edge WebSerial flashing |
| Verification | Golden Trace basic events, fault injection timeout/disconnect |

Postponed: STM32/RP2040 advanced variants, multi-board networking, complex 3D robotic arms, ngspice electrical simulation, and full WebUSB DFU.

---

## 10. AI-MCU Collaborative Architecture in Complex Smart Hardware (Cerebrum & Cerebellum Pattern)

To support complex smart embedded products, the platform physically and logically decouples "high-level AI decision algorithms" from "low-level real-time control logic", adopting a distributed heterogeneous collaborative architecture.

### 10.1 Division of Responsibilities: Cerebrum vs Cerebellum

- **High-Level AI Decision Layer (Cerebrum)**: Runs on high-performance edge compute chips (Linux/Cortex-A/NPU) or in the cloud, handling visual recognition, NLP, LLM Agent planning, SLAM pathfinding, and other compute-heavy, non-deterministic, high-latency tasks.
- **Low-Level Control Layer (Cerebellum / WinkMicroOS)**: Runs on real-time MCUs (Cortex-M/ESP32), handling closed-loop motor control (PID), real-time sensor updates, local safety policies, and watchdogs. It guarantees microsecond/millisecond hard real-time determinism and physical safety.

### 10.2 Virtual-Physical Hybrid Four-Layer Architecture

Based on distributed heterogeneous collaboration, the system completely decouples application logic from hardware, platforms, and simulation environments via a 4-layer hybrid architecture:

```text
+-----------------------------------------------------------------------------------+
|  1. AI & Decision Layer (Cerebrum)                                                |
|     - Location: Host SoC (Linux/Cortex-A/NPU) or Cloud                            |
|     - Responsibility: Object detection (YOLO), voice, SLAM, LLM Agent decisions   |
+-----------------------------------------------------------------------------------+
                                       │
                                       │ Protocol Comm (Protobuf / JSON over UART/SPI/WiFi)
                                       ▼
+-----------------------------------------------------------------------------------+
|  2. WinkOS App Layer (Cerebellum)                                                 |
|     - Location: Real-time MCU (WinkOS Core)                                       |
|     - Responsibility: Executes AI decision commands, local safety state machines  |
|     - Highlights: AI Agent friendly, single-source dual-target (Wasm/MCU)         |
+-----------------------------------------------------------------------------------+
                                       │
                                       ▼
+-----------------------------------------------------------------------------------+
|  3. WinkOS BAL Layer (Glue / Co-processor)                                        |
|     - Responsibility: Coordinates DAL hardware & Runtime tasks (sampling, filter) |
+-----------------------------------------------------------------------------------+
                                       │
                                       ▼
+-----------------------------------------------------------------------------------+
|  4. WinkOS DAL / PAL Layer (Chassis)                                              |
|     - Responsibility: Shields chip/peripheral differences, standard physical APIs  |
+-----------------------------------------------------------------------------------+
```

1. **AI & Decision Layer (Cerebrum)**:
   * Runs independently of real-time MCU. Communicates with underlying WinkMicroOS via non-blocking protocols.
   * In Web simulation, this layer can be simulated via mock AI service nodes or local JS models.
2. **WinkOS App Layer (Cerebellum)**:
   * Directly scheduled by WinkMicroOS, providing `app_init`, `app_loop`, `app_on_fault` lifecycle callbacks.
   * Compiles natively on hardware and runs in Wasm sandbox on Web, ensuring 100% identical behavior.
3. **WinkOS BAL Layer**:
   * Bridges App and DAL/PAL, encapsulating hard real-time edge algorithms such as PID control and sensor filtering without high-level AI intervention.
4. **WinkOS DAL/PAL Layer**:
   * Low-level driver and system service abstraction. In simulation, routes to frontend 3D physics engines via Wasm-JS Bridge; on hardware, statically binds to ESP32/STM32 physical peripherals.

### 10.3 Interfaces & Boundaries (BAL Positioning)

> **Active Specification SSOT**: [02-wink-micro-os/06-bal-layer.md](../02-wink-micro-os/06-bal-layer.md) (3 Domains, Naming, CI). Decisions: [ADR-0037](../../decisions/core/0037-bal-domain-partition-and-closed-loop-motor.md), [ADR-0038](../../decisions/core/0038-bal-naming-hard-cut-and-layer-ssot.md).

- **BAL Stays Lightweight**: BAL contains only real-time algorithms directly tied to underlying hardware (e.g., Kalman filter, moving average, PID control) and device augmentation/closed-loop components. Heavy neural networks or macro planning are strictly excluded to prevent runtime bloat on low-end MCUs.
- **3 Domains**: Physical Augmentation (single DAL), `math/` (pure algorithms), `control/` (cross-device closed-loop/orchestration). See `06-bal-layer.md`.
- **Communication Bridge**: BAL can host communication protocol adapters (e.g., default telemetry service) to broadcast real-time state to high-level AI and receive control commands.
- **Fail-Safe Degradation**: If high-level AI encounters out-of-memory errors, crashes, or network timeouts, the WinkMicroOS App state machine autonomously takes over upon detecting communication timeouts to execute local emergency braking, return-to-home, or alarms. Closed-loop controls must implement trip-on-feedback-failure (ADR-0037).

### 10.4 Intent Command & Data Contract

In heterogeneous collaboration, the App layer's most crucial responsibility is **encapsulating and isolating "intent commands" for the intelligent decision layer**, avoiding the anti-pattern of direct physical pin manipulation:

1. **Intent Control Command Encapsulation**:
   * App layer defines explicit control semantics (e.g., "set chassis target velocity `0.5m/s`", "move robotic arm to coordinate `(X,Y,Z)`").
   * Command parameters must use standard physical units (m/s, degrees, mm), never exposing physical pin numbers, PWM duty cycles, or raw timings to the decision layer.
   * This provides a natural Function-Calling Schema for AI Agents, making generation and verification seamless.
2. **Semantic Telemetry Data Output**:
   * Low-level sensor data processed by BAL (filtering, state fusion) is pushed by the App layer in semantic form (e.g., "battery percentage", "obstacle distance ahead"), filtering raw ADC readings and pin bounce.
3. **Local Real-Time Override & Veto Power**:
   * WinkMicroOS App layer holds ultimate execution veto power based on local physical constraints (e.g., ultrasonic anti-collision, limit switch triggers). Even if high-level AI hallucinates or sends hazardous commands, the App layer intercepts them locally to ensure physical safety.
