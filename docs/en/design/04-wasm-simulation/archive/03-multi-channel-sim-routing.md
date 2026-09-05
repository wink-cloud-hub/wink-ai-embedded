# 4.3 Multi-Channel Simulation Routing & Peripheral Selection Architecture

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/archive/03-multi-channel-sim-routing.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

| Item | Content |
|---|---|
| Status | Living Spec |
| Related ADRs | [ADR-0003 Simulation Fidelity Boundary](../../../decisions/unisim/0003-simulation-fidelity-boundary.md), [ADR-0002 Dual-Target Homology](../../../decisions/unisim/0002-dual-target-compilation.md), [ADR-0040 Arduino Semantic Simulation Gating](../../../decisions/unisim/0040-arduino-semantic-sim-json-gate.md) |
| Related Implementation | `wink-ai/packages/unisim`, `wink-ai/packages/embedded-frontend`, `wink-micro-os/targets/wasm` |
| Fidelity Specification | [05-simulation-consistency-and-fidelity-spec.md](./05-simulation-consistency-and-fidelity-spec.md) |

In embedded WebAssembly simulation, the central tension lies between **high hardware-firmware fidelity** and **browser execution performance**.

This specification defines UniSim's **four-channel + PWM subtype platform bypass routing architecture** and provides decision guidelines for peripheral component selection. Goal: Maintain fluid browser interactions while ensuring App / BAL / DAL execute **homologous driver logic identical to physical hardware**.

> **Scope**: This document covers only **Axis A: Peripheral Physical Sources** within the simulation multi-axis framework. For time base, timers, interrupts, scheduling, and fault gating, refer to [README Multi-Axis Overview](./README.md) and [05](./05-simulation-consistency-and-fidelity-spec.md) / [08](./08-simulation-consistency-checklist.md).

---

## 0. Simulation Fidelity Boundary (Precedes Channel Selection)

Governed by [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md): **Behavioral (causal) high fidelity**; cycle-level and electrical-level fidelity are not promised.

| Fidelity Tier | Guaranteed Scope | Non-Guaranteed Scope | Typical Verification Scenarios |
|---|---|---|---|
| **L1 Logical / Causal** | App/BAL/DAL state machines, error codes, timeout semantics, protocol framing and parsing | — | Obstacle avoidance state machines, OLED refresh logic |
| **L2 Protocol / Signal Semantics** | I2C/UART/SPI **transaction payloads**, GPIO causal edges, PWM **duty cycle semantics**, ADC raw semantics | Bit-level waveforms, wire impedance, VREF drift | SSD1306 framebuffer updates, register read/write slaves |
| **L3 Timing / Electrical Approximation** | Virtual clock pulse width / edge approximations for limited scenarios in `timing` Accuracy Mode | Cycle-accurate timing, preemptive ISR nesting, non-linear analog frontends | HC-SR04 echo pulse width capture (approximate) |

> **Terminology Constraint**: "Homologous" refers to **L1/L2 driver logic homology**, not "simulation substitutes physical hardware timing/electrical validation". Timing and electrical validation still require real hardware.

---

## 1. Core Architectural Design Principles

Traditional simulators flipping pin levels bit-by-bit at microsecond/nanosecond scales (e.g. 115200 baud UART or 400kHz I2C) generate hundreds of thousands of cross-boundary JS$\leftrightarrow$Wasm invocations per second, freezing the browser main thread.

Early prototypes embedded `#ifdef SIMULATION` within DAL drivers to create **business pass-through bypasses** (directly returning centimeters, Celsius, etc.). Engineering practice demonstrated that this severs simulation from real driver paths, **leaving protocol conversions, timeouts, and error recoveries untested in simulation**.

### 1.1 Homology Boundary Contract

