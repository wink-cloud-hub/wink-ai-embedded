# 06. 构建系统与工具链规范（Build & Toolchain Specification）

本文档定义 WinkMicroOS 的多目标编译、目录布局契约与构建系统架构规范。

| 字段 | 内容 |
|---|---|
| 契约版本 | `WINK_BUILD_LAYOUT_VERSION = 2` |
| 关联决策 | [ADR-0002 双目标编译](../../decisions/unisim/0002-dual-target-compilation.md)、[ADR-0079 完整 App ID 与 Wasm 路径](../../decisions/core/0079-micro-app-nested-app-discovery.md) |
| 统一容器 | `build/`（纯容器，严禁根目录包含 `CMakeCache.txt`） |

---

## 1. 双 Target 同源编译模型

WinkMicroOS 严格遵循 [ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md) 确立的“双 target 同源编译”架构：
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

## 2. 统一构建目录契约（Layout Contract v2）

所有编译入口统一归位至工作区根目录的 `build/` 纯容器内。根目录严格禁止直接生成 `CMakeCache.txt`，所有二进制输出均为独立的叶子目录，杜绝不同架构/配置间的缓存踩踏与源码污染。

```text
build/                                      # 容器：严格禁止存在 CMakeCache.txt
├── ide/                                    # IDE 语言服务专用隔离目录
│   ├── host/compile_commands.json          # 稳定的 Host 编译数据库镜像
│   └── esp32/compile_commands.json         # 稳定的 ESP32 编译数据库镜像
├── compile_commands.json                   # 默认活动编译数据库（默认稳定绑定 Host，防索引抖动）
├── wasm/<完整-app-id>/                     # 主应用默认 Wasm 配置（遵循 ADR-0079，零缩写）
├── host/<完整-app-id>/                     # 主应用默认 Host 配置
├── variants/<host|wasm>/[<来源键>/]<app-id>/<配置键>/
├── tests/<host|wasm>/<测试套件>/<配置键>/
├── firmware/esp32/[<来源键>/]<app-id>/<SoC>/<配置键>/
└── artifacts/<任务或场景>/<配置键>/          # 报告、日志与临时包，不作为 CMake binary dir
```

---

## 3. 标准构建与调试命令

### 3.1 WebAssembly 仿真产物构建
默认路径遵循 ADR-0079 规范，保持与 UniSim 引擎及前端逐字节兼容：

- **通过 Wink CLI 驱动（推荐）**：
  ```bash
  python wink-tools/wink.py build wasm --app fixtures/devkitc_smoke
  ```
- **原生 CMake 命令行调用**：
  ```bash
  # 配置输出至隔离叶子 build/wasm/<app-id>
  emcmake cmake -S wink-micro-os -B build/wasm/fixtures/devkitc_smoke -DTARGET_PLATFORM=wasm -DWINK_APP_DIR=wink-micro-app/fixtures/devkitc_smoke
  cmake --build build/wasm/fixtures/devkitc_smoke
  ```

### 3.2 Host 本地测试与单测构建
测试套件构建归位至 `build/tests/host/...`：

- **通过 Wink CLI 驱动（推荐）**：
  ```bash
  python wink-tools/wink.py test --framework ctest
  ```
- **原生 CMake 命令行调用**：
  ```bash
  cmake -S wink-micro-os -B build/tests/host/general -DTARGET_PLATFORM=host -DWINK_BUILD_TESTS=ON
  cmake --build build/tests/host/general
  ctest --test-dir build/tests/host/general --output-on-failure
  ```

### 3.3 ESP32 物理固件构建（无状态源码树）
ESP32 固件载体源码目录绝对只读，源文件清单与 `sdkconfig` 均外部化输出至构建叶子：

- **通过 Wink CLI 驱动（推荐，自动注入隔离参数）**：
  ```bash
  python wink-tools/wink.py esp32 --app fixtures/devkitc_smoke
  ```
- **原生 idf.py 命令行调用**：
  ```bash
  idf.py -C wink-firmware-carriers/esp32 -B build/firmware/esp32/fixtures/devkitc_smoke -DSDKCONFIG=build/firmware/esp32/fixtures/devkitc_smoke/sdkconfig build
  ```

---

## 4. 调试直达与缓存生命周期管理

- **查询构建绝对路径**（便于终端断点调试或 `cd` 跳转）：
  ```bash
  python wink-tools/wink.py path fixtures/devkitc_smoke --target esp32
  ```
- **检查磁盘缓存占用**：
  ```bash
  python wink-tools/wink.py cache status
  ```
- **安全修剪陈旧缓存**（自动遵守 Clean-Lock 协议，遇忙跳过）：
  ```bash
  python wink-tools/wink.py cache prune --older-than 7d
  ```
