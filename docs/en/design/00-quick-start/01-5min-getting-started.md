# ⚡ 5-Minute Quick Start Guide

<!-- i18n-meta
source: docs/zh/design/00-quick-start/01-5min-getting-started.md
translated: 2026-10-10
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

> This guide is intended for embedded developers, application engineers, and AI Agents. Through a progressive "4-stage funnel", it helps you establish an intuitive understanding of the **WinkMicroOS dual-target (Sim-to-Real) embedded platform** and complete your first closed loop within 5 minutes.

---

## 🧭 Onboarding Paths (30-Second Overview)

WinkMicroOS shatters the traditional embedded development bottleneck of "hardware-in-the-loop dependency, manual joint debugging, and non-reproducible failures". Through **Hardware-as-Code** and a **microsecond-level deterministic virtual clock**, it converges the entire development and verification workflow into an automated, assertable digital laboratory.

Choose the path that best matches your current setup and goals:

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ 🚀 Path A: Zero Install · 1-Min Browser Play (Visual Parity · Intuitive Acceptance, Devices & Waveforms) │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 💻 Path B: Local Hands-On · 3-Min Headless Simulation & Gates (Primary Dev Choice · AI Self-Loop)        │
├────────────────────────────────────────────────────────────────────────────────────────┤
│ 🔌 Path C: Sim-to-Real · 5-Min Physical Deployment ESP32 (Final Delivery · Dual-Target Silicon Parity)   │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 🚀 Path A: Zero-Install · 1-Minute Browser Experience (Visual Parity · Intuitive Acceptance)

Recommended for all first-time explorers of WinkMicroOS. Skip toolchain setup and immediately observe peripheral interactions directly inside your browser sandbox.