```text
┌ App / BAL / DAL (API + Impl) ───── No simulation business specializations; target: zero `#ifdef SIMULATION`
├ PAL / HAL API (Dual-target shared) ─ Stable interface shape; identical declarations shared between Hardware and Wasm
├ PAL Wasm Impl / `wasm_dev_*` ────── Sole legal platform bypass point (simulation-specialized)
└ JS Plugin / ProductWorld ────────── Generates "physical sources" only; never replaces DAL logic
```

### 1.2 Three Iron Rules

1. **Physical Source Substitution Only**  
   Simulation may substitute: Pin logic levels, pulse width edges, bus slave response bytes, ADC raw readings, buffer contents.  
   Simulation must never substitute: DAL unit conversions, CRC/checksums, timeout evaluations, retry loops, and error recovery.

2. **Platform Layer Bypass (PAL Convergence)**  
   All interception and routing sink down to **PAL / HAL** (and Wasm target implementations); embedding business-level shortcuts inside DAL is strictly forbidden.

3. **Fail-Loud Selection (ADR-0040)**  
   New peripherals must map to one of the channels below; if unmappable, adding unauthorized DAL `#ifdef` shortcuts is forbidden—expand PAL abstractions or establish new channel contracts first.

### 1.3 Accuracy Modes and Fidelity Gates

| Mode | Supported Fidelity Claims | Forbidden Verification Scenarios |
|---|---|---|
| `behavioral` | L1 state machines, L2 payload / StateChannel semantics | Edge-triggered IRQs, pulse width captures, debounce timing |
| `timing` | L2 edge causality + limited L3 virtual clock pulse width approximations | Cycle-level / electrical-level claims |

**Gate**: Use cases claiming "high pulse device consistency (e.g. ultrasonic)" must be validated under **`timing`** mode; `behavioral` results cannot serve as evidence for pulse width or interrupt consistency.

---

## 2. Four-Channel Platform Simulation Routing Mechanism

Routing is partitioned by PAL interface characteristics (bypassing **transport cost**, not driver logic):

```text
                             [ Wasm Firmware: Homologous App / BAL / DAL Logic ]
                                                      │
        ┌──────────────────────┬─────────────────────┼─────────────────────┬──────────────────────┐
        ▼                      ▼                     ▼                     ▼                      ▼
 [1. Pin-Level]        [2. Bus Protocol]     [2b. PWM Duty]      [3. Analog Signal]    [4. Buffer Payload]
 pal_gpio_*            pal_i2c/spi/uart_*    pal_pwm_set_duty    pal_adc_read          pal_ws2812_write /
 PinArbiter            I2CBus/SPIBus/UARTBus notifyDutyChange    (raw / voltage)       pal_camera_* / SAB
 Buttons/LED/Echo      OLED/Bus Sensors/UART Servo/Motor Duty    NTC/LDR/Joystick      WS2812/Camera Frames
        │                      │                     │                     │                      │
        └──────────────────────┴─────────────────────┴─────────────────────┴──────────────────────┘
                                                      ▼
                                       [ SimWorker + SimulationPluginHost ]
                                                      │
                                                      ▼
                         [ embedded-frontend: ControlHub / World UI / ProductWorld(3D) ]
```

> **Note**: PWM Duty is implemented alongside bus imports in code, but semantically represents **modulation/timing quantities**, not byte transactions; treat it as **Channel 2b** to avoid confusing it with I2C/SPI.

### 2.1 Channel 1: Pin-Level Channel

| | |
|---|---|
| **Homologous Preservation** | DAL trigger sequences, `pulse_in` / input captures, timeouts, and error handling |
| **Bypass Substitution** | Pin electrical sources: Driving levels and edge timestamps on `PinArbiter` |
| **PAL Anchors** | `pal_gpio_read` / `pal_gpio_write` / `pal_gpio_pulse_in` (or equivalent capture) |

* **Mechanism**: Firmware GPIO read/write maps to **`PinArbiter`** (multi-source arbitration with impedance/floating semantics). Plugins inject via `writePin`; UIs inject via ideal drivers.
* **Pulse Device Target Architecture (Ultrasonic)**:

