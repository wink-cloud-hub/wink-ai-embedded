# ESP-IDF v6.1 官方示例仿真适配与测试标准执行手册 (Playbook)

> **版本**：v1.0  
> **适用芯片**：Espressif ESP32 / ESP32-S3 / ESP32-C3 系列  
> **关联清单**：[CHECKLIST.md](CHECKLIST.md)  
> **执行脚本**：[run_esp32_headless_evidence.ps1](../../../wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1)  
> **适用对象**：AI Coding Agents（Antigravity、Claude Code 等）与嵌入式开发工程师。

---

## 零、 核心原则与硬性门禁 (Non-Negotiable Gates)

对于清单 [CHECKLIST.md](CHECKLIST.md) 中的**每一项示例**，在将其状态标记为 `[x]`（已完成）之前，**必须严格执行并通过以下三大硬性门禁**：

1. **真实编译输出仿真资产三件套 (unisim-assets/)**：
   使用 `wink-ai/packages/wink-tools/` 进行端到端真实构建编译，生成并输出资产三件套至微应用目录下的 `unisim-assets/`：
   - `device-tree.json`（由 `wink-app.json` 经 `runtime_device_tree.py` 严格校验生成的拓扑与引脚映射）
   - `wink_simulator.js`（Emscripten Wasm 运行时胶水层）
   - `wink_simulator.wasm`（包含 `wink_framework_esp_idf` 仿真内核、FreeRTOS 调度器与官方应用代码的 Wasm 二进制）
2. **确定性 Headless 自动化测试实证 (unisim-scenarios/)**：
   编写对应的场景脚本 `unisim-scenarios/<name>.scenario.json`，使用 `run_esp32_headless_evidence.ps1` 驱动 UniSim Headless 模式进行自动化测试，确保所有微秒级断言步骤（引脚电平、时钟时序、外设事件）**100% 绿灯通过**。
3. **原厂源码“一行不改”准则**：
   从 ESP-IDF 官方仓库镜像的代码文件（如 `blink_example_main.c`、`ledc_basic_example_main.c`）必须保持原汁原味，上游 SHA-256 哈希值需在 `wink-app.json` 中锁定。Kconfig / `sdkconfig` 宏定义一律在独立的 `include/sdkconfig.h` 中进行私有覆盖，底层行为由 `wink_framework_esp_idf` 门面垫片透明承接。

> [!CAUTION]
> **绝对门禁声明**：
> 任何未在 App 独立目录下产出 `unisim-assets/` 三件套、未编写 `unisim-scenarios/*.scenario.json`、未通过 `run_esp32_headless_evidence.ps1` 无头场景验证的示例，**一律严禁在 Checklist 中标记为 `[x]`！** 仅通过底层 CTest 编译或单元测试不等于应用级仿真交付。

---

## 一、 标准实施闭环工作流 (5 阶段)

