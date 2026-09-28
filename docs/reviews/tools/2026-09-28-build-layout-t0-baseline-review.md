# T0 基线盘点与载体受控文件审计报告

| 字段 | 内容 |
|---|---|
| 报告编号 | REV-20260928-BUILD-LAYOUT-T0 |
| 关联计划 | [构建输出归位与 CMake 缓存隔离实施计划](../../implementation-plans/tools/2026-09-28-build-output-layout-and-cache-isolation-plan.md) |
| 审计时间 | 2026-09-28 21:13:00 (UTC+8) |
| 审计范围 | `wink-ai-embedded` 与 `wink-ai` 双仓 Git 状态、根目录 `build*` 构建树分布、载体 `sdkconfig` 状态与 `generate_app_sources.py` 调用链路 |
| 审计结论 | **基线已完全冻结并归档；载体受控文件治理策略明确；准入 Phase 1 实施条件达成** |

---

## 1. 双仓 Git 状态与版本基线

| 仓库路径 | 当前分支 | HEAD Commit SHA | 工作区状态 |
|---|---|---|---|
| `wink-ai-embedded` | `master` | `2de63d745cfde5c8207cd1d0e90fac9daef14136` | 干净；仅有新起草的实施计划文件（未跟踪） |
| `wink-ai` | `master` | `36eb90fff16120f42f69fbde491739092a26f2e7` | 干净；`packages/wink-tools` 处于稳态，测试全绿 |

- **基线测试**：`packages/wink-tools/tools/tests/test_esp32_generate_app_sources.py` 运行通过（15 passed in 0.37s）。

---

## 2. 根目录 `build*` 散落目录盘点（15 棵构建树，共 1.21 GB）

通过自动化脚本对 `wink-ai-embedded` 根目录下的所有 `build*` 目录进行盘点：

| 目录名称 | 磁盘占用 (MB) | 是否含 CMakeCache | CMake 生成器 | 工程/目标标识 | 处置策略 |
|---|:---:|:---:|---|---|---|
| `build/` | 720.91 | 是 | Visual Studio 17 2022 | `wink-micro-os` | 内部叶子剥离，退役根 Cache，转为纯容器 |
| `build-host/` | 70.64 | 是 | MinGW Makefiles | `wink-micro-os-top` | Phase 4 最终物理清理 |
| `build_host/` | 82.66 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_idf/` | 90.98 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_idf_wasm_ctor/` | 4.42 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_hardening_standard/` | 109.99 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_hardening_msvc/` | 31.91 | 是 | Visual Studio 18 2026 | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_hardening_asan/` | 23.42 | 是 | Visual Studio 18 2026 | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_hardening_asan_mingw/`| 0.47 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_hardening_esp32c3/` | 15.30 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_hardening_esp32c6/` | 15.30 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_hardening_esp32s3/` | 15.30 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_hardening_lite/` | 15.83 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_hardening_pro/` | 15.81 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| `build_esp_hardening_invalid/` | 5.09 | 是 | MinGW Makefiles | `wink-micro-os` | Phase 4 最终物理清理 |
| **总计** | **1214.03 MB (~1.21 GB)** | - | - | - | - |

### 根 `build/` 容器内嵌套混乱事实证据：
根目录 `build/` 既包含 Visual Studio 解决方案级构件（`ALL_BUILD.dir`、`ZERO_CHECK.dir`、`x64`），又同时作为独立 CMake 构建的父目录（`build/test/`、`build/test-asan/`、`build/wasm/`、`build/wasm-unisim-smoke/`、`build/h3-linux-host/`）。父工程与子工程共用根路径，一旦误执行全量清理将直接连带损毁独立子测试构建树。

---

## 3. 载体 `sdkconfig` 与受控文件深度审计

### 3.1 受控事实确认
- 文件路径：`wink-firmware-carriers/esp32/sdkconfig`
- Git 跟踪状态：`100644 ba889b193246fa4e99891030d538dafd1b026e4a 0 wink-firmware-carriers/esp32/sdkconfig`
- 引入历史：Commit `f89d1465 refactor(carriers): migrate esp32_firmware to wink-firmware-carriers/esp32` 搬迁时连同当时构建产生的中间件全量提交入库。
- 载体现状：`wink-firmware-carriers/esp32/.gitignore` 中仅忽略了 `build/`、`sdkconfig.old` 与 `main/app_sources.cmake`，**未忽略 `sdkconfig`**。

### 3.2 与 `sdkconfig.defaults` 的配置差异比对
- `sdkconfig.defaults` 显式条目数：14 条
- `sdkconfig` 展开条目数：1453 条
- **关键差异项比对**：

| 配置项 | `sdkconfig.defaults` | `sdkconfig` | 根因与处置策略 |
|---|---|---|---|
| `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` | `4096` | `2304` | defaults 显式提升栈大小以防止事件溢出，应**以 defaults 为准（4096）** |
| `CONFIG_FREERTOS_CHECK_STACKOVERFLOW` | `2` | 未显式列出（展开为 canary 机制） | defaults 显式启用深度栈溢出检查，保留 |
| `CONFIG_ESPTOOLPY_FLASHSIZE` | 未声明（走默认） | `"2MB"` / `CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y` | 载体针对 DevKitC 标配 2MB/4MB，建议在 defaults 中明确声明 `CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y` 或保留当前通用大小 |
| `CONFIG_MONITOR_BAUD` / `CONSOLE_BAUD` | 未声明（走默认） | `115200` | 建议在 defaults 中明确声明 `CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200` |
| `CONFIG_COMPILER_OPTIMIZATION` | 未声明 | `CONFIG_COMPILER_OPTIMIZATION_DEFAULT=y` | 正常调试默认值，无需硬编码 |

### 3.3 固化建议清单（合并至 `sdkconfig.defaults`）
在 Phase 1 阶段，向 `wink-firmware-carriers/esp32/sdkconfig.defaults` 补全以下关键底线配置，即可确保脱离原有 `sdkconfig` 后生成行为 100% 幂等：

```ini
# UART Console
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_UART_BAUDRATE=115200