```text
ProductWorld / ControlHub
  → UltrasonicPlugin (holds distanceCm)
  → Calculates echoUs, injects ECHO rising/falling edges into PinArbiter via VirtualClock
  → C: pal_gpio_write(TRIG) + pal_gpio_pulse_in(ECHO)   ← Homologous measurement path
  → DAL: Pulse width → distance conversion / Timeouts   ← Homologous business path
```

### 2.2 Channel 2: Bus Protocol Channel

| | |
|---|---|
| **Homologous Preservation** | DAL register sequences, command packing, response parsing, error recovery |
| **Bypass Substitution** | Electrical bit timing; delivers **transaction-level payloads** to virtual slaves |
| **PAL Anchors** | `pal_i2c_transfer` / `pal_spi_transfer` / `pal_uart_write|read` |

* **Data Transport**: Performs synchronous `Uint8Array` slicing over Wasm Heap within the same Worker, distributing to plugin parsers via `I2CBus` / `SPIBus` / `UARTBus`.
* **Example**: SSD1306 — C side executes full I2C write buffering; `MonoOledPlugin` parses commands/data and publishes framebuffers.

### 2.3 Channel 2b: PWM Duty Subtype (Modulation Semantic)

| | |
|---|---|
| **Homologous Preservation** | DAL angle/velocity $\rightarrow$ duty conversions, enable and limit logic |
| **Bypass Substitution** | Microsecond-level PWM carrier waveforms and edges |
| **PAL Anchors** | `pal_pwm_set_duty` $\rightarrow$ `notifyDutyChange` $\rightarrow$ Plugin state / 3D joints |

Fidelity defaults to **L2 (duty semantics)**; does not claim carrier-cycle L3 unless explicitly contracted under `timing` mode.

### 2.4 Channel 3: Analog Signal Channel

| | |
|---|---|
| **Homologous Preservation** | DAL calibration, filtering, thresholding, and error codes over raw values |
| **Bypass Substitution** | Voltage / raw ADC sources on ADC channels |
| **PAL Anchors** | `pal_adc_read(channel)` (and symmetric DAC if present) |

Plugin Channel / ControlHub injects **physical sources bound to ADC channels**, not DAL business return values (returning `temperature_c` directly is forbidden).

### 2.5 Channel 4: Buffer Payload Channel

| | |
|---|---|
| **Homologous Preservation** | Application / DAL framebuffer or RGB array processing algorithms |
| **Bypass Substitution** | Non-standard ultra-high-frequency bit timing (e.g. WS2812 0.4μs NRZ codes) or byte-by-byte GPIO toggling |
| **PAL Anchors** | `pal_ws2812_write(buf,len)` / `pal_camera_capture` / `SharedArrayBuffer` streaming |

Must route through **named PAL buffer APIs**, never degrading into direct UI rendering inside DAL `#ifdef` blocks.

---

## 3. Peripheral Simulation Selection Decision Guide

### 3.1 Selection Decision Matrix

Status: `Landed` = End-to-end available; `Partial` = Mechanism exists, path un-converged; `Planned` = Architectural placeholder.

