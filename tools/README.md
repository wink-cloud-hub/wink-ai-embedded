# tools — Wink SDK Toolchain & Utilities

This directory contains the code generation engines, build orchestrators, static lints, and scripts for Wink Micro OS.

## ── Main CLI Orchestrator: `wink.py` ────────────────────────────────────

The primary tool is [wink.py](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/tools/wink.py), a unified CLI orchestrator that wraps the multiple build systems (CMake, Vite, ESP-IDF) and provides a path-agnostic gateway for application development.

### Configuration (`wink-workspace.json`)

To compile user applications stored in custom folders outside of the SDK monorepo, create a `wink-workspace.json` configuration file at the root of your project directory:

```json
{
  "sdk_dir": "path/to/wink-micro-os-sdk",
  "frontend_dir": "path/to/embedded-frontend",
  "esp32_dir": "path/to/esp32_firmware",
  "scripts_dir": "path/to/scripts"
}
```

The CLI will automatically search for this file in your current working directory. Paths can be absolute or relative.

*Alternatively, you can override paths by setting the following environment variables:*
`WINK_SDK_PATH`, `WINK_FRONTEND_PATH`, `WINK_ESP32_PATH`, `WINK_SCRIPTS_PATH`.

---

## ── Command Reference ──────────────────────────────────────────────────

Execute commands using: `python tools/wink.py <command> [options]`

### 1. `gen` (Code Generator)
Loads `wink-app.json` from the target app directory and runs the Jinja2 codegen engine to produce declarative device trees and C API documentation.
* **Command**: `python tools/wink.py gen --app <app_name_or_path>`
* **Arguments**:
  - `--app`: App name under `samples/` or absolute/relative path to your app directory (default: `oled_dashboard`).
* **Generated Artifacts**:
  - `device_tree.h`/`device_tree.c`: Declarative C static instances.
  - `app_options.cmake`: Active driver flags for CMake pruning.
  - `device_tree_api.md`: HTML/Markdown API specification contract created at `<app_dir>/docs/device_tree_api.md`.

### 2. `build` (Simulator Build)
Compiles the application to compile-and-run on the host machine or builds the WASM target for the web browser.
* **Command**: `python tools/wink.py build <target> --app <app_name_or_path> [--clean]`
* **Arguments**:
  - `<target>`: Choose `host` (native desktop simulator executable) or `wasm` (WebAssembly build for web-app).
  - `--app`: App name or directory path.
  - `--clean` *(host only)*: Deletes the CMake build directory first to perform a clean recompilation.

### 3. `esp32` (Real ESP32 Target Firmware)
Generates the ESP32 source scanning configuration and compiles/flashes firmware to real Espressif chips using ESP-IDF.
* **Command**: `python tools/wink.py esp32 --app <app_name_or_path> [idf_args]`
* **Arguments**:
  - `--app`: App name or directory path (default: `devkitc_smoke`).
  - `[idf_args]`: Direct args forwarded to `idf.py` (default: `build`). Examples: `build`, `flash`, `monitor`, `clean`.
* **Usage**: `python tools/wink.py esp32 --app devkitc_smoke flash monitor`

### 4. `web` (Frontend Developer Dashboard)
Starts the Vue Vite developer web server representing the interactive dashboard of the simulator.
* **Command**: `python tools/wink.py web [--port <number>]`
* **Arguments**:
  - `--port`: The local port the Vite server binds to (default: `5173`).

### 5. `test` (Regression & Unit Tests)
Executes all Python Golden codegen checks and C unit tests on host platform via CTest.
* **Command**: `python tools/wink.py test`

---

## ── Platform Command Matrix ───────────────────────────────────────────

Below is a quick reference command matrix categorized by your target platform:

| Target Platform | Codegen Command | Build / Action Command | Test / Verification Command |
|-----------------|-----------------|------------------------|-----------------------------|
| **Host Simulator (Desktop)** | `python tools/wink.py gen --app <app>` | `python tools/wink.py build host --app <app>` | `python tools/wink.py test` |
| **WASM Simulator (Web Dashboard)** | `python tools/wink.py gen --app <app>` | `python tools/wink.py build wasm --app <app>` <br> `python tools/wink.py web` | *Included in `test` suite* |
| **ESP32 Hardware (Real Device)** | `python tools/wink.py gen --app <app>` | `python tools/wink.py esp32 --app <app> build` <br> `python tools/wink.py esp32 --app <app> flash monitor` | *Manual verification step* |

---

## ── Other Utilities in `tools/` ────────────────────────────────────────

* **[codegen/](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/tools/codegen)**: The Jinja2 templating system and driver database plugins for generating code.
* **Log Format Hardening Lint**: Analyzes calling scopes to assert all `LOG_X` and `pal_log_x` calls pass a compile-time string literal to future-proof tokenized logging.
* **Header Self-Containment Lint**: Asserts that every public header under `pal/include/` and `dal/include/` compiles independently to prevent prerequisite compile errors downstream.