# Flash configuration
CONFIG_ESPTOOLPY_FLASHMODE_DIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_40M=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
```

---

## 4. `generate_app_sources.py` 双调用链与路径解析审计

### 4.1 双调用链拓扑
```mermaid
graph TD
    CLI["wink.py esp32 (tools/cli/commands/esp32.py)"] -->|步骤1: run_cmd()| GenScript["tools/esp32/generate_app_sources.py"]
    GenScript -->|当前写死| HardcodedPath["wink-firmware-carriers/esp32/main/app_sources.cmake"]
    CLI -->|步骤2: idf.py build| IDF["ESP-IDF CMake Configure"]
    IDF -->|步骤3: execute_process()| CarrierCMake["wink-firmware-carriers/esp32/main/CMakeLists.txt"]
    CarrierCMake -->|再次调用| GenScript
    CarrierCMake -->|include| HardcodedPath
```

- **缺陷根因**：
  1. `generate_app_sources.py` 内部硬编码 `out_dir = esp32_firmware_dir / "main"`，不支持外部指定输出路径；
  2. CLI 与载体 CMake 各自触发一次，两处均直接污染源码树；
  3. `_format_repo_relative` 使用了 `prefix = "${CMAKE_CURRENT_LIST_DIR}/" + "../" * up_steps`，一旦被 include 的 cmake 片段挪入 `build/` 容器，`${CMAKE_CURRENT_LIST_DIR}` 计算出的相对路径将彻底脱节。

### 4.2 改造规范确认
1. 脚本增设 `--output <path>`：若传参则写入指定路径，未传参维持向后兼容并警告；
2. 路径生成规范：通过注入的 `${WINK_SDK_PATH}` 或规范化绝对路径定位源码，不再依赖动态推导的相对上溯层级；
3. 双调用同步切换至 `${BUILD_DIR}/main/app_sources.cmake`。

---

## 5. 准入与执行建议

1. **已满足 Phase 1 准入条件**：
   - 双仓基线与脏文件清点完毕；
   - 载体 `sdkconfig` 为受控文件的机制与迁移合并清单已明确；
   - 生成脚本的参数与双调用改造设计清晰，单元测试基线全绿。
2. **Phase 1 实施建议操作顺序**：
   - **Step 1**：在 `wink-ai` 工具仓改造 `generate_app_sources.py`，支持 `--output` 并更新单元测试；
   - **Step 2**：在 `wink-ai-embedded` 仓更新 `sdkconfig.defaults`；
   - **Step 3**：改造载体 `CMakeLists.txt` 与 CLI `esp32.py`，完成双调用切换与 `-DSDKCONFIG` 注入；
   - **Step 4**：执行双应用交替编译，验证载体工作区 `git status` 零污染。