```text
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段一：新建 App 与原厂代码镜像 (Setup & Mirror)                        │
│   ├── 创建 wink-micro-app/vendor/esp_idfv61/<feature>/                 │
│   ├── 镜像官方源码（保持一行不改，计算 SHA-256 哈希）                    │
│   ├── 配置 include/sdkconfig.h (Kconfig 语料私有宏)                     │
│   ├── 配置 CMakeLists.txt (导出 WINK_APP_SOURCES 与 WINK_APP_ESP_IDF)   │
│   ├── 配置 wink-app.json (声明 board, mcu, devices 引脚映射)           │
│   └── 编写 unisim-scenarios/<feature>.scenario.json 确定性场景测试脚本 │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段二：使用 wink-tools 进行真实编译构建输出 (Real Build)               │
│   ├── 运行: python packages/wink-tools/wink.py build sim --app <app>   │
│   ├── [1/3] 校验并生成 device-tree.json                                │
│   ├── [2/3] Emscripten 真实编译 Wasm 二进制并链接 wink_framework_esp_idf│
│   └── [3/3] 提取并校验 unisim-assets/ 三件套有效性                     │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段三：使用 run_esp32_headless_evidence.ps1 自动化实证 (Headless Verification) │
│   ├── 运行: powershell run_esp32_headless_evidence.ps1 -App <app>      │
│   ├── 加载 unisim-assets/ 仿真三件套                                   │
│   ├── 执行微秒级时序模拟 (Virtual Time Scheduler)                       │
│   └── 断言微秒级引脚电平与插件状态 (ASSERT_POINT 100% 绿灯)            │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段四：底座 CTest 回归与分层门禁检查 (Unit & Lint Gates)               │
│   ├── 执行 CTest 单元测试验证底层外设模型 (test_esp_idf_*)             │
│   └── 运行分层门禁: winkcli lint --pack layering --pack api            │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段五：核对清单 (Checklist) 回写与归档 (Checklist Sign-off)            │
│   ├── 更新 ESP32_IDFV61_EXAMPLE_CHECKLIST.md 状态为 [x]                │
│   ├── 填入对应 App 目录名与详实验收记录（场景路径、关键断言时序）        │
│   └── 更新总体适配进度计数                                             │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 二、 阶段一：新建 App 与原厂代码镜像 (Setup & Mirror)

### 1. 目录命名与结构规范
在 `wink-micro-app/vendor/esp_idfv61/` 下创建新目录，统一遵循命名规范：
`<feature_lowercase>`（例如 `blink_gpio`、`ledc_basic`）。

标准目录结构如下：
```text
wink-micro-app/vendor/esp_idfv61/<feature>/
├── CMakeLists.txt              # 1. 构建脚本：导出 WINK_APP_SOURCES 与 WINK_APP_ESP_IDF
├── wink-app.json               # 2. 微应用元数据与引脚拓扑配置
├── <feature>_example_main.c    # 3. 官方原始源码（一行不改）
├── include/
│   └── sdkconfig.h             # 4. Kconfig 宏私有覆盖层（必须 include "sdkconfig_base.h"）
├── unisim-assets/              # 5. 仿真构建资产输出目录（由阶段二自动生成/输出）
│   ├── device-tree.json
│   ├── wink_simulator.js
│   └── wink_simulator.wasm
└── unisim-scenarios/           # 6. 确定性自动化场景用例目录
    └── <feature>.scenario.json
```

### 2. `CMakeLists.txt` 规范
每一个 ESP-IDF 示例工程的 `CMakeLists.txt` 必须包含以下标准化配置：
```cmake
# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/<feature>/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_<feature> C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/<feature>_example_main.c
        PARENT_SCOPE
    )
    set(WINK_APP_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PARENT_SCOPE
    )
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idfv61_<feature>: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
```

> **核心机制**：
> - `set(WINK_APP_SOURCES ... PARENT_SCOPE)`：将官方应用源码导出到主 Wasm 仿真可执行程序构建树。
> - `set(WINK_APP_INCLUDE_DIRS ... PARENT_SCOPE)`：将本地 `include/` 加入头文件检索路径，确保优先命中本地私有 `sdkconfig.h`。
> - `set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)`：触发根 CMakeLists.txt 自动链接 `wink_framework_esp_idf`，自动注入 FreeRTOS 调度器与 ESP-IDF 垫片实现。

### 3. `wink-app.json` 配置规范
配置中必须包含 upstream 溯源信息、正确的开发板、MCU 架构和设备引脚映射：
```json
{
  "app_name": "esp_idfv61_<feature>",
  "display_name": "Espressif ESP-IDF <Feature Name>",
  "board": "esp32_devkitc_v4",
  "category": "vendor_example",
  "mcu": "esp32",
  "tick_ms": 1,
  "upstream": {
    "vendor": "Espressif",
    "version": "v6.1",
    "source_dir": "examples/<category>/<feature>/main",
    "files": {
      "<feature>_example_main.c": "<sha256_hash>"
    }
  },
  "devices": {
    "led": {
      "type": "led",
      "gpio_pin": 2,
      "active_high": true
    }
  }
}
```

### 4. `unisim-scenarios/<feature>.scenario.json` 测试用例规范
定义微秒级时序步骤、预期观测电平与断言：
```json
{
  "header": {
    "version": "1.0.0",
    "name": "ESP32 ESP-IDF v6.1 <feature> headless simulation proof",
    "templateId": "esp_idfv61_<feature>",
    "accuracyMode": "behavioral",
    "timeoutUs": "3500000",
    "failurePolicy": "fail-fast",
    "determinism": { "prngSeed": 42 }
  },
  "steps": [
    {
      "type": "ASSERT_POINT",
      "timeUs": "50ms",
      "target": "gpio:2",
      "matcher": 0,
      "description": "[上电初始 50ms] GPIO2 初始输出低电平 0 (LED 熄灭)"
    },
    {
      "type": "ASSERT_POINT",
      "timeUs": "1050ms",
      "target": "gpio:2",
      "matcher": 1,
      "description": "[周期翻转 1050ms] 经过 1000ms FreeRTOS 延时后，GPIO2 翻转为高电平 1 (LED 点亮)"
    }
  ]
}
```

---

## 三、 阶段二：使用 `wink-tools` 进行真实构建编译 (Real Build)

### 1. 构建入口命令
在 `wink-ai` 仓库根目录下，通过 `wink.py build sim` 执行端到端构建：

```bash
# 在 wink-ai/ 目录下执行：
python packages/wink-tools/wink.py build sim --app vendor/esp_idfv61/<feature>
```

> **参数说明**：
> - `--app vendor/esp_idfv61/<feature>`：指定微应用路径或 App ID。
> - `--clean`：（可选）构建前清理构建缓存目录 `build/wasm/vendor/esp_idfv61/<feature>`。
> - `--out <dir>`：（可选）自定义输出目录，默认自动输出到 `<app_dir>/unisim-assets`。

### 2. 内部执行三部曲解析
1. **[1/3] Generating DeviceTree**：
   调用 `tools/frontend/runtime_device_tree.py`，根据 `wink-app.json` 与板级定义（`esp32_devkitc_v4`）生成标准的 `device-tree.json`。
2. **[2/3] Compiling WASM simulator**：
   调用 Emscripten 工具链（`emcmake cmake` 与 `cmake --build`），将 WinkMicroOS 运行时、`wink_framework_esp_idf` 仿真内核及官方业务代码编译链接为 WebAssembly 目标文件。
3. **[3/3] Extracting WASM assets**：
   将编译生成的 `wink_simulator.js` 与 `wink_simulator.wasm` 自动提取并拷贝至目标微应用的 `<app_dir>/unisim-assets/` 目录。

### 3. 产物校验清单
构建完成后，必须检查 `<app_dir>/unisim-assets/` 确保包含以下三件套，且文件大小合法非零：
| 产物文件名 | 期望大小范围 | 说明 |
| :--- | :--- | :--- |
| `device-tree.json` | 300B ~ 2KB | 包含 board, mcu, devices 拓扑与引脚映射 |
| `wink_simulator.js` | 100KB ~ 150KB | Emscripten 生成的 JavaScript 运行时加载胶水层 |
| `wink_simulator.wasm` | 130KB ~ 300KB | 包含完整 ESP-IDF 门面、FreeRTOS 调度器与业务固件的 Wasm 二进制 |

---

## 四、 阶段三：使用 `run_esp32_headless_evidence.ps1` 自动化实证 (Headless Verification)

### 1. 统一自动化测试脚本
在 `wink-ai-embedded` 仓库根目录下运行统一验证脚本：

```powershell
# 运行指定应用：
powershell -ExecutionPolicy Bypass -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App blink_gpio

