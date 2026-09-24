# ESP-IDF 仿真拦截层 (ESP-IDF Simulation Interception Layer)

> **开源许可**：运行时源码（`src/`, `include/`, `chips/`）遵循 **LGPL-3.0-only**；单测与工具（`test/`, `tools/`）遵循 **GPL-3.0-only**。

## 1. 概述与设计原理

本模块提供针对乐鑫官方 ESP-IDF（v5.1+ ~ v6.1+）原生 C-ABI 的行为级高保真仿真拦截层（Axis B）。
旨在使基于乐鑫官方 ESP-IDF 编写的 C 嵌入式业务代码，在无需修改或仅需极少配置适配的前提下，直接在 WinkMicroOS 仿真宿主（Host 原生环境与浏览器 WebAssembly）中编译与运行。

### 核心特性
- **真机零增量**：当定义 `ESP_PLATFORM` 时，CMake 与头文件自动早退，完全不影响真机固件编译与链接体积。
- **纯 C-ABI 垫片**：门面层代码严格采用 C99 编写，下沉对接 Wink PAL（Platform Abstraction Layer）。
- **零运行期堆分配**：运行期禁止 `malloc/free`，资源池走静态预分配。
- **硬件资源契约（ADR-0065）**：门面层严禁调用 `pal_resource_claim()`，硬件资源由底层独占管理。
- **SoC 双 SSOT 仲裁（ADR-0085）**：原生芯片特性与引脚能力严格由 `chips/<target>/` 独立定义。

## 2. 多框架互斥说明 (T-009)

WinkMicroOS 支持多应用运行时接入，关于应用生命周期符号绑定规则如下：
1. **强强互斥**：`esp_idf` 与 `mcs51` 均向宿主导出强符号 `wink_app_get_callbacks()`。两者严禁在同一个应用程序镜像中同时链接。顶层 CMake 已配置强互斥守卫，检测到 `WINK_APP_ESP_IDF` 与 `WINK_APP_MCS51` 同时为真时将抛出 `FATAL_ERROR`。
2. **弱符号让位**：`arduino` 框架导出的是弱符号（weak symbol），当与 `esp_idf` 共存时由链接器自动让位，无需手动隔离。

## 3. 目录拓扑

```
frameworks/esp_idf/
├── CMakeLists.txt              # 框架 CMake 构建配置（真机早退）
├── esp_idf_sources.cmake       # 源码与头文件包含路径 SSOT 清单
├── README.md                   # 架构与开发者指引
├── docs/                       # 架构规范与覆盖矩阵
│   ├── 01-architecture-and-governance-guide.md
│   ├── 02-api-coverage-matrix.md
│   └── 03-include-closure-inventory.md
├── include/                    # 乐鑫原生头文件垫片家族
│   ├── driver/                 # 外设驱动门面（如 gpio.h）
│   ├── esp_private/            # 私有头文件分片
│   ├── freertos/               # FreeRTOS 兼容头文件桩
│   ├── hal/                    # HAL 类型定义（如 gpio_types.h）
│   └── soc/                    # SoC 统一转发
├── chips/                      # 芯片原生能力定义（ADR-0085）
│   └── esp32/                  # 经典 ESP32 SoC 特性
├── src/                        # 门面与桥接实现 (LGPL-3.0-only)
│   ├── core/                   # 错误码、日志、系统
│   ├── drivers/                # 外设驱动实现 (下沉至 PAL)
│   ├── esp_idf_bridge.c        # 复位与系统桥接
│   └── esp_idf_runtime.c       # wink_app 生命周期强符号导出
├── tools/                      # 工具集 (GPL-3.0-only)
│   └── lint/                   # 外部 Lint Pack
└── test/                       # 单元测试与官方语料 (GPL-3.0-only)
    ├── core/                   # 核心单测
    ├── corpus/                 # 原厂官方语料镜像
    └── wasm/                   # wasm compile-only 门禁
```