| Peripheral Category | Representative Device | Channel | PAL Anchor | Status | Recommended Accuracy | Homology / Bypass Strategy |
|---|---|---|---|---|---|---|
| Discrete / Indicators | Buttons, LEDs, Relays | **1 Pin** | `pal_gpio_*` / PinArbiter | Landed | behavioral; timing for IRQs | Preserves read/write & IRQs; substitutes pin level sources |
| Pulse Timing Sensors | HC-SR04 | **1 Pin** | `gpio` + `pulse_in`/capture | **Partial** | **timing (Mandatory)** | Preserves capture & conversions; substitutes ECHO edge sources (§5) |
| Bus Displays | SSD1306, etc. | **2 Bus** | `pal_i2c/spi_transfer` | Landed | behavioral | Preserves protocol framing; substitutes bit timing with payloads |
| Bus Sensors | MPU6050, AHT20 | **2 Bus** | Same as above | Partial～Planned | behavioral | Preserves register logic; plugins act as virtual slaves |
| Serial Modules | GPS NMEA, AT Modules | **2 Bus** | `pal_uart_*` | Partial | behavioral | Preserves frame parsing; substitutes electrical waveforms |
| Servo / Motor PWM | SG90, H-Bridge | **2b PWM** | `pal_pwm_set_duty` | Landed (duty) | behavioral; timing for edges | Preserves duty semantics; bypasses carrier edges |
| Analog Sensors | NTC, LDR, Joystick | **3 Analog** | `pal_adc_read` | Planned | behavioral | Preserves calibration/thresholds; substitutes raw sources |
| High-Frequency LEDs | WS2812B | **4 Buffer** | `pal_ws2812_write` | Planned | behavioral | Preserves RGB buffer logic; bypasses NRZ bit timing |
| High-Throughput Media | Camera / I2S | **4 Buffer** | capture / SAB | Planned | behavioral | Preserves algorithmic consumption; substitutes frame feeds |

### 3.2 Selection Decision Tree

```text
                              [ New Peripheral Component ]
                                           │
                        ┌──────────────────┴──────────────────┐
                        ▼ (Yes)                               ▼ (No)
         [ 1. Standard Digital Bus Transactions? ]    [ 2. PWM Duty / Motor Modulation? ]
               (I2C / SPI / UART)                              │
                        │                         ┌────────────┴────────────┐
                        ▼                         ▼ (Yes)                   ▼ (No)
                【Channel 2: Bus】          【Channel 2b: PWM】   [ 3. Pure Analog ADC/DAC? ]
                                                                      │
                                                         ┌────────────┴────────────┐
                                                         ▼ (Yes)                   ▼ (No)
                                                  【Channel 3: Analog】   [ 4. GPIO / Pulse Capture? ]
                                                                            │
                                                               ┌────────────┴────────────┐
                                                               ▼ (Yes)                   ▼ (No)
                                                        【Channel 1: Pin】   [ 5. High-Throughput / Non-standard? ]
                                                                                  │
                                                                     ┌────────────┴────────────┐
                                                                     ▼ (Yes)                   ▼ (No)
                                                              【Channel 4: Buffer】     【Fail-Loud】
                                                                                  Expand PAL or create ADR;
                                                                                  DAL business #ifdef forbidden
```

---

## 4. Plugin Channel Fidelity Boundaries

`js_sim_get_plugin_channel` / ControlHub / `stateChannels` act as the **physical semantic SSOT between plugins and the host**, not DAL business bypass APIs.

| Allowed | Forbidden |
|---|---|
| UI/3D $\rightarrow$ Plugin injects `distanceCm`, voltages, register mirrors | DAL reading business semantic channels **directly** to `return` to application |
| Plugin computes via channel, writing to **Pin / Bus slave / ADC source** | Using channels to bypass `pulse_in` / bus transactions / ADC paths without transition notes |
| Observation planes, Trace, and UI bindings reading channels | Calling channel equivalents inside DAL `#ifdef SIMULATION` blocks |

**Ultrasonic Convergence Target**: Channels feed Plugins only; measurement paths return to Channel 1 edge injection (see §2.1). C-side `wasm_dev_*` shortcuts ("reading cm and converting to μs") are **Deprecated shortcuts** and forbidden in new device templates.

---

## 5. Architectural Status & Fidelity Convergence (SSOT Alignment)

Cross-checked against `packages/unisim`, `packages/embedded-frontend`, and `wink-micro-os/targets/wasm`:

