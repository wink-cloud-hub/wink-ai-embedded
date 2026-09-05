# ⚡ 5-Minute Quick Start Guide

<!-- i18n-meta
source: docs/zh/design/00-quick-start/01-5min-getting-started.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

This guide helps new developers and AI Agents quickly set up their environment, launch the `wink-micro-os` local WebAssembly simulation, and run their first closed-loop demo.

---

## 1. Prerequisites

Before starting, please ensure the following toolchains are installed locally:

* **Node.js**: `>= 18.0.0`
* **Python**: `>= 3.9` (Used for documentation and plan verification scripts)
* **CMake & GCC / Clang**: (Optional, used for local cross-compilation of native MCU C kernel unit tests)
* **Emscripten (emsdk)**: (Optional, used for re-compiling `wink-micro-os` to `wasm32`)

---

## 2. Running Your First Wasm Simulation Demo in 5 Minutes

### Step 1: Install Dependencies & Build
```bash
# Enter workspace
cd wink-ai-embedded

# Run toolchain tests or local build verification
python docs/implementation-plans/scripts/list_plans.py
```

### Step 2: Load Example Manifest (`wink-app.json`)
The platform is driven by a single source of truth configuration file defining peripheral topology. A standard sample manifest can be found under the `examples/` directory:

```json
{
  "schemaVersion": 2,
  "name": "hello_blink",
  "target_board": "esp32_devkitc",
  "tick_ms": 10,
  "devices": [
    {
      "id": "led_1",
      "model": "gpio_led",
      "pin_map": { "pin": 2 }
    }
  ]
}
```

### Step 3: Launch Wasm Simulation & Live Tracing
* Load the exported Wasm module (`wink_micro_os.wasm`) into the frontend workbench.
* Observe the `SimTraceSpecV2` event stream emitted in the Console:
  ```text
  [TRACE] 00:00:00.010000 | GPIO_SET | pin: 2 | value: 1
  [TRACE] 00:00:00.510000 | GPIO_SET | pin: 2 | value: 0
  ```

---

## 3. Common Development Navigation

* **Modifying C Kernel DAL/PAL** ➔ See [02-wink-micro-os Design Specifications](../02-wink-micro-os/README.md)
* **Inspecting Wasm Bridge ABI Contract** ➔ See [04-wasm-simulation Design Specifications](../04-wasm-simulation/00-README.md)
* **AI Retrieval Guidelines** ➔ Please refer to [docs/AGENTS.md](../../AGENTS.md)