# 批量运行所有已包含 unisim-scenarios 的 ESP-IDF 应用：
powershell -ExecutionPolicy Bypass -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1
```

### 2. 执行机制与安全性保障
1. 自动定位同级 `wink-ai` 仓库中的 `wink-tools` 命令行入口；
2. 注入开发通道标记 `$env:WINK_DEV = '1'`，绕过票据签发硬阻断；
3. 自动检测 `<app>/unisim-scenarios/` 目录并批量拉起 Headless 模式运行所有场景脚本；
4. 汇总各用例执行状态并输出彩色总览报告，进程返回标准退出码（0 = 全部成功，非 0 = 存在失败）。

### 3. 结果判断与标准输出
- **成功标准**：
  - 控制台按序输出各 Step 的绿色勾选标记 `✓ Step #N [ASSERT_POINT] @ ...µs - Status: PASSED`；
  - 最终汇总输出 `All ESP-IDF headless carriers PASSED.`，退出码为 `0`。

---

## 五、 阶段四：底座 CTest 回归与架构分层门禁 (Unit & Lint Gates)

如果当前适配的示例涉及底座 FreeRTOS 调度器、新增 HAL 驱动桩或门面扩充：

1. **底层 CTest 回归测试**：
   ```bash
   ctest -R test_esp_idf_<feature> --output-on-failure
   ```
2. **架构分层与 API 门禁校验**：
   依据 ADR-0043，在 `wink-ai-embedded` 运行分层门禁检查：
   ```bash
   winkcli lint --pack layering --pack api
   ```

---

## 六、 阶段五：核对清单 (Checklist) 回写与归档 (Checklist Sign-off)

只有在**阶段二（真实编译输出 assets）**与**阶段三（Headless 场景测试 100% 通过）**均成功完成之后，方可编辑 [CHECKLIST.md](CHECKLIST.md)：

