# README
- en: [English](./README.md)
- zh_CN: [简体中文](./README.zh_CN.md)

# Wink-Micro-OS ESP32 Firmware

Compile business applications under `wink-micro-os/samples/` (or an external `wink-micro-app/<name>/`) into firmware that can be flashed onto ESP32.

> **All build/flash/monitor operations go through Wink CLI.** You do not need to manually activate an ESP-IDF shell, dot-source an EIM profile, or call `idf.py` directly for normal workflows — Wink handles IDF activation, UTF-8 env setup, MSYS/EMSDK stripping, and source regeneration automatically.

---

## 🎯 Core Feature: Zero-Code Modification Firmware

**Business code lives in `wink-micro-os/samples/<AppName>/` (or `wink-micro-app/<name>/`). Switching Apps or adding/removing source files requires NO modifications inside `esp32_firmware/`.**

Source files are scanned automatically by `tools/esp32/generate_app_sources.py`, which CMake invokes at configure time (driven by Wink CLI).

---

## 🚀 Building & Flashing via Wink CLI

Run all commands from the **repository root** (the directory containing `wink-micro-os/` and `esp32_firmware/`).

### Prerequisite
- ESP-IDF v6.x installed via Espressif IDE Manager (EIM) — see [`wink-tools/preinstall.md`](../wink-tools/preinstall.md) §3. Wink never auto-installs IDF (ADR-0030).
- Python 3.10+ on PATH.

### 1. Build firmware
```powershell
# Build the default sample (devkitc_smoke)
python wink-tools/wink.py esp32

# Build a named sample under wink-micro-os/samples/<name>/
python wink-tools/wink.py esp32 --app avoidance_car

# Build an app in the external wink-micro-app directory (or absolute path)
python wink-tools/wink.py esp32 --app wink-micro-app/my_custom_app
python wink-tools/wink.py esp32 --app D:/projects/my_app
```

### 2. Flash + Serial Monitor
```powershell
# Use '--' BEFORE any idf.py args that start with '-' (e.g. -p, -v, -b, -D).
# Subcommands like build/flash/monitor/fullclean/menuconfig don't need '--'.
python wink-tools/wink.py esp32 --app devkitc_smoke -- -p COM3 flash monitor

# Flash without opening the serial monitor
python wink-tools/wink.py esp32 --app devkitc_smoke -- -p COM3 flash

# Attach serial monitor to an already-built firmware
python wink-tools/wink.py esp32 --app devkitc_smoke -- -p COM3 monitor
```

Port names: Windows `COM3`; Linux `/dev/ttyUSB0`; macOS `/dev/tty.usbserial-*` or `/dev/cu.usbmodem*`.

**Exit the serial monitor:** press `Ctrl + ]`.

### 3. Clean build
```powershell
# fullclean removes esp32_firmware/build/ then rebuilds
python wink-tools/wink.py esp32 --app devkitc_smoke fullclean
```

### 4. Pass extra flags (verbose, baud rate, Kconfig)
Place `--` before any `-flag`:
```powershell
# Verbose build output
python wink-tools/wink.py esp32 --app devkitc_smoke -- build -v

# Custom baud rate for flashing
python wink-tools/wink.py esp32 --app devkitc_smoke -- -b 921600 -p COM3 flash

# Interactive menuconfig (run in a terminal that stays open)
python wink-tools/wink.py esp32 --app devkitc_smoke menuconfig
```

---

## 🛠️ When to call idf.py directly

Only for advanced interactive workflows (`idf.py menuconfig`, `idf.py size`, `idf.py gdb`, `idf.py efuse-*`). For direct `idf.py` use you must first activate an IDF shell (source the EIM PowerShell profile or run `export.ps1`/`export.sh`) because Wink will not have done it for you. See [`preinstall.md §3`](../wink-tools/preinstall.md).

Running `idf.py build` directly from an activated shell **still works** — CMake invokes the Python source scanner at configure time — but bypasses Wink's UTF-8/MSYS guards. **Always prefer `wink.py esp32` for routine builds.**

---

## 📜 App source scanner (tools/esp32/generate_app_sources.py)

Normally invoked by Wink CLI or CMake, so you rarely run it by hand. It:

1. Recursively collects all `*.c` under the resolved app directory, excluding host end-to-end tests (`test_*.c`).
2. Optionally adds `wink-micro-os/samples/common/src/*.c` minus BAL-migrated helpers.
3. Writes `esp32_firmware/main/app_sources.cmake` with `WINK_APP_NAME`, `WINK_APP_DIR`, `WINK_APP_SOURCES`, `WINK_APP_COMMON_INCLUDE_DIR` (UTF-8-sig / CRLF so CMake reads it cleanly on Windows).

Manual invocation for debugging (single line):
```powershell
python wink-tools/tools/esp32/generate_app_sources.py --app-dir wink-micro-app/devkitc_smoke --esp32-firmware-dir esp32_firmware
```

---

## 📋 Available Sample Apps

| App Name | Description |
|---|---|
| `devkitc_smoke` | DevKitC Smoke Test (GPIO / PWM / I2C / Dual-core / Watchdog) |
| `avoidance_car` | Obstacle Avoidance Car (Ultrasonic + Servo) |
| `oled_dashboard` | OLED Dashboard (Button + LED + SSD1306) |

---

## 💡 FAQ

### Q: I added a new `.c` source. Do I need to edit CMakeLists.txt?
**No.** The scanner picks it up automatically. Re-run `python wink-tools/wink.py esp32 --app <name>`.

### Q: Chinese comments cause encoding errors?
Wink sets `PYTHONUTF8=1`/`PYTHONIOENCODING=utf-8` automatically; GCC is configured with `-finput-charset=UTF-8`. If you still see mojibake, verify your source files are saved as UTF-8 and your terminal uses a Unicode font.

### Q: When is `fullclean` needed?
- After switching App
- After modifying CMake scripts or Kconfig
- After upgrading ESP-IDF
- On strange link errors or stale-build artifacts
- For daily iteration just run `build esp32`

### Q: Is "IDF_TARGET is not set, guessed 'esp32'" an error?
**No.** Informational only — ESP-IDF auto-detects the chip target from `sdkconfig`.

### Q: `wink env doctor` says IDF is missing?
Run `python wink-tools/wink.py env doctor` and follow the hints; the most common cause is ESP-IDF not installed via EIM. See [`preinstall.md §3`](../wink-tools/preinstall.md).

---

## 📐 Architecture

```
esp32_firmware/
├── CMakeLists.txt              # ESP-IDF project entry, pulls in targets/esp32 via EXTRA_COMPONENT_DIRS
├── sdkconfig                   # ESP-IDF default config (2MB flash)
└── main/
    ├── app_main.c              # FreeRTOS task creation + stack/heap monitoring
    ├── CMakeLists.txt          # Includes auto-generated app_sources.cmake
    └── app_sources.cmake       # ⚙️ Auto-generated at configure time by tools/esp32/generate_app_sources.py; do not edit
```

The build tools live in `../wink-tools/tools/esp32/` — `activate.py` (IDF env harvest), `build.py` (sanitized idf.py runner), `generate_app_sources.py` (source scanner). **Unified entry point: `python wink-tools/wink.py esp32`.** See [`wink-tools/README.md`](../wink-tools/README.md).
