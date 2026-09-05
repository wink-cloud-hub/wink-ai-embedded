# 06. 构建系统与工具链规范（Build & Toolchain Specification）

本文档定义 WinkMicroOS 的多目标编译与构建契约。

---

## 1. 双 Target 同源编译模型

WinkMicroOS 严格遵循 [ADR-0002](../../decisions/core/0002-dual-target-compilation.md) 确立的“双 target 同源编译”架构：
一份 C 业务逻辑代码，既能编译为 WebAssembly 在浏览器仿真中运行，也能编译为目标微控制器固件（如 ESP32、8051）在真实硬件上执行。

```text
                               ┌───────────────────────────┐
                               │  wink-micro-app (C Code)  │
                               └─────────────┬─────────────┘
                                             │
                      ┌──────────────────────┴──────────────────────┐
                      ▼                                             ▼
          [Wasm 目标构建 (Emscripten)]                  [真机硬件构建 (ESP-IDF/Toolchain)]
                      │                                             │
             wink_simulator.wasm                               firmware.bin
                      │                                             │
             浏览器 UniSim 仿真运行                            ESP32 物理硬件烧录运行
```

---

## 2. CMake 标准构建命令

### 2.1 WebAssembly 仿真产物构建
依赖 Emscripten SDK（`emcmake`）：
```bash
# 配置 Wasm 构建目录
emcmake cmake -B build-wasm -DTARGET_PLATFORM=wasm

# 编译生成 wink_simulator.wasm 与运行时 js
cmake --build build-wasm
```

### 2.2 Host 本地测试与单测构建
依赖本地系统 Clang / GCC：
```bash
# 配置 Host 单元测试构建目录
cmake -B build-host -DTARGET_PLATFORM=host -DWINK_BUILD_TESTS=ON

# 编译并运行测试套件
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

### 2.3 ESP32 物理固件构建
依赖 ESP-IDF 环境：
```bash
# 依据应用入口通过 idf.py 构建
idf.py -B build-esp32 build
```
