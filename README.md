# WinkMicroOS

**AI can already write, run, test and fix software on its own. Embedded is the exception — firmware only proves itself on silicon, and silicon sits behind a human hand.**

WinkMicroOS is the deterministic digital lab that closes that loop: the same C source runs in a browser Wasm sandbox and on real MCUs — from ESP32 down to cost-optimized 8-bit industrial microcontrollers — and every run leaves reviewable PASS evidence behind. *Built for agents, not just humans.*

[![CI](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/pr.yml/badge.svg)](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/pr.yml)
[![Nightly](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/nightly.yml/badge.svg)](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/nightly.yml)
[![License Gate](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/license-gate.yml/badge.svg)](https://github.com/wink-cloud-hub/wink-ai-embedded/actions/workflows/license-gate.yml)
[![Release](https://img.shields.io/github/v/release/wink-cloud-hub/wink-ai-embedded?label=release)](https://github.com/wink-cloud-hub/wink-ai-embedded/releases/latest)
[![License](https://img.shields.io/badge/license-LGPL--3.0--only%20runtime-blue)](./.github/license-map.json)
![Platform](https://img.shields.io/badge/targets-wasm%20%7C%20esp32%20%7C%208051%20%7C%20avr%20%7C%20pdk-informational)
![Docs](https://img.shields.io/badge/docs-English%20%7C%20%E7%AE%80%E4%BD%93%E4%B8%AD%E6%96%87-success)

**English** | [简体中文](./README.zh-CN.md)
&nbsp;·&nbsp; [▶ Try it online](http://www.wink-ai.com/simulator/index.html) &nbsp;·&nbsp; [5-min guide](./docs/en/design/00-quick-start/01-5min-getting-started.md) &nbsp;·&nbsp; [Docs hub](./docs/en/README.md) &nbsp;·&nbsp; [Roadmap](./docs/en/design/01-system-overall/02-mvp-roadmap.md)

**Status:** released · public CI green · test executables (see [`wink-micro-os/TESTING.md`](./wink-micro-os/TESTING.md))

<!-- TODO(asset): add a 15-second hero GIF — import repo → run the `button-led` scenario → press the virtual button → LED lights up with the live waveform. A workbench screenshot works too. -->

---

## Bridging the Embedded Closed-Loop Divide

AI programming has achieved automated closed-loop velocity in pure software. In embedded systems, the loop remains stubbornly open due to physical hardware in the loop:

```text
Pure software today — CLOSED
  generate → run → test → fix → ↻

Embedded today — OPEN (a human in the middle)
  generate ─▶ [human: flash] ─▶ [human: press keys] ─▶ [human: read scope] ─▶ [human: extract logs] ─▶ ↻

"Automated" bench rigs — STILL OPEN (a fake loop)
  generate → auto-flash → logic analyzer → [human: interpret & decide] ─▶ ↻

WinkMicroOS — CLOSED
  generate → hardware-as-code (device tree + scenario JSON)
           → deterministic run (host / wasm)
           → structured trace + PASS evidence
           → fix → ↻
```

Automating flashing and capturing waveforms automates individual tools, not the engineering loop: physical benches still require manual button presses, wiring rework, and hardware edge-case reproduction. The loop only closes when hardware itself is fully abstracted as code (Hardware-as-Code) — reproducible, seeded, fully scriptable, and free of physical time constraints.

That is the design premise of WinkMicroOS.

## Why WinkMicroOS

- **Where the volume actually is.** Wokwi, QEMU and friends serve ARM/RISC-V dev boards. Most industrial volume ships as 8/16-bit parts (MCS-51/STC, Cmsemicon, Padauk, Holtek, Sonix…) with no SWD/JTAG and no affordable ICE — debugging often reduces to painful blind guesswork. WinkMicroOS simulates them instruction-level, with breakpoints, registers and stack fully inspectable in browsers and headless environments.
- **Same source, everywhere.** One C codebase compiles for `host` (tests), `wasm32` (browser/headless simulator) and `targets/esp32`; unmodified Keil C51 sources ([ADR-0075](./docs/decisions/core/0075-mcs51-production-wasm-target-headless.md)) and unmodified Arduino sketches ([ADR-0035](./docs/decisions/core/0035-arduino-compat-polymorphism-sandbox.md)) run in simulation without a porting layer.
- **Deterministic by construction.** Virtual-clock ticking, seeded PRNG, headless scenario scripts with declared assertions. Failures are deterministically reproducible — eliminating intermittent bugs caused by bench environment variations.
- **Hardware is code.** Board topology lives in JSON (`wink-app.json` + board registry), test stimulus lives in scenario JSON. No breadboard, no jumper wires, CI-friendly.
- **Shareable behavior, not oscilloscope traces.** A run produces a shareable online simulation session — cross-functional teams can experience real device behavior and interaction cadence directly in the browser, leaving static oscilloscope screenshots behind.
- **Deterministic fault injection.** Power dips, sensor dropouts, motor stalls, and thermal runaway: destructive and edge-case faults are injected with zero hardware risk and replayed with microsecond precision.
- **Evidence you can ship on.** Every run emits a structured, replayable PASS/FAIL record with a deterministic trace (`SimTraceSpec`, `traceVersion: 1`) — enabling AI-generated firmware to be vetted through verifiable PASS evidence chains and merged with high confidence.
- **Designed for AI agents.** Every input and output of the loop is text, documented for machine consumption ([AGENTS.md](./AGENTS.md)) — agents can scaffold a driver, run the scenario suite, and read structured failures to self-correct.

## Built for agents, not just humans

Existing simulators are good tools — for a person sitting at a keyboard. An agent needs different properties: everything text-defined, headless by default, deterministic, and producing structured evidence.

| Tool | Great at | Why it doesn't close the agent loop |
|---|---|---|
| Proteus | Circuit-level SPICE simulation, a classroom classic | Desktop, licensed, heavyweight; chip library centered on legacy 51/AVR/PIC; no machine-readable evidence output |
| Wokwi | Web Arduino/ESP32 simulator with excellent UX | Maker & education scope; no industrial test framework; no coverage of the low-cost industrial MCUs that ship by the billion |
| QEMU | Open-source instruction-level / system virtualization | Built for OS-level targets, not MCU microsecond peripheral timing; heavy to put inside a firmware CI loop |
| Renode | Multi-node IoT system simulation (Cortex-M / RISC-V) | Powerful but workflow-heavy; aimed at advanced 32-bit scenarios, not the cost-optimized 8-bit ecosystem |

WinkMicroOS is not an incremental patch on legacy desktop tools. It is native infrastructure designed for a new primary developer: AI Agents. Hardware-as-code, deterministic scenarios, structured evidence — fully text-defined, fully programmatically orchestrable.

> **Engineering Boundaries:** Simulation does not replace physical hardware; it shifts architectural risks leftward. High-frequency electrical characteristics, EMC, fine thermal dynamics, and mechanical wear still require real bench verification — our goal is to compress bench testing into the final sign-off step, rather than an agonizing first iteration.

## End-to-End Walkthrough

### 1 · Describe the hardware

```json
// wink-micro-app/mcs51_button_led/wink-app.json
{
  "app_name": "mcs51_button_led",
  "board": "stc89c52_devboard",
  "mcu": "at89c52",
  "devices": {
    "btn": { "type": "button", "gpio_pin": 26, "active_low": true },
    "led": { "type": "led",    "gpio_pin": 8,  "active_high": false }
  }
}
```

### 2 · Write the logic — unmodified vendor code or WinkMicroOS APIs

Unmodified Keil C51, straight from the vendor's IDE — `sbit`, SFRs and all (build-time transpile emits the simulator artifact; the original file is never edited):

```c
// wink-micro-app/mcs51_button_led/button_led.c  (SPDX: Apache-2.0)
#include <wink_mcu.h>

sbit KEY = P3^2;    /* push button on P3.2 / INT0, active-low */
sbit LED = P1^0;    /* LED on P1.0, low-drive-on */

void main(void) {
    LED = 1;
    while (1) {
        LED = (KEY == 0) ? 0 : 1;
        _nop_();    /* microstep / cooperative yield point */
    }
}
```

WinkMicroOS also provides a modern event-driven style, maintaining strict behavioral parity across host, Wasm, and ESP32:

```c
// wink-micro-app/avoidance_car/app_callbacks.c  (SPDX: Apache-2.0)
static void app_on_event(const wink_event_t *evt)
{
    if (evt->device != &front_radar || evt->type != WINK_EVENT_DISTANCE_READY) {
        return;
    }
    float cm = (float)evt->param / 10.0f;
    neck_servo_set_angle(cm < 20.0f ? 1800 : 900);  /* 0.1° units */
}
```

### 3 · Prove it headlessly

Scenarios are deterministic, self-asserting and run without a browser ([SimTraceSpec](./docs/en/design/04-wasm-simulation/00-README.md)):

```json
// wink-micro-app/mcs51_button_led/unisim-scenarios/button-led.scenario.json (excerpt)
{
  "header": { "accuracyMode": "behavioral", "failurePolicy": "fail-fast",
              "determinism": { "prngSeed": 42 } },
  "steps": [
    { "type": "INPUT_PLUGIN_EVENT", "timeUs": "200ms", "targetPluginId": "btn",
      "action": "SET_PRESSED", "params": { "pressed": true } },
    { "type": "ASSERT_POINT", "timeUs": "800ms", "target": "plugin:led/on", "matcher": true },
    { "type": "ASSERT_POINT", "timeUs": "800ms", "target": "gpio:8", "matcher": 0 }
  ]
}
```

Validate this workflow locally with a single command — without hardware connections or launching a browser:

```console
$ winkcli test                            # host build + full test suite (35 executables as of 2026-07)
[PASS] All tests passed
```

## Supported targets

Board definitions are the hardware SSOT: [`wink-tools/tools/codegen/boards/`](./wink-tools/tools/codegen/boards/README.md).

| Family | Board in registry | Simulation tier | Upstream compatibility |
|---|---|---|---|
| ESP32 (Xtensa) | `esp32_devkitc_v4` | Tier 1 — full Wasm runtime, scheduler & multitasking | Native WinkMicroOS apps (BAL / DAL) |
| MCS-51 / 8051 | `stc89c52_devboard`, `cms8s78xx_devboard` | Tier 2 — instruction interception + virtual SFR gateway | **Unmodified Keil C51** sources (`sbit`, `REGX52.H`, vendor SFRs) |
| AVR | `arduino_uno_r3` | Arduino compatibility layer | **Unmodified Arduino sketches** (`Serial`, `String`, `millis`) |
| Padauk PDK | `padauk_pfs154_devboard` | Tier 3 — 1:1 ISA virtual machine | Vendor PDK sources ([ADR-0064](./docs/decisions/unisim/0064-chip-simulation-four-tier-taxonomy.md)) |

## Architecture

The platform is built around the **"Sim-to-Real Simulation Kernel"**. To allow AI-generated firmware to safely and reliably bridge the virtual-physical divide, WinkMicroOS establishes a deterministic closed loop across both a **macro engineering delivery pipeline** and a **micro tri-stack co-simulation system**.

### 1 · Macro Workflow & Closed-Loop Delivery

Driven by a single source of truth (`wink-app.json`), the unified `winkcli` toolchain orchestrates code generation, layered linting gates, and dual-target compilation. A complete Sim-to-Real loop is achieved by asserting telemetry traces streamed back from both virtual and physical targets:

```mermaid
graph TD
    classDef input fill:#e0f2fe,stroke:#0284c7,stroke-width:1.5px,color:#0369a1;
    classDef tool fill:#fef3c7,stroke:#d97706,stroke-width:1.5px,color:#92400e;
    classDef core fill:#f1f5f9,stroke:#475569,stroke-width:1.5px,color:#1e293b;
    classDef target fill:#fee2e2,stroke:#dc2626,stroke-width:1.5px,color:#b91c1c;
    classDef verify fill:#f0fdf4,stroke:#16a34a,stroke-width:1.5px,color:#15803d;

    Manifest["Application Manifest (SSOT)<br><code>wink-app.json</code> (AI / Low-Code Topology)"]:::input
    CLI["Unified Toolchain <code>winkcli</code><br>• Codegen (gen)  • Lint Gates (lint)  • Sim Assertions (test)  • Build & Flash (build)"]:::tool

    SimKernel["<b>Sim-to-Real Simulation Kernel</b><br>100% Dual-Target C Source ⟷ UniSim Digital Twin<br>(Microsecond Virtual Clock · Behavioral High-Fidelity)"]:::core

    WasmTarget["In-Browser / CI Behavioral Simulation<br>(UniSim Engine + 2D/3D Digital Harness)"]:::target
    RealTarget["Physical MCU Board Deployment<br>(ESP32 · MCS-51 · Arduino · PDK)"]:::target

    TraceCheck["Sim-to-Real Trace Comparison & Calibration<br><b>Golden Trace ⟷ UART Real Trace Regression Loop</b>"]:::verify

    Manifest -->|"Parse Board Topology"| CLI
    CLI -->|"Drive Codegen & Sandbox Setup"| SimKernel

    SimKernel -->|"emcmake wasm Build"| WasmTarget
    SimKernel -->|"Cross-Compiler Toolchain"| RealTarget

    WasmTarget -->|"Virtual Golden Trace"| TraceCheck
    RealTarget -->|"Physical Board UART Trace"| TraceCheck
```

### 2 · Micro Runtime Architecture: Tri-Stack Co-Simulation & Causal Closed-Loop

To balance microsecond execution fidelity with system-engineering clarity, the core runtime is orthogonally decoupled into **three distinct technology stacks**. Each stack is highly cohesive and interlocks via standard abstraction boundaries to form a deterministic closed loop:

```mermaid
graph LR
    classDef fw fill:#dcfce7,stroke:#16a34a,stroke-width:1.5px,color:#15803d;
    classDef unisim fill:#fef3c7,stroke:#d97706,stroke-width:1.5px,color:#92400e;
    classDef bench fill:#f3e8ff,stroke:#9333ea,stroke-width:1.5px,color:#7e22ce;

    FW["<b>1. Embedded Firmware Stack</b><br>(wink-micro-os)<br>──────────────<br>Pure C Control & Business Logic<br>100% Dual-Target"]:::fw

    UniSim["<b>2. Simulation Runtime</b><br>(UniSim Base)<br>──────────────<br>Microsecond VirtualClock · Sandboxes<br>PinArbiter · 5-Channel Bus Proxies"]:::unisim

    Bench["<b>3. Digital Workbench Stack</b><br>(Digital Workbench)<br>──────────────<br>Transducers · Continuous Physics ODEs<br>Deterministic Scenarios · Twin Viewport"]:::bench

    FW <===>|"PAL Platform Bypass / Wasm-Bridge ABI"| UniSim
    UniSim <===>|"Micro-Step Stepping / Signal-to-Physics Transduction"| Bench
```

#### 2.1 Micro Architecture of the Three Stacks

```mermaid
graph LR
    classDef fw fill:#dcfce7,stroke:#16a34a,stroke-width:1.5px,color:#15803d;
    classDef unisim fill:#fef3c7,stroke:#d97706,stroke-width:1.5px,color:#92400e;
    classDef bench fill:#f3e8ff,stroke:#9333ea,stroke-width:1.5px,color:#7e22ce;

    Stack1["<b>① Embedded Firmware Stack (wink-micro-os) · Control Domain</b><br>────────────────────────────────────────────<br><b>App Business Logic</b><br>State Machines · Intent Orchestration<br>↓<br><b>BAL Business Abstraction</b><br>Control Algorithms (PID/Filtering) · Math<br>↓<br><b>DAL Device Abstraction</b><br>Servo · Ultrasonic · OLED Semantic APIs<br>↓<br><b>PAL Platform Abstraction</b><br>GPIO · PWM · I2C · SPI · UART · Timers"]:::fw

    Stack2["<b>② Simulation Runtime (UniSim) · Electrical & Clock Domain</b><br>────────────────────────────────────────────<br><b>Wasm-Bridge ABI</b><br>Cross-Language Calling Gate · Asyncify Suspension<br>↓<br><b>VirtualClock & Scheduler</b><br>Single-Gate Microsecond Advance · Zero Wall-Clock Leak<br>↓<br><b>PinArbiter & 5-Channel Bus Proxies</b><br>GPIO / PWM / I2C / SPI / UART / IRQ<br>↓<br><b>Chip Sandbox & Fault Matrix</b><br>Tier 1~4 Heterogeneous ISA VMs · Fault Injection"]:::unisim

    Stack3["<b>③ Digital Workbench Stack (Digital Workbench) · Physics & UX Domain</b><br>────────────────────────────────────────────<br><b>Digital Twin UX & Scenario Harness</b><br>2D/3D Canvas · scenes.json Deterministic Stimuli<br>↓<br><b>Plant & Environment Physics (Layer 4)</b><br>Thermodynamic ODEs · Kinematics & Raycasting<br>↓<br><b>Peripheral Electromechanics (Layer 3)</b><br>Physics-to-Electrical Transducers · Jitter & Inertia<br>↓<br><b>Closed-Loop Test Harness</b><br>Golden Trace Recording · Sim-to-Real Calibration"]:::bench

    Stack1 <--> Stack2 <--> Stack3
```

#### 2.2 End-to-End Causal Closed-Loop & Sim-to-Real Calibration

The system ties the three stacks together via an unbroken causal chain, eliminating open-loop false animations detached from physical reality:

1. **Actuation Flow (Left ➔ Middle ➔ Right)**: The firmware App outputs control signals through BAL/DAL to PAL ➔ UniSim's PinArbiter and bus routing capture electrical events ➔ Digital Workbench peripheral models transduce levels/PWM into mechanical forces or thermal power, driving continuous differential equation physics steps.
2. **Perception Flow (Right ➔ Middle ➔ Left)**: Environmental physics computes updated continuous states (displacement/temperature/ToF acoustics) ➔ Transducers convert physical states into continuous voltages/resistances ➔ UniSim injects raw ADC codes or edge interrupts ➔ Firmware PAL samples incoming events, driving the state machine closed loop.
3. **Sim-to-Real Calibration**: The exact same firmware source runs alongside UniSim in browsers/CI to produce high-fidelity `Golden Traces`, or flashes directly to real MCU hardware (ESP32/MCS-51) streaming back `UART Traces`. Automatic trace diffing calibrates plant physics parameters, ensuring digital twins continuously converge toward physical reality.

### 3 · Core Architectural Pillars

1. **Unified SSOT Driving Engine**: A single source of truth, `wink-app.json`, defines hardware topology and configuration. `winkcli` orchestrates C code generation, architectural linting, headless testing, and firmware builds.
2. **Sim-to-Real Simulation Kernel**:
   Coordinated across three cohesive technology stacks:
   * **Embedded Firmware Stack** (`App ➔ BAL ➔ DAL ➔ PAL`): 100% dual-target C code, compile-time static dispatch, zero runtime dynamic allocation, deterministic timing control.
   * **Simulation Runtime** (`ABI ➔ VirtualClock ➔ Bus Channels ➔ Chip Sandbox`): Microsecond deterministic electrical base, zero wall-clock dependency, zero business physics in kernel.
   * **Digital Workbench Stack** (`Twin Viewport ➔ Plant Physics ➔ Electromechanical Transducers`): Continuous physics ODE solving and automated testbench, eliminating open-loop false animations.
3. **Closed-Loop Dual Delivery (Sim-to-Real)**: A single codebase can be compiled into WebAssembly for zero-hardware interactive testing and fault injection in the browser/CI, or flashed unmodified onto real MCU hardware, with UART trace streaming back for automated model calibration.

## Repository layout

| Path | What it is | License |
|---|---|---|
| [`wink-micro-os/`](./wink-micro-os/) | The C runtime: PAL / DAL / BAL, runtime, trace, targets (`host`, `wasm`, `esp32`), MCS-51 framework, host test suite | **LGPL-3.0-only** (links into your firmware — it can stay proprietary) |
| [`wink-micro-app/`](./wink-micro-app/) | Example & regression apps (MCS-51, PDK, Arduino, ESP32), each with device tree, scenarios and prebuilt Wasm | Apache-2.0 |
| [`wink-firmware-carriers/`](./wink-firmware-carriers/) | Flashable carrier projects (ESP-IDF) for real boards | LGPL-3.0-only |
| [`wink-tools/tools/codegen/boards/`](./wink-tools/tools/codegen/boards/) | Board registry — the hardware single source of truth consumed by the WinkCli toolchain | Apache-2.0 |
| [`wink-plugin-peripherals/`](./wink-plugin-peripherals/) | Simulator peripheral plugins (TypeScript): ultrasonic, WS2812, … | GPL-3.0-only |
| [`docs/`](./docs/README.md) | Bilingual SSOT design specs, ADRs, tech designs, plans, reviews | GPL-3.0-only |

## Design principles

| Rule | Why | Reference |
|---|---|---|
| Negative error codes: `0` = OK, `< 0` = error | Uniform failure handling across all layers | [ADR-0001](./docs/decisions/core/0001-error-code-sign-convention.md) |
| Compile-time static dispatch (POD + named APIs, no vtables / `container_of`) | Predictable code size & stack on 8-bit MCUs | [ADR-0004](./docs/decisions/core/0004-static-dispatch-vs-runtime-ops.md) |
| Dual-target same source: wasm32 + xtensa from one C codebase | Simulated behavior must equal shipped behavior | [ADR-0002](./docs/decisions/unisim/0002-dual-target-compilation.md) |
| No dynamic allocation; cooperative loop execution | No heap fragmentation, bounded RAM on tiny parts | [ADR-0007](./docs/decisions/core/0007-cooperative-loop-execution-model.md) |
| PWM duty in basis points (`PAL_PWM_DUTY_PCT/PERMILLE`), floats banned for duty | Deterministic, unit-safe actuator control | [ADR-0066](./docs/decisions/core/0066-pwm-basis-points-and-float-deprecation.md) |
| Layering & API shape enforced by YAML rules in CI | Keeps App/BAL/DAL/PAL boundaries honest | [ADR-0043](./docs/decisions/tools/0043-yaml-layer-lint.md) |

## Quick start

> 💡 **Getting Started Guide**: This section provides a condensed walkthrough. For a detailed step-by-step tutorial, scenario demos, and troubleshooting instructions, see the [**5-Minute Getting Started Guide**](./docs/en/design/00-quick-start/01-5min-getting-started.md).

### 1 · Zero install — run a demo in your browser

1. Open the online simulator: **<http://www.wink-ai.com/simulator/index.html>**
2. Clone in your Local and import this repository
3. Run the `button-led` scenario, press the virtual button, watch the LED and the live pin waveform

### 2 · Install WinkCli (one-time)

The build / simulate / flash commands below are driven by **WinkCli**, the WinkMicroOS toolchain. Install it once:

```powershell
# Option 1 - winget (recommended on Windows)
winget install WinkAI.WinkCli

# Option 2 - GitHub Releases (offline / no package manager):
#   download winkcli-v<version>-windows-x86_64.zip from
#   https://github.com/wink-cloud-hub/wink-ai-embedded/releases
#   and add winkcli.exe to your PATH
```

> Full install & environment guide: [`wink-tools/docs/en/00-install.md`](./wink-tools/docs/en/00-install.md).

### 3 · Local build & test — no hardware required

```bash
git clone https://github.com/wink-cloud-hub/wink-ai-embedded.git
cd wink-ai-embedded
winkcli test            # everyday gate: host build + full suite
winkcli test --clean    # clean rebuild when CMake or caches are suspect
```

> The underlying flow is plain CMake + CTest: `cmake -B build-host -DTARGET_PLATFORM=host && cmake --build build-host && ctest --test-dir build-host --output-on-failure`.
> Test tiers, fidelity guarantees and the MSVC second-chain: [`wink-micro-os/TESTING.md`](./wink-micro-os/TESTING.md).

### 4 · Real hardware — flash an ESP32

```powershell
winkcli esp32 --app devkitc_smoke                       # build
winkcli esp32 --app devkitc_smoke -- -p COM3 flash monitor
```

> Requires ESP-IDF v6.x via Espressif IDE Manager. Details: [`wink-firmware-carriers/esp32/README.md`](./wink-firmware-carriers/esp32/README.md).

## Built for AI-in-the-loop development

- **Machine-readable hardware**: device tree and board registry are JSON schemas, not schematic PDFs.
- **Deterministic stimulus**: scenario scripts inject button bounce, timing races, sensor dropouts and fault conditions — seeded and replayable.
- **Structured failures & evidence**: fault codes plus the trace ring buffer (`SimTraceSpec`, `traceVersion: 1`) point directly to root causes, and every run emits a structured, replayable PASS/FAIL evidence chain — completely eliminating physical black-box debugging and hardware damage.
- **Agent guide included**: [`AGENTS.md`](./AGENTS.md) and [`.agents/skills/`](./.agents/skills/) describe the repo's conventions, gates and safe editing rules for coding agents.

<!-- TODO: if/when a public MCP server or agent CLI exists, add a "Connect your agent" snippet here. Do not promise integrations that are not shipped. -->

## Documentation

| Hub | Contents |
|---|---|
| [Global docs hub](./docs/README.md) | Topology, governance, CLI query tools |
| [English docs](./docs/en/README.md) · [简体中文](./docs/zh/README.md) | Bilingual SSOT (design specs 01–07) |
| [Wasm simulation (UniSim)](./docs/en/design/04-wasm-simulation/00-README.md) | Mechanisms, fidelity axes, assurance |
| [Decisions (ADR)](./docs/decisions/) | Architecture decision records, by domain |
| [Implementation plans](./docs/implementation-plans/) · [Reviews](./docs/reviews/) | Execution stream & verification records |

## Roadmap

- Cross-platform winkcli distribution (Linux/macOS binaries) so public CI can build firmware end-to-end
- Wider 8/16-bit MCU coverage (more Padauk / Holtek / Sonix parts) and board-registry growth
- Scenario library expansion: fault injection, bus timing races, long-run soak tests

Current milestones and scope: [`docs/en/design/01-system-overall/02-mvp-roadmap.md`](./docs/en/design/01-system-overall/02-mvp-roadmap.md).

## Contributing

Contributions are welcome. For anything beyond a small fix, open an issue first so we can agree on the design. First-time contributors can start with `good first issue` labels, or start with a board definition or a device driver under `wink-micro-app/`.

When using an AI coding agent (e.g., Claude Code or Antigravity), direct it to review [`AGENTS.md`](./AGENTS.md) first to ensure adherence to repository architecture and quality gates.

<!-- TODO(P2): add CONTRIBUTING.md, CODE_OF_CONDUCT.md, SECURITY.md and issue templates. -->

## License

Layered license map — the runtime is **LGPL-3.0-only** (your firmware may stay proprietary); the platform and docs default to **GPL-3.0-only**:

| Scope | License |
|---|---|
| `wink-micro-os/**` runtime (pal / dal / bal / runtime / trace / osal / targets / frameworks) | LGPL-3.0-only |
| `wink-micro-os/codegen/**` (driver / role descriptions & templates; outputs belong to you) | Apache-2.0 |
| `wink-micro-app/**` samples · `wink-tools/tools/codegen/boards/**` | Apache-2.0 |
| `wink-firmware-carriers/**` | LGPL-3.0-only |
| `wink-tools/**` (otherwise) · `wink-plugin-peripherals/**` · platform & docs | GPL-3.0-only |
| `wink-micro-os/third_party/**` (ArduinoCore-API, Unity) | LGPL-2.1-or-later / MIT |

Single source of truth: [`.github/license-map.json`](./.github/license-map.json), enforced by CI. Third-party attributions: [`wink-micro-os/NOTICE`](./wink-micro-os/NOTICE).