### Step 1 · Access the Online Simulation Workbench & Import Suite
Open your modern web browser (Chrome / Edge recommended):  
👉 **[Wink-AI Online Simulator](http://www.wink-ai.com/simulator/index.html)**

Clone the repository locally:
```bash
git clone https://github.com/wink-cloud-hub/wink-ai-embedded.git
```
On the web page, click the **Open Suite Root Directory** button, and select the cloned `wink-ai-embedded` folder to import.

### Step 2 · Select an Out-of-the-Box Demo App
The online workbench comes with classic application suites from [`wink-micro-app`](../../../../wink-micro-app/):
* **Classic Recommendation (8-bit minimal)**: `mcs51/button_led` — The classic 8051 push-button LED driver (unmodified Keil C51 source code).
* **Advanced Recommendation (Smart vehicle)**: `native/avoidance_car` — Smart obstacle avoidance robot (ultrasonic distance sensing + servo steering, full event-driven loop).
* **Vendor Official (Silicon vendor suite)**: `vendor/esp_idfv61/` — Official silicon vendor demos (ESP-IDF official examples).

### Step 3 · Virtual Interaction & Timing Waveform Observation
Once the application starts, you will observe the digital hardware and live state in the workbench view:
1. **Interactive Operation**: Click the virtual tactile push-button (`btn`) on the canvas and observe the virtual LED (`led`) instantly switch to illuminated.
2. **Timing Observation**: Expand the **Logic Analyzer** at the bottom to observe the microsecond-level GPIO pin level transitions driven by the virtual clock.
3. **Trace Log**: Inspect the structured causal event stream adhering to `SimTraceSpec` in the virtual console:
   ```text
   [TRACE] 00:00:00.200000 | GPIO_INPUT | pin: 26 | value: 0 (btn pressed)
   [TRACE] 00:00:00.200050 | GPIO_SET   | pin: 8  | value: 0 (led lit)
   ```

---

## 💻 Path B: Local Hands-On · 3-Minute Headless Simulation & Gates (Primary Dev Choice · Evidence Chain for AI Self-Verification)

If you are a code developer or seeking automated AI Agent workflows, the local CLI toolchain provides headless assertion capabilities completely independent of any UI.

### Step 1 · Clone Repository
```bash
git clone https://github.com/wink-cloud-hub/wink-ai-embedded.git
cd wink-ai-embedded
```

### Step 2 · Install & Verify the Unified CLI `winkcli`
All building, code generation, simulation assertions, and physical flashing are orchestrated by [`winkcli`](../../../../wink-tools/docs/en/01-cli-reference.md):

```powershell
# Option 1 - winget installation (Recommended on Windows)
winget install WinkAI.WinkCli

# Option 2 - GitHub Releases offline archive
# Download and unzip from https://github.com/wink-cloud-hub/wink-ai-embedded/releases and add to PATH
```

> 💡 **Source-Tree Developer Tip**: If `winkcli` is not installed globally, running `python wink.py` at the root of this repository provides the exact same CLI functionality.

Run environment diagnostics to verify system toolchain readiness:
```bash
winkcli doctor
```

### Step 3 · Run Headless Scenario Assertions & Full Test Suite
Execute deterministic tests without connecting any physical MCU:
```bash
# Build host simulation and run the full test suite
winkcli test
```

You can also run microsecond-level headless assertions for a specific application (using [`mcs51/button_led`](../../../../wink-micro-app/mcs51/button_led/) as an example):
```bash
winkcli sim run --app mcs51_button_led --mode headless
```

**Expected Console Output**:
```text
[INFO] Loaded scenario: button-led.scenario.json (Seed: 42, Mode: behavioral)
[PASS] 00:00:00.100000 | ASSERT_POINT: target "plugin:led/on" == false
[PASS] 00:00:00.800000 | ASSERT_POINT: target "plugin:led/on" == true
[PASS] 00:00:00.800000 | ASSERT_POINT: target "gpio:8" == 0
[PASS] 00:00:01.500000 | ASSERT_POINT: target "plugin:led/on" == false
[RESULT] All 5 assertions passed. Trace verified. (virtual time: 1500000 us)
```

---

## 🔌 Path C: Sim-to-Real · 5-Minute Physical Deployment ESP32 (Final Delivery · Dual-Target Silicon Parity)

Once verified in simulation, the exact same business logic can be deployed directly to physical silicon.

### Step 1 · Connect Hardware
Connect your ESP32 development board (e.g. `esp32_devkitc_v4`) to your computer via a Type-C or Micro-USB cable.

### Step 2 · One-Click Build & Flash
Using the official smoke verification fixture [`fixtures/devkitc_smoke`](../../../../wink-micro-app/fixtures/devkitc_smoke/) as an example, compile, flash, and open the serial monitor via pass-through arguments:

```powershell
winkcli esp32 --app devkitc_smoke -- -p COM3 flash monitor
```
*(Note: Replace `COM3` with the actual serial port from your Device Manager)*

### Step 3 · Observe Physical Causal Response
* Press the on-board `BOOT` button; the status LED lights up immediately;
* UART messages in the terminal match the events captured in the Wasm simulator byte-for-byte;
* This confirms **behavioral parity** between the virtual sandbox and real physical hardware.

---

## 🧩 Core Concepts: Hardware-as-Code & Dual Operational Modes

### 1. Hardware-as-Code: Hardware Circuit & Peripheral Topology Represented in JSON
In WinkMicroOS, peripheral connection topologies are no longer scattered across arbitrary C macros or board headers, but declared in a single source of truth: [`wink-app.json`](../../../../wink-micro-app/mcs51/button_led/wink-app.json):

```json
{
  "app_name": "button_led",
  "board": "stc89c52_devboard",
  "mcu": "at89c52",
  "devices": {
    "btn": { "type": "button", "gpio_pin": 26, "active_low": true },
    "led": { "type": "led",    "gpio_pin": 8,  "active_high": false }
  }
}
```

### 2. Two Core Application Modes
The platform supports two distinct, complementary development paradigms (see [`wink-micro-app` Architecture Specification](../../../../wink-micro-app/README.md)):

| Dimension | Mode 1: AI-Native Unified OS (Arduino-like) | Mode 2: Zero-Modification Compatibility (Tier 2/3) |
|---|---|---|
| **Target Scope** | Zero code change across MCUs, semantic AI-friendly APIs | Existing legacy projects with unrestricted libraries/frameworks |
| **Code Pattern** | Uses WinkMicroOS DAL/BAL semantic APIs (Role-Action) | **100% unmodified Keil C51, Arduino, or ESP-IDF source code** |
| **Sim-to-Real** | 100% behavioral parity: OS kernel linked in both Wasm and silicon | Orthogonal decoupling: silicon runs native firmware; Wasm traps SFRs/APIs |
| **Representative Apps** | `native/avoidance_car`, `native/oled_dashboard` | `vendor/esp_idfv61/`, `vendor/cms8s78xx` |

---

## 🛠️ Create & Run Your First Custom App

Scaffold and run a brand new embedded project in 30 seconds using `winkcli`:

```bash
# 1. Scaffold application skeleton
winkcli create app my_first_demo

# 2. Generate device tree and simulation assets
winkcli gen app-schema --app my_first_demo

# 3. Local build verification
winkcli build host --app my_first_demo

# 4. Execute headless simulation
winkcli sim run --app my_first_demo --mode headless
```

---

## ❓ Frequently Asked Questions (FAQ)

### Q1: `winkcli doctor` reports missing gcc or cmake?
* **Answer**: On Windows, download [WinLibs (GCC + MinGW-w64)](https://winlibs.com/) and add its `bin/` directory to `PATH`; alternatively, run `winkcli setup --set gcc="D:/path/to/gcc.exe"` to explicitly bind the compiler path. See the [Toolchain Setup Guide](../../../../wink-tools/docs/en/02-toolchain-setup.md).

### Q2: Clicking the button in the simulator does not turn on the LED?
* **Answer**: Check the pin polarity configuration in `wink-app.json`:
  * If the button pulls low when pressed (connected to GND), set `"active_low": true`;
  * If the LED is driven low to illuminate, ensure the active drive level matches your code logic.

### Q3: Flashing ESP32 fails with "Access Denied" on serial port?
* **Answer**: The serial port is likely occupied by another terminal (e.g. VS Code serial monitor or another debug tool). Close existing connections and retry.

---

## 📖 Advanced Technical Navigation & Specifications

After completing quick start, dive deeper into core architecture documents:

* 🏛️ **System Overview Architecture** ➔ [01-system-overview.md](../01-system-overall/01-system-overview.md)
* ⚙️ **C Kernel Layering & Static Dispatch (PAL/DAL/BAL)** ➔ [02-wink-micro-os Specifications](../02-wink-micro-os/README.md)
* 🌐 **UniSim Simulation Engine & Bridge ABI Contract** ➔ [04-wasm-simulation Specifications](../04-wasm-simulation/00-README.md)
* 🔧 **WinkCli Toolchain Complete Reference** ➔ [wink-tools CLI Reference](../../../../wink-tools/docs/en/01-cli-reference.md)
* 🤖 **AI Agent Guidelines & Architecture Retrieval** ➔ [docs/AGENTS.md](../../../AGENTS.md)
