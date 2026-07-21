# tools — WinkMicroOS SDK Toolchain & Utilities

SDK-owned CLI, device-tree codegen, config generators, and static lints.
Lives under `wink-micro-os/tools/` so peripheral/driver work stays in one tree.

> **Prerequisites & environment setup:** see [preinstall.md](./preinstall.md).
> First step on a new machine is always `python wink-micro-os/tools/wink.py doctor`.

## Layout

| Path | Role |
|------|------|
| `wink.py` | Unified CLI (`gen` / `build` / `esp32` / `web` / `test` / `doctor` / `setup` / `lint`) |
| `pack_sdk_source.py` | Phase 1 Source SDK tarball (`wink-micro-os-sdk-source-v*.tar.gz`) |
| `pack_sdk_binary.py` | Phase 2 Binary SDK tarball (`wink-micro-os-sdk-binary-v*.tar.gz`) |
| `binary_sdk_cmake/` | Consumer-facing CMake entry + smoke test for Binary SDK |
| `codegen/` | Generators: device tree, `wink_config.h`, PT state helpers |
| `lint/` | YAML layer/API engine (`wink lint`, ADR-0043) + legacy script gates |
| `toolchain/` | Toolchain detect / hint / (later) install; drives `doctor` + `setup` and gates every non-diagnostic command via `ensure_for(profile)` (ADR-0029/0030). See `toolchain/tools.json.example` |
| `esp32/` | `activate.py` (harvest IDF env: hot PATH → EIM profile via `powershell.exe` → `export.ps1`/`export.sh` fallback), `build.py` (strip MSYS/MinGW/EMSDK contamination and run `idf.py -C esp32_firmware …` in sanitized env, `PYTHONUTF8=1` enforced), `generate_app_sources.py` (scan `samples/<app>/*.c` → `esp32_firmware/main/app_sources.cmake`, invoked automatically at CMake configure). Entry point: `python -m tools.esp32.build`. |

## Workspace Resolution

Workspace layout (frontend / esp32_firmware / apps) is resolved via:

1. `WINK_SDK_PATH` / `WINK_FRONTEND_PATH` / `WINK_ESP32_PATH` / `WINK_SCRIPTS_PATH` (legacy — no longer required for `esp32`)
2. `wink-workspace.json` (`sdk_dir`, `frontend_dir`, …)
3. Defaults: SDK = this package; siblings = `../embedded-frontend`, `../esp32_firmware`, …

Example `wink-workspace.json`:

```json
{
  "sdk_dir": "path/to/wink-micro-os",
  "frontend_dir": "path/to/embedded-frontend",
  "esp32_dir": "path/to/esp32_firmware"
}
```

## wink.py CLI

```bash
python wink-micro-os/tools/wink.py <command> [options]
```

| Command | Purpose |
|---------|---------|
| `doctor` | Probe every registered toolchain capability; print a collect-all report (no fail-fast) |
| `setup --set KEY=PATH [--workspace]` | Tool caps (`gcc`/`cmake`/…) → `~/.wink/tools.json` (or `<ws>/.wink/tools.json` with `--workspace`). Layout keys (`esp32_dir`/`sdk_dir`/…) → `<ws>/wink-workspace.json` |
| `setup --install CAP` | Phase B hint-only auto-install (ESP-IDF is never auto-installed — ADR-0030) |
| `gen --app <name\|path>` | `wink-app.json` → device_tree + docs |
| `build host\|wasm --app …` | Host or WASM simulator (`build/host/`, `build/wasm/{projectCode}/`) |
| `esp32 --app … [idf args]` | ESP-IDF build / flash / monitor |
| `web [--port N]` | Vite frontend |
| `test` | Codegen golden + host ctest |
| `lint [--pack layering\|api\|arduino] …` | YAML layer/API/Arduino lint (ADR-0043); **skips** toolchain gate |

Every non-diagnostic command first runs `tools.toolchain.ensure_for(<profile>)` and
exits with an actionable report if a required capability is missing. Use
`--skip-toolchain-check` only as an escape hatch (loud stderr warning).
`doctor` / `setup` / `lint` skip the gate.

