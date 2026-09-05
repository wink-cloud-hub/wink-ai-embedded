# 06. Build System and Toolchain Specification

This document defines the multi-target compilation and build contracts of WinkMicroOS.

---

## 1. Dual-Target Homologous Compilation Model

WinkMicroOS adheres to the dual-target architecture established in [ADR-0002](../../decisions/core/0002-dual-target-compilation.md):
A single C codebase compiles both to WebAssembly for browser-based simulation and to microcontroller firmware (e.g. ESP32, 8051) for physical execution.

---

## 2. Standard CMake Build Commands

### WebAssembly Simulation Target
```bash
emcmake cmake -B build-wasm -DTARGET_PLATFORM=wasm
cmake --build build-wasm
```

### Host Unit Testing Target
```bash
cmake -B build-host -DTARGET_PLATFORM=host -DWINK_BUILD_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

### ESP32 Physical Target
```bash
idf.py -B build-esp32 build
```
