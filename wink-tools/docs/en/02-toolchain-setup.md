<!--
visibility: public
winkcli-version: ">=0.1.0"
i18n-meta
source: wink-tools/docs/zh/02-toolchain-setup.md
translated: 2026-09-15
translator: AI-assisted
sync-status: up-to-date
-->
# 02 · Toolchain Setup

Before running build / generate / test commands, `winkcli` verifies that the required toolchain capabilities are available. This page covers the capability list, diagnostics and configuration.

---

## 1. Capabilities

| Capability | Tool / environment | Targets | Notes |
|---|---|---|---|
| `python` | Python 3.10+ | General | Runs `winkcli` and the internal toolchain |
| `jinja2` | Jinja2 | Codegen | Template rendering engine |
| `gcc` | Native GCC / MinGW / Clang | Host simulation | Compiles host runtime and test units |
| `cmake` | CMake | General builds | Cross-platform build system |
| `make` | Make / Ninja | Host / Wasm | Build backend |
| `emsdk` | Emscripten SDK | Wasm simulation | Compiles Wasm/JS modules |
| `idf` | Espressif ESP-IDF | ESP32 hardware | **Never auto-installed** — install via EIM |
| `node` | Node.js & npm | Web frontend | Runs peripheral plugins and the web viewport |

---

## 2. Gating behavior

For `build` / `gen` / `test` / `esp32` / `sim` commands, `winkcli` first resolves the tool set required by that command:

- all present → run normally;
- anything missing → **abort with a collect-all report** listing every missing item and its fix:

```text
[winkcli] Toolchain gate check failed for profile 'host':
  ✗ gcc: Executable 'gcc' not found in PATH or configured paths
Please install missing tools or run 'winkcli setup --set gcc=<path>'.
```

Escape hatch (prints a warning; not for daily use):

```bash
winkcli build host --skip-toolchain-check
```

---

## 3. Diagnostics (`winkcli doctor`)

```bash
winkcli doctor
```

Example output:

```text
Wink Micro OS Toolchain Doctor
==================================================
  ✓ python     : Python 3.10.11
  ✓ jinja2     : Jinja2 3.1.2 installed
  ✓ gcc        : gcc 13.1.0
  ✓ cmake      : cmake version 3.28.1
  ✓ emsdk      : emcc 3.1.51
  ✗ idf        : IDF_PATH not set and 'idf.py' not in PATH
```

---

## 4. Path binding & persistence (`winkcli setup`)

`winkcli setup --set KEY=PATH` **validates the path first, then persists it** (invalid paths are rejected):

```bash
# User-level config: ~/.wink/tools.json
winkcli setup --set emsdk=C:/emsdk

# Workspace-level config: <workspace>/.wink/tools.json
winkcli setup --set gcc=D:/toolchains/mingw64/bin/gcc.exe --workspace

# Initialize the SDK suite
winkcli setup --init                                    # shallow clone into ~/.wink/sdk/wink-ai-embedded
winkcli setup --embedded-dir /path/to/wink-ai-embedded  # bind an existing local checkout
```

Resolution order (later overrides earlier): environment variables → workspace config → user config → factory detection.

> Environment variables and the `wink-workspace.json` descriptor are documented in [00 · Installation](./00-install.md).
> For encoding, PyYAML and PATH-conflict issues see [06 · Troubleshooting](./06-troubleshooting.md).
