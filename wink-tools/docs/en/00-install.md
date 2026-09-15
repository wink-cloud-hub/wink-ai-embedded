<!--
visibility: public
winkcli-version: ">=0.1.0"
i18n-meta
source: wink-tools/docs/zh/00-install.md
translated: 2026-09-15
translator: AI-assisted
sync-status: up-to-date
-->
# 00 · Installation & Environment Setup

`winkcli` builds per target. Prepare the environment for your target before running the corresponding commands.

---

## 1. Install WinkCli

Two install channels are supported:

**Option 1 - winget (recommended on Windows)**

```powershell
winget install WinkAI.WinkCli
```

> The package is being onboarded to winget-pkgs ([PR #434970](https://github.com/microsoft/winget-pkgs/pull/434970)); if `winget` cannot find it yet, use Option 2.

**Option 2 - GitHub Releases (offline / no package manager)**

1. Download `winkcli-v<version>-windows-x86_64.zip` from [Releases](https://github.com/wink-cloud-hub/wink-ai-embedded/releases);
2. Extract it and add the folder containing `winkcli.exe` to `PATH`.

After installing, verify the toolchain is reachable:

```bash
winkcli doctor
```

---

## 2. Per-Target Requirements

### 2.1 Host simulation (`build host` / `test`)

| Tool | Purpose | Notes |
|---|---|---|
| **Python 3** | Runs `winkcli`, codegen and linters | ≥ 3.10 |
| **Jinja2** | Codegen template rendering | `pip install "jinja2>=3.1.4"` |
| **PyYAML** | Rule parsing for `winkcli lint` | `pip install "PyYAML>=6"` |
| **gcc** (MinGW / WinLibs on Windows) | Host builds & unit tests | Must be on `PATH`, or bind via `winkcli setup --set gcc=...` |
| **cmake** | Build system | ≥ 3.15 |
| **make / ninja** | Build backend | MinGW Makefiles or Ninja on Windows |
| **ctest** | Test runner | Ships with CMake |

Verify:

```bash
python --version
gcc --version
cmake --version
winkcli doctor
```

### 2.2 Wasm simulation (`build wasm` / `sim run`)

| Tool | Purpose | Notes |
|---|---|---|
| **Emscripten SDK (emsdk)** | Provides `emcmake` / `emcc` | ≥ 3.1.50, must be activated |

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest

# Bind emsdk to winkcli
winkcli setup --set emsdk=/path/to/emsdk
```

### 2.3 ESP32 hardware (`esp32`)

| Tool | Purpose | Notes |
|---|---|---|
| **ESP-IDF v6.x** | ESP32 official toolchain | Install via Espressif IDE Manager (EIM); `winkcli` never auto-installs it |

Run `winkcli doctor` afterwards and confirm the `idf` row shows ✓.

---

## 3. Path Configuration (optional)

Usually unnecessary inside the monorepo. Bind paths explicitly when:

- consuming an unpacked SDK tarball or a split directory layout;
- tools are not on `PATH`;
- multiple toolchain versions coexist and you need to pin one.

```bash
# User-level config (~/.wink/tools.json)
winkcli setup --set gcc=C:/toolchains/mingw64/bin

# Workspace-level config (<workspace>/.wink/tools.json)
winkcli setup --set gcc=D:/toolchains/mingw64/bin --workspace
```

A workspace descriptor `wink-workspace.json` (`sdk_dir` / `frontend_dir` / `esp32_dir`) can be used instead of environment variables.

---

## 4. Command ↔ Minimal Environment

| Command | Minimal environment |
|---|---|
| `doctor` | Python |
| `setup` | Python |
| `gen app-schema` | Python + Jinja2 |
| `build host` / `test` (host part) | Python + Jinja2 + gcc + cmake + make/ninja |
| `build wasm` / `sim run` | Host base + activated emsdk |
| `esp32` | ESP-IDF v6.x (user installed) |
| `build unisim-plugin` / `dev unisim-plugin` | Node / npm + `embedded-frontend` |
| `lint` | Python + PyYAML |

> When dependencies are missing, `winkcli` aborts before execution and prints a collect-all diagnostic report (what is missing, how to install it, how to configure it). See [02 · Toolchain Setup](./02-toolchain-setup.md).