```powershell
python wink-micro-os/tools/wink.py lint --pack layering --pack api
python wink-micro-os/tools/wink.py lint --pack arduino
# optional: --paths … / --changed / --format text|json|sarif / --output FILE
#           --strict / --today YYYY-MM-DD / --explain RULE_ID
```

---

## SDK Pack & Consume

### Source SDK (Phase 1)

Delivers the original implementation source. Supports all targets (host / wasm / esp32).

**Pack:**

```powershell
python wink-micro-os/tools/pack_sdk_source.py --out-dir wink-micro-os/dist
# → wink-micro-os-sdk-source-v0.1.0.tar.gz
```

**UnPack:**

```powershell
Remove-Item -Recurse -Force "$env:TEMP/wink-sdk" -ErrorAction SilentlyContinue
mkdir "$env:TEMP/wink-sdk" -Force
tar -xzf wink-micro-os/dist/wink-micro-os-sdk-source-v0.1.0.tar.gz -C "$env:TEMP/wink-sdk"
```

**Consume — Host:**

```powershell
$env:WINK_SDK_PATH = "$env:TEMP/wink-sdk/wink-micro-os-sdk-source-v0.1.0"
python "$env:WINK_SDK_PATH/tools/wink.py" build host --app (Resolve-Path wink-micro-app/avoidance_car)
```

**Consume — Wasm:**

```powershell
# Same extraction, same WINK_SDK_PATH
$env:WINK_SDK_PATH = "$env:TEMP/wink-sdk/wink-micro-os-sdk-source-v0.1.0"
python "$env:WINK_SDK_PATH/tools/wink.py" build wasm --app (Resolve-Path wink-micro-app/avoidance_car)
```

**Consume — ESP32:**

```powershell
$env:WINK_SDK_PATH = "$env:TEMP/wink-sdk/wink-micro-os-sdk-source-v0.1.0"
python "$env:WINK_SDK_PATH/tools/wink.py" esp32 --app (Resolve-Path wink-micro-app/avoidance_car) build

# 烧录 + 监视
python wink-micro-os/tools/wink.py esp32 --app (Resolve-Path wink-micro-app/avoidance_car) -- -p COM3 flash monitor

# 只烧录
python wink-micro-os/tools/wink.py esp32 --app (Resolve-Path wink-micro-app/avoidance_car) -- -p COM3 flash

```

> `wink_config.h` is always generated from `$WINK_APP_DIR/wink-app.json` (not a hardcoded monorepo path).

---

### Binary SDK (Phase 2)

Delivers precompiled `libwink_micro_os.a` + public headers only — no implementation source.

#### Pack

**Host only:**

```powershell
python wink-micro-os/tools/pack_sdk_binary.py --out-dir wink-micro-os/dist
# → wink-micro-os-sdk-binary-v0.1.0.tar.gz  (contains libs/host/)
```

**Host + Wasm combined:**

```powershell
python wink-micro-os/tools/pack_sdk_binary.py --targets host,wasm --out-dir wink-micro-os/dist
# → single tarball containing libs/host/ + libs/wasm/
```

**Skip build (use existing build dir):**

```powershell
python wink-micro-os/tools/pack_sdk_binary.py --skip-build --build-dir wink-micro-os/build-pack --out-dir wink-micro-os/dist
```

The pack script:
- Compiles with ABI ceilings (`-DWINK_MAX_SOFT_TIMERS=32 -DPAL_PWM_CHANNELS=16`) and section-split flags (`-ffunction-sections -fdata-sections`)
- Merges all component `.a` + PAL objects into a single `libwink_micro_os.a`
- Copies public headers into `include/` (auto-scans `pal/include`, `runtime/include`, `trace/include`, `dal/include`, `bal/include`; skips `internal/`)
- Writes `SDK_MANIFEST.txt` with `toolchain=`, `cflags=`, `content_sha256=`, per-file hashes

#### Consume — Host BINARY

