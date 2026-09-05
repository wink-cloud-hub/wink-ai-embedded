# 4.4 Velxio Simulation Technology Migration & Adaptation Analysis

<!-- i18n-meta
source: docs/zh/design/04-wasm-simulation/archive/04-velxio-migration-analysis.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

To accelerate low-code embedded simulation development, we analyze and evaluate reusable architectural patterns from the open-source `velxio` and Wokwi simulation ecosystems. Because Wink-AI utilizes **API/Component-Level WebAssembly Simulation** (DAL+PAL bridging in WinkMicroOS) rather than **Instruction-Level Hardware Emulation** (such as CPU emulators or cloud QEMU), technology elements require careful categorization.

---

## 1. Zero-Overhead Reusable Modules

### 1.1 Wokwi-elements Virtual Component Library
- **Velxio Approach**: Integrates `@wokwi/elements` Web Components to render LEDs, pushbuttons, LCD1602, 7-segment displays, keypads, and potentiometers.
- **Migration Strategy**: **100% Adopted**. In Vue 3, Vite is configured to allow custom elements starting with `wokwi-`:
  ```typescript
  import { defineConfig } from 'vite';
  import vue from '@vitejs/plugin-vue';

  export default defineConfig({
    plugins: [
      vue({
        template: {
          compilerOptions: {
            isCustomElement: (tag) => tag.startsWith('wokwi-')
          }
        }
      })
    ]
  });
  ```

### 1.2 SVG Wire Orthogonal Routing
- **Velxio Approach**: Uses Union-Find data structures for netlist connectivity and computes orthogonal Manhattan paths (`v[N]`, `h[N]`, `*`).
- **Migration Strategy**: **Port Algorithm Directly** into Pinia-driven Vue 3 SVG wire templates.

### 1.3 Virtual I2C / SPI State Machines
- **Velxio Approach**: Implements TypeScript state machines for standard virtual peripherals:
  - `VirtualSSD1306`: Parses I2C framebuffer commands and renders to HTML5 Canvas.
  - `VirtualDS1307` / `DS3231`: Emulates RTC registers.
  - `VirtualPCF8574`: 8-bit I2C GPIO expander.
- **Migration Strategy**: **Reuse Directly** by mounting them on our `I2CBus` engine.

---

## 2. Modules to Re-Architect or Discard

### 2.1 Eliminating CPU Emulators and Cloud QEMU
- **Velxio Approach**: Relies on `avr8js`, `rp2040js`, and backend cloud QEMU processes streaming pin states over WebSockets.
- **Wink-AI Approach**: **Completely Discarded**. User C code is compiled directly to `wasm32` via Emscripten and executed locally within a single client-side Web Worker. This eliminates cloud server costs and network latency.

### 2.2 State Management & UI Modernization
- Replaces React Zustand with Vue 3 native **Pinia**.
- Implements attribute configuration panels via **Element Plus** and `@yo-cloud/yo-ux-vue` `<SchemaForm>`.

---

## 3. Technology Stack Migration Comparison Matrix

| Simulation Module | Velxio Implementation | Wink-AI Implementation | Migration Strategy |
|:---|:---|:---|:---|
| **Canvas Drag & Render** | React + SVG Wires | Vue 3 + SVG Wires | Port orthogonal algorithms; render in Vue templates |
| **Component Visuals** | `@wokwi/elements` | `@wokwi/elements` | **Adopted wholesale** |
| **C/C++ Compilation** | Cloud `arduino-cli` / `IDF` hex/bin | Cloud/Local `emcc` to Wasm | **Re-architected** compilation pipeline |
| **CPU Simulation** | `avr8js` / `rp2040js` / Cloud QEMU | **Web Worker Wasm Engine** | **Native Wasm execution; discarded emulators** |
| **Peripheral Control** | `PartSimulationRegistry` (Pin Hooks) | `WasmPeripheralRegistry` (PAL Bridge) | **Re-architected** to bridge Wasm PAL exports |
| **Netlist Solving** | Union-Find + `ngspice-wasm` | Union-Find Netlist Merging | Preserves netlist merging; **omits SPICE** for behavioral speed |