1. 将对应条目的 `状态` 列更新为 `[x]`。
2. 在 `对应 wink-micro-app` 列填入创建的应用目录名（例如 `esp_idfv61_blink_gpio`）。
3. 在 `验收标准与架构说明` 列详细注明：
   - 包含 `unisim-assets/` 三件套与 `unisim-scenarios/` 场景脚本；
   - 经过 `run_esp32_headless_evidence.ps1` 驱动 UniSim Headless 测试 100% 绿灯；
   - 标注关键微秒级断言时序与实证结果。
4. 同步更新文档顶部统计计数。

---

## 七、 标杆示范案例对照 (Reference Examples)

### 黄金标杆：`#001: blink_gpio`（GPIO Output + FreeRTOS Task Delay）

- **代码位置**：[blink_gpio/](blink_gpio/)
- **官方源码**：`blink_example_main.c`（ESP-IDF v6.1 官方原始源码，SHA-256: `f22a5003ce...`，一行不改）
- **仿真资产包**：[blink_gpio/unisim-assets/](blink_gpio/unisim-assets/)
  - `device-tree.json` (362 B)
  - `wink_simulator.js` (122.6 KB)
  - `wink_simulator.wasm` (146.1 KB)
- **场景测试脚本**：[blink_gpio/unisim-scenarios/blink_gpio.scenario.json](blink_gpio/unisim-scenarios/blink_gpio.scenario.json)
- **实证时序断言表**：

| 时间点 (Virtual Time) | 触发事件 / 动作 | 预期 GPIO2 电平 | 断言结果 | 说明 |
| :---: | :--- | :---: | :---: | :--- |
| **50 ms** | 上电初始化完成，进入 loop 前首次置低 | `0` | **PASSED** | 验证 `gpio_reset_pin` 与初始熄灭 |
| **500 ms** | 处于第一个 1000ms `vTaskDelay` 周期中 | `0` | **PASSED** | 维持低电平无毛刺 |
| **1050 ms** | 第 1 次 `vTaskDelay` 超时唤醒，LED 翻转置高 | `1` | **PASSED** | 验证 FreeRTOS 调度器精确定时与引脚置高 |
| **1500 ms** | 处于第二个 1000ms `vTaskDelay` 周期中 | `1` | **PASSED** | 维持高电平点亮状态 |
| **2050 ms** | 第 2 次 `vTaskDelay` 超时唤醒，LED 翻转置低 | `0` | **PASSED** | 验证周期性循环与引脚翻转 |
| **2500 ms** | 处于第三个 1000ms `vTaskDelay` 周期中 | `0` | **PASSED** | 维持低电平熄灭状态 |
| **3050 ms** | 第 3 次 `vTaskDelay` 超时唤醒，LED 再次置高 | `1` | **PASSED** | 验证长程调度稳定性 |

- **执行实证记录**：
```text
=================== [UNISIM SIMULATION ENGINE (HEADLESS)] ===================
Loaded 1 scenarios from [blink_gpio.scenario.json]
[wink I] [    0 ms] [I] [example] I (0) example: Example configured to blink GPIO LED!
[wink I] [    0 ms] [I] [example] I (0) example: Turning the LED OFF!
[wink I] [ 1000 ms] [I] [example] I (1000) example: Turning the LED ON!
[wink I] [ 2000 ms] [I] [example] I (2000) example: Turning the LED OFF!
[wink I] [ 3000 ms] [I] [example] I (3000) example: Turning the LED ON!
      ✓ Step #1 [ASSERT_POINT] @ 50000µs - Status: PASSED
      ✓ Step #2 [ASSERT_POINT] @ 500000µs - Status: PASSED
      ✓ Step #3 [ASSERT_POINT] @ 1050000µs - Status: PASSED
      ✓ Step #4 [ASSERT_POINT] @ 1500000µs - Status: PASSED
      ✓ Step #5 [ASSERT_POINT] @ 2050000µs - Status: PASSED
      ✓ Step #6 [ASSERT_POINT] @ 2500000µs - Status: PASSED
      ✓ Step #7 [ASSERT_POINT] @ 3050000µs - Status: PASSED
  ✔ Status: PASS | Virtual Time: 3500000µs | Wall-Clock: 95ms
```

---

> **本手册是 ESP32 ESP-IDF v6.1 所有示例工程仿真适配的唯一权威执行手册。所有新示例的适配与核准均须严格遵循此手册五阶段闭环。**
