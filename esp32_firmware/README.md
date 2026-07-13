# README
- en: [English](./README.md)
- zh_CN: [简体中文](./README.zh_CN.md)

# Wink-Micro-OS ESP32 Firmware

Compile business applications under `wink-micro-os/samples/` into firmware that can be flashed onto ESP32.

---

## 🎯 Core Feature: Zero-Code Modification Firmware

**Business code is located in `wink-micro-os/samples/<AppName>/`. When switching Apps or adding/removing source files, not a single line of code in `esp32_firmware/` needs to be modified!**

Automatically scanned and injected into the build by `tools/esp32/generate_app_sources.py` (invoked automatically at CMake configure time).

---

## 📜 App source scanner (tools/esp32/generate_app_sources.py)

### What it does

Automatically scans all `.c` source files under `wink-micro-os/samples/<AppName>/` (excluding host end-to-end test files like `test_*.c` and BAL-only files), and generates the CMake snippet `main/app_sources.cmake` to be automatically included by the build system. CMake invokes it at configure time; you rarely need to run it by hand.

**Completely eliminates:**
- Manually modifying `CMakeLists.txt` paths when switching Apps
- Manually adding new source files to the `SRCS` list
- Hardcoded coupling with the business directory structure

### How to use

**Method 1: Automatic call by CMake (Recommended, completely transparent)**
```powershell
# Simply run build, the script runs automatically during the configure stage
idf.py build
```

**Method 2: Specifying App manually**
```powershell
# Build a specific App (with zero source code changes)
idf.py build -DWINK_APP=devkitc_smoke
idf.py build -DWINK_APP=avoidance_car
idf.py build -DWINK_APP=oled_dashboard
```

**Method 3: Run scanner standalone (for debugging)**
```powershell
# From the repo root, generate for a specific App
python ..\wink-micro-os\tools\esp32\generate_app_sources.py --app-dir ..\wink-micro-app\devkitc_smoke --esp32-firmware-dir .

# Or by name (resolves under wink-micro-os/samples/<name>)
python ..\wink-micro-os\tools\esp32\generate_app_sources.py --app-name avoidance_car --esp32-firmware-dir .
```

### Output Artifact

The script outputs to `main/app_sources.cmake` (**Automatically generated, do not modify manually**):
```cmake
set(WINK_APP_NAME "devkitc_smoke")
set(WINK_APP_DIR ".../wink-micro-os/samples/devkitc_smoke")
set(WINK_APP_SOURCES
    .../app_callbacks.c
    .../board_config.c
    .../device_tree.c
)
```

---

## 🚀 Complete Flashing Workflow

### 1. Build via the Wink CLI (recommended)

From the repo root, run:

```powershell
python wink-micro-os/tools/wink.py esp32 --app <path-to-app> build
```

Wink activates ESP-IDF automatically (hot PATH → EIM profile via `powershell.exe` on Windows → `export.ps1` / `export.sh` fallback). You do **NOT** need to manually dot-source an EIM profile. `PYTHONUTF8=1` / `PYTHONIOENCODING=utf-8` are injected automatically to avoid Chinese-comment mojibake.

For manual `idf.py` invocations (e.g. running `idf.py menuconfig` interactively), activate an EIM profile in your shell first — see [preinstall.md §3](../wink-micro-os/tools/preinstall.md).

### 2. Compile Firmware

```powershell
# (Optional) Completely clean build directory to compile from scratch
python wink-micro-os/tools/wink.py esp32 --app <path-to-app> -- fullclean

# Build (default subcommand)
python wink-micro-os/tools/wink.py esp32 --app <path-to-app> build

# Anything after `--` is forwarded verbatim to idf.py
python wink-micro-os/tools/wink.py esp32 --app avoidance_car -- build -DSOME_EXTRA=1
```

### 3. Flash + Serial Monitor

```powershell
# Replace COM3 with your actual serial port name (Linux/macOS: /dev/ttyUSB0 etc.)
python wink-micro-os/tools/wink.py esp32 --app <path-to-app> -- -p COM3 flash monitor
```

### 4. Exit Monitor

Press shortcut: `Ctrl + ]`

---

## 📋 Available Apps

| App Name | Description |
|---|---|
| `devkitc_smoke` | DevKitC Smoke Test (GPIO / PWM / I2C / Dual-core / Watchdog) |
| `avoidance_car` | Obstacle Avoidance Car (Ultrasonic + Servo) |
| `oled_dashboard` | OLED Dashboard (Button + LED + SSD1306) |

---

## 💡 FAQ

### Q: I added a new `.c` source file. Do I need to modify CMakeLists.txt?
**No.** The script will automatically scan it. Simply run `idf.py build` again.

### Q: Compile error due to encoding of Chinese comments?
We have configured GCC UTF-8 encoding flags (`-finput-charset=UTF-8`) in CMake, so you shouldn't normally encounter this. If you still do, make sure the source files are saved in UTF-8 encoding.

### Q: When do I need to run `fullclean`?
- After switching Apps
- After modifying CMake scripts
- When encountering strange link errors or build issues
- For daily code changes, just run `build` directly, no `fullclean` is needed

### Q: Is "IDF_TARGET is not set, guessed 'esp32'" an error?
**No.** This is informational — ESP-IDF automatically detects the build target as `esp32` from `sdkconfig` and can be safely ignored.

---

## 📐 Architecture Description

```
esp32_firmware/
├── CMakeLists.txt              # ESP-IDF project entry point, pulls in targets/esp32 via EXTRA_COMPONENT_DIRS
├── sdkconfig                   # ESP32 default config (2MB flash)
└── main/
    ├── app_main.c              # FreeRTOS task creation + stack/heap monitoring
    ├── CMakeLists.txt          # Includes the automatically generated app_sources.cmake
    └── app_sources.cmake       # ⚙️ Auto-generated at CMake configure by tools/esp32/generate_app_sources.py, do not modify manually
```

> The build tools live under `../wink-micro-os/tools/esp32/` (`activate.py`, `build.py`, `generate_app_sources.py`); see [wink-micro-os/tools/README.md](../wink-micro-os/tools/README.md).