1. **PinArbiter**: GPIO electrical SSOT (supersedes `PinManager` from earlier drafts).
2. **Bus Transport**: Synchronous Heap slicing within the same Worker $\rightarrow$ `I2CBus` / `SPIBus` / `UARTBus`.
3. **OLED**: Scheme-A short routing retired; unified as `js_pal_i2c_transfer` $\rightarrow$ `MonoOledPlugin`.
4. **Legacy Dedicated Imports**: `js_sim_trigger_ultrasonic` / `js_sim_measure_echo_pulse_us` retired; forbidden in new designs.
5. **Trace**: DAL/PAL do not log traces directly; Worker records `pal.transfer` summaries upon `js_pal_*` returns.
6. **ProductWorld / Raycaster**: 3D collisions belong to presentation layer; distances feed Plugins, **never** directly returning from C DAL.

### 5.1 Known Fidelity Gaps (Targeted for Convergence)

| Gap | Current State | Target State | Priority |
|---|---|---|---|
| Ultrasonic Measurement Shortcut | `wasm_dev_ultrasonic_get_pulse_us` queries `distanceCm` and converts cm$\rightarrow\mu\text{s}$ in C | Plugin injects ECHO edges + homologous `pulse_in`; remove C conversion shortcut | P0 |
| Obsolete DAL Comments | `dal_ultrasonic.c` mentions retired `js_sim_trigger/measure` | Align comments with ADR-0003 evolved PAL paths | P1 |
| Channels 3 & 4 | Architectural placeholders | Add PAL APIs + plugins; upgrade status to Landed | P2 |
| UART / SPI UI | Bus models exist in engine, few frontend visual consumers | Add World/Hub bindings per device without altering channel model | P2 |

---

## 6. Fidelity Acceptance Checklist (Self-Check for New Peripherals / Bypasses)

- [ ] DAL / App contains **no** simulation business branches (no `#ifdef SIMULATION` returning physical shortcuts).
- [ ] Bypass anchors reside in **PAL APIs or Wasm PAL implementations**, identifying Channel 1 / 2 / 2b / 3 / 4.
- [ ] Decision matrix populated: Homologous items, bypass items, landing status, and Accuracy Mode.
- [ ] Pulse / edge / timeout use cases reproduce under **`timing`** mode; `behavioral` is not used as timing evidence.
- [ ] Plugin Channels serve only as physical sources or observations; measurement paths trace back to corresponding `js_pal_*`.
- [ ] Unclassifiable devices Fail-Loud (expanding PAL or ADR), with zero unauthorized DAL shortcuts added.

---

## 7. Deprecations & Migration Guide

| Status | Item | Description |
|---|---|---|
| **Deprecated** | DAL business pass-throughs (entire drivers `#ifdef SIMULATION` returning business values) | Breaks homologous paths, creates false test coverage |
| **Deprecated** | Driver-embedded 3D Raycasters / direct `js_sim_get_distance` calls | Presentation layers must not penetrate DAL |
| **Deprecated** | Per-device dedicated `js_sim_trigger_*` / `js_sim_measure_*` ABIs | Standardized to Pin/Bus/ADC/Buffer + Plugin Channels |
| **In Transition** | C `wasm_dev_*` querying `distanceCm` and calculating pulse width locally | Deprecated shortcut; converges to §2.1 edge injection |
| **Evolution Note** | Relative to ADR-0003 Decision 2 original text | "Physical source substitution only" remains valid; anchor sinks from DAL `#ifdef` to **PAL Wasm Impl + Plugins**, with zero simulation macros in DAL |

---

## 8. Related Documentation

* [02-virtual-peripheral-registry.md](./02-virtual-peripheral-registry.md) — Virtual Peripherals & DeviceTree
* [05-simulation-consistency-and-fidelity-spec.md](./05-simulation-consistency-and-fidelity-spec.md) — Consistency & Fidelity Specification
* [ADR-0003](../../../decisions/unisim/0003-simulation-fidelity-boundary.md) — Simulation Fidelity Boundary
* UniSim Implementation References: `packages/unisim/docs/ARCHITECTURE.md`, `sim-observation-layers.md`, `BUS_MODELS.md`
