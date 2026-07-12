# tools — WinkMicroOS SDK Toolchain & Utilities

SDK-owned CLI, device-tree codegen, config generators, and static lints.
Lives under `wink-micro-os/tools/` so peripheral/driver work stays in one tree.

## Layout

| Path | Role |
|------|------|
| `wink.py` | Unified CLI (`gen` / `build` / `esp32` / `web` / `test`) |
| `pack_sdk_source.py` | Phase 1 Source SDK tarball (`wink-micro-os-sdk-source-v*.tar.gz`) |
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

### Source SDK pack (Phase 1)

```bash
python wink-micro-os/tools/pack_sdk_source.py --out-dir wink-micro-os/dist
# → wink-micro-os/dist/wink-micro-os-sdk-source-v0.1.0.tar.gz
```

M2 smoke (SDK and App in separate trees):

```powershell
tar -xzf wink-micro-os/dist/wink-micro-os-sdk-source-v0.1.0.tar.gz -C $env:TEMP/wink-sdk
$env:WINK_SDK_PATH = "$env:TEMP/wink-sdk/wink-micro-os-sdk-source-v0.1.0"
python "$env:WINK_SDK_PATH/tools/wink.py" build host --app (Resolve-Path wink-micro-app/avoidance_car)
```

`wink_config.h` is generated from `$WINK_APP_DIR/wink-app.json` (not the monorepo-root `wink-app.json`).

### Binary SDK pack (Phase 2)

```bash
python wink-micro-os/tools/pack_sdk_binary.py --out-dir wink-micro-os/dist
# → wink-micro-os/dist/wink-micro-os-sdk-binary-v0.1.0.tar.gz
```

The binary pack builds the OS with ABI ceiling defines (`-DWINK_MAX_SOFT_TIMERS=32 -DPAL_PWM_CHANNELS=16`) and section-split flags (`-ffunction-sections -fdata-sections`), merges all component `.a` + `pal_host` objects into a single `libwink_micro_os.a`, copies the public header whitelist into `include/`, and writes `SDK_MANIFEST.txt` with toolchain, cflags, and content hash.

M2 BINARY smoke (SDK and App in separate trees):

```powershell
python wink-micro-os/tools/pack_sdk_binary.py --out-dir wink-micro-os/dist
tar -xzf wink-micro-os/dist/wink-micro-os-sdk-binary-v0.1.0.tar.gz -C $env:TEMP/wink-sdk-bin
$sdk = "$env:TEMP/wink-sdk-bin/wink-micro-os-sdk-binary-v0.1.0"
$env:WINK_SDK_PATH = $sdk
cmake -S $sdk -B $sdk/build-smoke -DTARGET_PLATFORM=host `
  -DWINK_APP_DIR=(Resolve-Path wink-micro-app/avoidance_car)
cmake --build $sdk/build-smoke
ctest --test-dir $sdk/build-smoke -R binary_sdk_smoke --output-on-failure
```

Or via `wink.py` with explicit `--sdk-mode`:

```powershell
python "$env:WINK_SDK_PATH/tools/wink.py" build host --sdk-mode binary --app (Resolve-Path wink-micro-app/avoidance_car)
```

ABI version and toolchain matrix: see [ADR-0028](../../docs/design/decisions/0028-host-binary-abi-toolchain-contract.md).

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
