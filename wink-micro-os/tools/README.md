# tools — WinkMicroOS SDK Toolchain & Utilities

SDK-owned CLI, device-tree codegen, config generators, and static lints.
Lives under `wink-micro-os/tools/` so peripheral/driver work stays in one tree.

## Layout

| Path | Role |
|------|------|
| `wink.py` | Unified CLI (`gen` / `build` / `esp32` / `web` / `test`) |
| `codegen/` | Generators: device tree, `wink_config.h`, PT state helpers |
| `lint/` | Build/test gates (PT footguns, header self-containment, log fmt) |

## Main CLI: `wink.py`

```bash
python wink-micro-os/tools/wink.py <command> [options]
```

Workspace layout (frontend / esp32_firmware / apps) is resolved via:

1. `WINK_SDK_PATH` / `WINK_FRONTEND_PATH` / `WINK_ESP32_PATH` / `WINK_SCRIPTS_PATH`
2. `wink-workspace.json` (`sdk_dir`, `frontend_dir`, …)
3. Defaults: SDK = this package; siblings = `../embedded-frontend`, `../esp32_firmware`, …

Example `wink-workspace.json` at the workspace root:

```json
{
  "sdk_dir": "path/to/wink-micro-os",
  "frontend_dir": "path/to/embedded-frontend",
  "esp32_dir": "path/to/esp32_firmware",
  "scripts_dir": "path/to/scripts"
}
```

### Commands

| Command | Purpose |
|---------|---------|
| `gen --app <name\|path>` | `wink-app.json` → device_tree + docs |
| `build host\|wasm --app …` | Host or WASM simulator |
| `esp32 --app … [idf args]` | ESP-IDF build / flash / monitor |
| `web [--port N]` | Vite frontend |
| `test` | Codegen golden + host ctest |

### Platform matrix

| Target | Codegen | Build | Test |
|--------|---------|-------|------|
| Host | `…/wink.py gen --app <app>` | `…/wink.py build host --app <app>` | `…/wink.py test` |
| WASM | same | `…/wink.py build wasm --app <app>` then `web` | via `test` |
| ESP32 | same | `…/wink.py esp32 --app <app> build` | manual |

## Codegen

```bash
python wink-micro-os/tools/wink.py gen --app devkitc_smoke
# or
python wink-micro-os/tools/codegen/app_codegen.py \
    --config wink-micro-app/devkitc_smoke/wink-app.json \
    --out-dir build/generated
```

Add a device type: drop `codegen/drivers/<type>.py` subclassing `DriverBase`.

Golden tests (from workspace root, with SDK on `PYTHONPATH`):

```bash
$env:PYTHONPATH = "wink-micro-os"   # PowerShell
python wink-micro-os/tools/codegen/tests/test_golden.py
```

## Lint

Invoked from `wink-micro-os/run-tests.ps1` and CMake (`check_pt_footguns`):

- `lint/check_pt_variables.py`
- `lint/check_headers_self_contained.py`
- `lint/check_log_format_literals.py`
