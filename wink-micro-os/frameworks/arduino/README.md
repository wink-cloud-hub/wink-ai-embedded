# Arduino Compatibility Layer - Development & Porting Guide

This directory contains the Arduino Compatibility Layer for Wink Micro OS. It enables standard Arduino sketches (using `setup()` and `loop()`) to be compiled and executed on top of the Wink cooperative runtime.

---

## Key Development Rules & Gotchas

### 1. Linker Symbol Omission Guard (Weak Symbols in Static Libraries)
* **The Problem:** The default stubs for `setup()` and `loop()` are defined as `weak` symbols inside `src/ArduinoStubs.cpp` so they can be overridden by a user's sketch. However, inside a static library (`.a`), the linker will **not** load an object file containing only `weak` symbols if there are no prior unresolved references to them. This causes the linker to silently discard the user's sketch in `libmain.a` because it already resolved `setup`/`loop` from the stubs.
* **The Solution:**
  * **ESP32 Targets:** In the application's `CMakeLists.txt` (typically `main/CMakeLists.txt`), register the component containing your sketch with `WHOLE_ARCHIVE` to force the linker to load it:
    ```cmake
    idf_component_register(
        SRCS ...
        WHOLE_ARCHIVE
    )
    ```
  * **Host/Wasm Unit Tests:** When linking against `libwink_arduino_compat.a` without whole-archive options, unit tests must define their own mock `setup()` and `loop()` symbols directly in the test source file to resolve references.

### 2. Name Linkage (Name Mangling)
* Since Arduino sketches are compiled as C++ (`.ino` or `.cpp`), ensure all core entry points (`setup`, `loop`, `app_main`) use `extern "C"` linkage so the compiler does not mangle their names. This guarantees they resolve correctly to the C-linkage calls in `Common.cpp`.

### 3. Cooperative Runtime & Non-blocking Design
* **The Problem:** Using blocking delay APIs (e.g., `delay(500)`) in the event loop blocks the cooperative task scheduler, generating `WINK_WARN_TICK_OVERRUN` and `WINK_WARN_WCET_EXCEEDED` warnings as execution time exceeds the tick threshold.
* **The Guideline:** For production firmware built on top of Wink Micro OS, prefer non-blocking timer frameworks (`wink_soft_timer`) over blocking delays to keep CPU utilization fluid and maintain scheduler guarantees.