```powershell
mkdir "$env:TEMP/wink-sdk-bin" -Force
tar -xzf wink-micro-os/dist/wink-micro-os-sdk-binary-v0.1.0.tar.gz -C "$env:TEMP/wink-sdk-bin"
$sdk = "$env:TEMP/wink-sdk-bin/wink-micro-os-sdk-binary-v0.1.0"
$env:WINK_SDK_PATH = $sdk

# Option A: wink.py (auto-detects binary mode from libs/host/)
python "$env:WINK_SDK_PATH/tools/wink.py" build host --app (Resolve-Path wink-micro-app/avoidance_car)

# Option B: explicit --sdk-mode
python "$env:WINK_SDK_PATH/tools/wink.py" build host --sdk-mode binary --app (Resolve-Path wink-micro-app/avoidance_car)

# Option C: pure CMake smoke test
cmake -S $sdk -B $sdk/build-smoke -DTARGET_PLATFORM=host `
  "-DWINK_APP_DIR=$((Resolve-Path wink-micro-app/avoidance_car).Path)"
cmake --build $sdk/build-smoke
ctest --test-dir $sdk/build-smoke -R binary_sdk_smoke --output-on-failure
```

#### Consume — Wasm BINARY

Requires Emscripten SDK activated in the shell.

```powershell
# 1) Unpack Binary SDK (if not done in Host section above)
mkdir "$env:TEMP/wink-sdk-bin" -Force
tar -xzf wink-micro-os/dist/wink-micro-os-sdk-binary-v0.1.0.tar.gz -C "$env:TEMP/wink-sdk-bin"
$sdk = "$env:TEMP/wink-sdk-bin/wink-micro-os-sdk-binary-v0.1.0"
$env:WINK_SDK_PATH = $sdk

# 2) Configure + build (emcmake required)
#    -S must be the SDK root ($sdk), NOT the App directory.
#    -DWINK_APP_DIR must be one quoted argument (PowerShell splits on '=' otherwise).
Remove-Item -Recurse -Force build/wasm/avoidance_car -ErrorAction SilentlyContinue
emcmake cmake -S $sdk -B build/wasm/avoidance_car `
  -DTARGET_PLATFORM=wasm `
  "-DWINK_APP_DIR=$((Resolve-Path wink-micro-app/avoidance_car).Path)"
cmake --build build/wasm/avoidance_car
# → build/wasm/avoidance_car/wink_simulator.js + wink_simulator.wasm

# Easier alternative (output: build/wasm/{projectCode}/):
Remove-Item -Recurse -Force build/wasm/avoidance_car -ErrorAction SilentlyContinue
python "$env:WINK_SDK_PATH/tools/wink.py" build wasm --sdk-mode binary `
  --app (Resolve-Path wink-micro-app/avoidance_car)
```

---

### SDK Mode Detection

| Signal | Mode |
|--------|------|
| SDK root contains `libs/<target>/` or `SDK_MANIFEST.txt` says `mode=binary` | **BINARY** |
| Otherwise (Source tarball / monorepo tree) | **SOURCE** |
| Explicit `--sdk-mode binary` but no `libs/` | `FATAL_ERROR` |
| Explicit `--sdk-mode source` but manifest says `mode=binary` | `FATAL_ERROR` |

ABI version and toolchain matrix: [ADR-0028](../../docs/design/decisions/0028-host-binary-abi-toolchain-contract.md).

---

## Codegen

```bash
python wink-micro-os/tools/wink.py gen --app devkitc_smoke
# or directly:
python wink-micro-os/tools/codegen/app_codegen.py \
    --config wink-micro-app/devkitc_smoke/wink-app.json \
    --out-dir build/generated
```

Add a device type: drop `codegen/drivers/<type>.py` subclassing `DriverBase`.

Golden tests (from workspace root, with SDK on `PYTHONPATH`):

```bash
$env:PYTHONPATH = "wink-micro-os"
python wink-micro-os/tools/codegen/tests/test_golden.py
```

## Lint

### `wink lint` — layer / API gate (ADR-0043)

YAML-driven App/BAL/DAL/PAL boundary + API-shape checks. Rules live in
`tools/lint/rules/*.yaml` (the single source of truth); the engine only loads
and reports, so add or relax a rule by editing YAML, not Python.

**Run it (from workspace root):**

```powershell
# Full scan with the default packs (layering + api)
python wink-micro-os/tools/wink.py lint

# Pick packs explicitly
python wink-micro-os/tools/wink.py lint --pack layering --pack api --pack arduino
```

Exit code `0` = clean (or only allowlisted findings); `1` = an un-allowlisted
`error`. Warnings do not fail unless you pass `--strict`. `lint` skips the
toolchain gate, so it needs nothing but Python.

