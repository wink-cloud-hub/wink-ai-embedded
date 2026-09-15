<!--
visibility: public
winkcli-version: ">=0.1.0"
i18n-meta
source: wink-tools/docs/zh/06-troubleshooting.md
translated: 2026-09-15
translator: AI-assisted
sync-status: up-to-date
-->
# 06 · Troubleshooting

> First step, always: `winkcli doctor`. It lists every missing or broken dependency with a fix hint in one pass.

---

## 1. Garbled characters on Windows consoles

Chinese Windows consoles default to CP936; printing `✓` / `✗` may raise `UnicodeEncodeError` or render mojibake.

- `winkcli` switches the console code page to UTF-8 (65001) automatically;
- if an external script still trips, set explicitly:

```cmd
set PYTHONUTF8=1
chcp 65001
```

## 2. `PyYAML is required`

`winkcli lint` needs PyYAML to parse rule files:

```bash
python -m pip install "PyYAML>=6"
```

## 3. PATH pollution / same-name tool conflicts

With MSYS2 / MinGW / Git Bash / Emscripten / ESP-IDF installed side by side, multiple `gcc` or `make` may exist on `PATH`.

```bash
# Bind the exact tool paths (workspace config)
winkcli setup --set gcc=D:/toolchains/mingw64/bin/gcc.exe --workspace
winkcli setup --set emsdk=C:/emsdk --workspace
```

## 4. `emsdk` reported as inactive

`emcmake` / `emcc` require an activated environment:

```bash
./emsdk install latest
./emsdk activate latest
# Windows PowerShell:
#   .\emsdk_env.ps1
# Linux/macOS:
#   source ./emsdk_env.sh
```

## 5. `idf` not detected

ESP-IDF must be installed via Espressif IDE Manager (EIM) and its environment exported (or `IDF_PATH` set). `winkcli` never installs ESP-IDF automatically.

## 6. Stale absolute paths after moving machines / SDKs

Configs store absolute paths; moving the SDK invalidates them:

```bash
winkcli doctor
winkcli setup --embedded-dir /new/path/to/wink-ai-embedded
winkcli setup --set gcc=/new/path/to/mingw64/bin
```

Full environment checklist: [00 · Installation](./00-install.md). Gating behavior and config precedence: [02 · Toolchain Setup](./02-toolchain-setup.md).
