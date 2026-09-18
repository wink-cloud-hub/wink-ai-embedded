<!--
visibility: public
winkcli-version: ">=0.1.0"
-->
# WinkCli — WinkMicroOS Toolchain (`wink-tools`)

`winkcli` is the unified build & simulation CLI for WinkMicroOS: it hides the differences between CMake / Emscripten / ESP-IDF / cross-compilers and gives one consistent entry point for **Host simulation**, **browser Wasm simulation** and **ESP32 hardware**.

- Install (winget): `winget install WinkAI.WinkCli`
- Install (GitHub Releases): download `winkcli-v<version>-windows-x86_64.zip` from [Releases](https://github.com/wink-cloud-hub/wink-ai-embedded/releases) and add `winkcli.exe` to `PATH`
- Online simulator (zero install): <http://www.wink-ai.com/simulator/index.html>
- Full documentation: [`docs/en/`](./docs/en/)
- Release notes: [GitHub Releases](https://github.com/wink-cloud-hub/wink-ai-embedded/releases)

**English** | [简体中文](./README.zh-CN.md)

---

## 60-second quick start

```bash
# 1. Diagnostics: check Python / GCC / CMake / EMSdk / ESP-IDF availability
winkcli doctor

# 2. Pull or bind the SDK suite (wink-ai-embedded)
winkcli setup --init
#    or bind an existing local checkout:
winkcli setup --embedded-dir /path/to/wink-ai-embedded

# 3. Build the Host simulator (includes codegen)
winkcli build host --app oled_dashboard

# 4. Run the full test matrix
winkcli test
```

---

## Command cheat sheet

```bash
# Code & schema generation
winkcli gen app-schema --app <app_name>
winkcli gen wasm-export --input <header> --output <header>

# Build targets
winkcli build host --app <app_name>            # host-native simulation
winkcli build wasm --app <app_name>            # browser Wasm simulation
winkcli build sim --app <app_name>             # simulation assets (device-tree + wasm)
winkcli esp32 --app devkitc_smoke              # ESP32 firmware
winkcli esp32 --app devkitc_smoke -- -p COM3 flash monitor

# Run simulations (headless / headed)
winkcli sim run --app <app_name> --mode headless
winkcli sim run --app <app_name> --scenarios <file> --reporter json

# Environment & config
winkcli doctor
winkcli setup --init
winkcli setup --set gcc=D:/toolchains/mingw64/bin

# Static architecture lint (SARIF for CI)
winkcli lint --changed
winkcli lint --format sarif --output lint.sarif --strict

# SDK packaging
winkcli pack source --out dist/wink-sdk-source
winkcli pack binary --target host --out dist/wink-sdk-host

# Scaffolding
winkcli create app <app_id>
winkcli create dal --category sensor --role temperature_sensor
```

---

## Documentation

| Doc | Content |
|---|---|
| [00 · Installation & Environment](./docs/en/00-install.md) | Toolchain install (TODO), per-target setup, command ↔ environment |
| [01 · CLI Reference](./docs/en/01-cli-reference.md) | Command-group guide + machine-generated full command appendix |
| [02 · Toolchain Setup](./docs/en/02-toolchain-setup.md) | `doctor` / `setup`, config persistence & path binding |
| [03 · Codegen Guide](./docs/en/03-codegen-guide.md) | `wink-app.json` / board descriptors, driver YAML extensions |
| [04 · Lint Guide](./docs/en/04-lint-guide.md) | Rule packs, allowlists, CI / SARIF integration |
| [05 · SDK Packaging](./docs/en/05-sdk-packaging.md) | Source / Binary SDK packaging & constraints |
| [06 · Troubleshooting](./docs/en/06-troubleshooting.md) | Common errors and fixes |