**Packs** (`--pack`, repeatable; default `layering`+`api`):

| Pack | Checks |
|------|--------|
| `layering` | include boundaries (`BAL-HDR-NO-PAL`, `DAL-HDR-NO-HAL`, …), forbidden header names, math-header purity |
| `api` | `NO-OPS-VTABLE`; `NO-MALLOC-HOTPATH` (warn) on `bal`/`dal`/`runtime` src; public DAL APIs must return `wink_status_t` not `bool` |
| `arduino` | ADR-0035 Arduino C++ bleed into kernel dirs (`pal`/`dal`/`runtime`/…); SSOT in `packs/legacy_arduino.py` |

**Common flags:**

| Flag | Use |
|------|-----|
| `--changed [REV]` | Only lint files from `git diff --name-only [REV]` (default `HEAD`) — fast pre-commit check |
| `--paths A B …` | Lint only the given files |
| `--rule ID` | Report just one rule |
| `--format text\|json\|sarif` | `sarif` for IDE/CI annotations; pair with `--output FILE` |
| `--strict` | Treat warnings as failures |
| `--explain RULE_ID` | Print a rule's rationale, references and active allowlist, then exit 0 |
| `--report-allowlist` | List active / soon-to-expire `allow_paths` exceptions |
| `--today YYYY-MM-DD` | Override "now" for `until` expiry (also `$WINK_LINT_TODAY`) |

**Typical workflows:**

```powershell
# Before committing C changes — lint just what you touched
python wink-micro-os/tools/wink.py lint --changed

# Understand why a rule fired
python wink-micro-os/tools/wink.py lint --explain DAL-HDR-NO-HAL

# Emit SARIF for CI code-scanning
python wink-micro-os/tools/wink.py lint --format sarif --output lint.sarif
```

**Exceptions:** never disable a rule in code. Add an `allow_paths` entry (with a
`reason` and, preferably, an `until` date ≤ 90 days) to the rule in
`tools/lint/rules/*.yaml`. Expiring entries emit `info` at 30 days and `warning`
at 7 days so they don't rot silently.

### Coverage: `wink lint` vs legacy scripts

Not every static gate is YAML-driven yet. Text/AST-scan boundary rules live in
`wink lint`; checks that need a compiler or a built binary stay as scripts (see
ADR-0043 non-goals). Both run in `run-tests.ps1`.

| Gate | Enforced by | Notes |
|------|-------------|-------|
| BAL/DAL include boundaries, header names, math purity | **`wink lint --pack layering`** | ADR-0023 / ADR-0038 / ADR-0043 |
| ops/vtable, hot-path malloc, `bool` DAL API | **`wink lint --pack api`** | `NO-MALLOC` covers `bal`/`dal`/`runtime` src |
| Arduino C++ isolation (source) | **`wink lint --pack arduino`** | SSOT `packs/legacy_arduino.py`; `check_arduino_isolation.py` is a CLI shim |
| `wink_bal` PUBLIC include dirs / `src`↔`include` mirror | CMake (`BAL-INC-2`, `BAL-SRC-1`) | link/layout — stays in `bal/CMakeLists.txt` |
| Header self-containment (compiles standalone) | `lint/check_headers_self_contained.py` | needs `gcc`/`g++` — not text-scannable |
| Log format-string literal gate | `lint/check_log_format_literals.py` | tokenizer-based; YAML pack is future work |
| PT (protothread) variable footguns | `lint/check_pt_variables.py` + CMake (`check_pt_footguns`) | |
| ESP_PLATFORM guard density / ADR-0017 strict-nonblocking elision | inline steps in `run-tests.ps1` | compile-probe based |
| Arduino **binary** symbol audit (ADR-0036/0040) | `lint/check_arduino_symbols.py` | scans compiled binary via `nm` — needs a build dir |

Backlog toward more YAML coverage: header self-containment / log-literal packs,
an App-layer PAL ban, and `--baseline` fingerprint diff (see ADR-0043).

### Legacy script gates

Invoked from `wink-micro-os/run-tests.ps1` and CMake (`check_pt_footguns`):

- `lint/check_pt_variables.py`
- `lint/check_headers_self_contained.py`
- `lint/check_log_format_literals.py`
- `lint/check_arduino_symbols.py` (binary audit; needs a build dir)
