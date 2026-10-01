# ESP-IDF v6.1 官方示例仿真适配与测试标准执行手册 (Playbook)

> **版本**：v2.0 (Aligned with Schema v2.0 & ADR-0091)  
> **适用芯片**：Espressif ESP32 / ESP32-S3 / ESP32-C3 / ESP32-C6 系列  
> **数据单一真理源 (SSOT)**：[`checklist.data.json`](checklist.data.json)  
> **派生执行看板**：[`CHECKLIST.md`](CHECKLIST.md)（由生成脚本单向生成，**严禁纯手工编辑**）  
> **执行脚本**：[run_esp32_headless_evidence.ps1](../../../wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1)  
> **门禁规范**：[`CLASSIFICATION-SPEC.md`](CLASSIFICATION-SPEC.md) 与 [`.gates/gates.yaml`](.gates/gates.yaml)  
> **适用对象**：AI Coding Agents（Antigravity、Claude Code 等）与嵌入式开发工程师。

---

## 零、 核心原则与硬性门禁 (Non-Negotiable Gates)

对于清单 [`checklist.data.json`](checklist.data.json) 中的**每一个示例及其执行配置实例 (`executions[config]`)**，在满足看板打勾 `[x]`（已验证）之前，**必须严格执行并通过以下硬性约束**：

1. **执行配置 (executions) 一等公民实体**：
   每个示例支持多种执行宿主配置（`wasm_browser`、`wasm_node`、`esp32_hardware`、`host_native`）。**严禁一个后端的测试通过自动代表其他配置已交付**。必须针对特定配置生成并绑定独立的测试凭据。
2. **按 Target/Backend 提供真实异构交付资产 (Heterogeneous Assets)**：
   依据执行配置的底层物理属性，分别产出对应防伪资产：
   - **Wasm 仿真配置 (`wasm_*`)**：使用 `wink-tools` 编译输出微应用目录下的 `unisim-assets/` 三件套（`device-tree.json`, `wink_simulator.js`, `wink_simulator.wasm`）；
   - **物理芯片配置 (`esp32_hardware`)**：产出真实 xtensa 交叉编译二进制产物（ELF / BIN / MAP）及对应 SHA-256；
   - **宿主/构建类配置 (`host_native` / `build_system`)**：产出本地原生构建二进制或标准编译链接执行日志。
3. **自动化测试实证闭环 (Verifiable Evidence)**：
   依据配置属性完成真实自动化验证，严禁仅以声明充当交付：
   - **Wasm 仿真配置**：编写 `unisim-scenarios/<name>.scenario.json`，经由 `run_esp32_headless_evidence.ps1` 执行无头仿真，所有微秒断言步骤 100% 绿灯；
   - **物理芯片配置**：提供真机烧录输出、串口上电启动日志与预期特征字符串正则匹配记录；
   - **构建系统配置**：提供 CMake 编译输出日志与进程退出码为 0 的执行报告。
4. **原厂源码“一行不改”准则**：
   从 ESP-IDF 官方仓库镜像的代码文件（如 `blink_example_main.c`、`ledc_basic_example_main.c`）必须保持原汁原味，上游 SHA-256 哈希值需在 `wink-app.json` 中锁定。Kconfig / `sdkconfig` 宏定义一律在独立的 `include/sdkconfig.h` 中进行私有覆盖，底层行为由 `wink_framework_esp_idf` 门面垫片透明承接。
5. **SSOT 单一写入路径铁律**：
   所有交付凭证必须写入单一数据源 [`checklist.data.json`](checklist.data.json) 中对应配置的 `evidence` 字段，随后运行生成脚本自动更新看板 [`CHECKLIST.md`](CHECKLIST.md)。**严禁手动直接修改 CHECKLIST.md 中的勾选状态！**
6. **门面底座中立与防腐铁律（Zero Kernel Bypass & Anti-Decay）**：
   为保证 291 项 Active 示例（1 项已验证，290 项待适配）的接入不会侵染、劣化底层仿真门面（`wink_framework_esp_idf`），任何示例适配必须遵守以下硬性防腐红线：
   - **严禁 App 特化分支**：底座门面严禁出现 `if (strcmp(app, ...))` 等针对特定示例的私有 Bypass；行为差异必须通过原厂标准配置宏（Kconfig / sdkconfig）或确定性场景注入驱动；
   - **严禁随意开临时延时纤程**：严禁在协议栈或驱动中调用 `xTaskCreate` 启动临时延时任务，必须统一使用带代际 Token 的定时器工作项，杜绝打爆 LITE 8 任务槽；
   - **新增资源必须自锚复位因果链**：新增任何有状态或句柄的外设/协议模块，必须在 `esp_idf_bridge.c` 复位流程中注册注销逻辑，并在无头场景结束时验证基线干净；
   - **严禁手工伪造未收割原厂头**：必须经由 Harvester 生成或按 SLA 规范声明，严禁在 `include/` 私设手写头。
7. **防假绿与语义完整性硬性门禁（Anti-False-Green & Scenario Semantic Integrity）**：
   任何场景编排必须遵循真测试四大公理（因果性、状态跳变、拓扑闭环、变异杀伤）。严禁退化为静态无关电源断言（如 `power:VCC_3V3 == 3.3`）、无状态跳变或恒真判定。断言必须直接命中被测业务核心出口（GPIO 翻转、串口回显、网络状态码与报文、PWM 占空比等），引脚必须在 `wink-app.json` 声明。所有 Wasm 仿真用例交付前必须通过 `g1.scenario_semantic_integrity` 门禁与 Canary 变异杀伤校验。现场操作强制执行 Skill [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md)。

> [!CAUTION]
> **绝对门禁声明**：
> 任何未在 App 独立目录下产出对应 Target 真实资产、未通过自动化实证检验（含防假绿语义完整性检验）并生成合规执行报告的配置实例，其 `delivery_state` 必须诚实保留为 `planned`，`evidence` 必须为 `null`，**一律严禁在看板中标记为 `[x]`！** 仅通过底层 CTest 编译不等于应用级交付。

---

## 一、 标准实施闭环工作流 (5 阶段：通用准备 + 目标分支实证 + 统一收口)

```text
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段一：新建 App 与原厂代码镜像 (Setup & Mirror) [通用准备]            │
│   ├── 创建 wink-micro-app/vendor/esp_idfv61/<feature>/                 │
│   ├── 镜像官方源码（保持一行不改，计算 SHA-256 哈希）                    │
│   ├── 配置 include/sdkconfig.h (Kconfig 语料私有宏)                     │
│   ├── 配置 CMakeLists.txt (导出 WINK_APP_SOURCES 与 WINK_APP_ESP_IDF)   │
│   └── 配置 wink-app.json (声明 board, mcu, devices 引脚拓扑)           │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段二：Target 差异化编译构建 (Branch by Target Backend) [分支阶段]    │
│   ├── 分支 A (Wasm 仿真): wink.py build sim --app <app>               │
│   │   └── 生成并校验 unisim-assets/ 三件套 (tree, js, wasm)            │
│   ├── 分支 B (ESP32 硬件): wink.py esp32 build <app> 或 idf.py build   │
│   │   └── 生成真实 xtensa ELF 固件并计算 firmware_elf_sha256           │
│   └── 分支 C (构建工程): CMake 原生工程生成与构建编译链接测试          │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段三：Target 差异化自动化实证 (Branch by Target Backend) [分支阶段]  │
│   ├── 分支 A (Wasm 仿真): run_esp32_headless_evidence.ps1 -App <app>   │
│   │   ├── 本地生成凭据: 带 -WriteEvidence 参数回写 assets & scenario 哈希│
│   │   └── CI 只读重验: 纯只读校验 scenario 与 report 断言 100% 绿灯     │
│   ├── 分支 B (ESP32 硬件): 串口自动化断言 (Serial Output Assertion)    │
│   │   └── 刷入固件，捕获串口启动特征日志，断言正则全绿，产出 hw-report │
│   └── 分支 C (构建工程): 原生构建链路状态校验与编译日志归档           │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段四：底座 CTest 回归与防腐门禁检查 (Unit & Anti-Decay Gates) [通用] │
│   ├── 执行 CTest 单元测试验证底层外设模型 (test_esp_idf_*)             │
│   ├── 运行分层架构防腐检查: winkcli lint --pack layering --pack api     │
│   └── 运行 Gate 1~5 门禁校验: python run_gates.py --mode pr            │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 阶段五：数据源 (checklist.data.json) 归档与看板生成 (Sign-off) [通用]   │
│   ├── 定位指定配置实例 config_id，置 delivery_state 为 "verified"       │
│   ├── 填入真实防伪凭据 evidence (支持 wasm/hardware/build 多态 backend)│
│   ├── 运行生成脚本: python generate_checklist_v1_1.py                  │
│   └── 依据六要素合取公式 CanCheckMark(E, C) 自动在 CHECKLIST.md 打勾   │
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
      "type": "INJECT_NET_FIXTURE",
      "timeUs": "0ms",
      "protocol": "http",
      "description": "上电注入网络模拟应答路由 (URL 前缀、状态码与响应体)",
      "routes": [
        {
          "method": "GET",
          "url_prefix": "http://httpbin.org/get",
          "status_code": 200,
          "headers": { "Content-Type": "application/json" },
          "body": "{\"origin\":\"127.0.0.1\",\"url\":\"http://httpbin.org/get\"}"
        }
      ]
    },
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

> **网络测试步骤说明 (`INJECT_NET_FIXTURE`)**：
> - 适用于 HTTP/MQTT 等网络类示例。无头运行器（Headless Runner）在启动微应用仿真时，解析并在 C 门面注册模拟路由表与静态载荷；
> - 运行结束或复位时自动调用 `sim_net_responder_reset()` 清空，杜绝跨用例状态串扰。

### 5. 原厂未收割 API 处置规程（Strict Harvester Pipeline）
若官方示例代码调用了当前门面公开 `include/` 中尚未收割的原厂 API：
- **路径 A（纳入实现）**：必须通过 Harvester 闭源收割规则生成对应头文件并经门禁校验合入，**严禁开发者在 `include/` 下手工捏造未经审定的原厂头**；
- **路径 B（声明 Out-of-Scope）**：若该特性属于明确排除范围（如 eFuse 熔丝硬件、特定外部 PHY 等），必须在 `wink_sla.h` 下使用 `WINK_SLA_ERROR` 进行编译期阻断，或运行期返回 `ESP_ERR_NOT_SUPPORTED`，并在 `checklist.data.json` 标记为 `[-] Out-of-Scope`，**严禁写空函数静默返回 `ESP_OK`**。



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

## 四、 阶段三：自动化实证闭环 (Target-Specific Automated Evidence)

依据执行配置的 Target 属性，进入对应的自动化实证流水线：

### 1. Wasm 仿真配置实证（`run_esp32_headless_evidence.ps1`）
在 `wink-ai-embedded` 仓库根目录下运行统一无头实证脚本：

```powershell
# 本地测试指定应用并生成凭据（写入 assets_sha256 与 run-report.json）：
powershell -ExecutionPolicy Bypass -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App blink_gpio -WriteEvidence

# 纯只读回归测试（CI 与本地验证模式，绝不修改数据源）：
powershell -ExecutionPolicy Bypass -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App blink_gpio

# 批量运行所有已包含 unisim-scenarios 的 ESP-IDF 应用：
powershell -ExecutionPolicy Bypass -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1
```

- **执行机制与安全性保障**：
  1. 自动定位 `wink-tools` 命令行入口；
  2. 注入开发通道标记 `$env:WINK_DEV = '1'`，绕过票据签发硬阻断；
  3. 自动检测 `<app>/unisim-scenarios/` 目录并批量拉起 Headless 模式运行所有场景脚本；
  4. 汇总各用例执行状态并输出彩色总览报告，进程返回标准退出码（0 = 全部成功，非 0 = 存在失败）。
- **结果判断与标准输出**：
  - 各 Step 绿色勾选 `✓ Step #N [ASSERT_POINT] @ ...µs - Status: PASSED`；
  - 最终汇总 `All ESP-IDF headless carriers PASSED.`，退出码为 `0`。

### 2. 物理硬件配置实证（ESP32 真实芯片串口断言流程）
对于已上真机验证的条目（如 `esp32_hardware` 配置）：
1. **交叉编译固件**：通过 `wink.py esp32 build <app>` 或 ESP-IDF 原厂 `idf.py build` 产出目标 ELF 与 BIN；
2. **防伪哈希锁定**：计算 ELF 真实 SHA-256 填入 `firmware_elf_sha256`；
3. **串口自动化捕获与断言**：
   - 烧录至指定物理开发板（如 `esp32_devkitc_v4`）；
   - 通过串口监视工具（115200 波特率）捕获芯片上电冷启动与业务日志；
   - 依据 `acceptance.positive_cases` 中声明的正则模式进行逐行命中匹配；
   - 输出符合统一 Schema 的串口执行报告 `.governance/reports/hw/run-<timestamp>-<app>.json`（包含 `passed_steps`, `board_type`, `raw_log`）；
4. **差分比对（Diff Parity）**：对要求虚实差分比对的条目，将真机串口时戳与 Wasm 仿真虚拟时钟对齐，验证事件时序误差在声明的 `tolerance_us` 阈值以内。

### 3. 纯构建工程配置实证（Build System Verification）
对于原厂构建系统与工具链验证示例（`#426~#444`）：
1. **CMake 原生调用**：使用原生构建系统执行完整生成与编译：
   ```bash
   cmake -B build -S examples/build_system/<feature>
   cmake --build build
   ```
2. **编译日志归档**：将完整标准输出与标准错误流归档至 `.governance/reports/build/run-<timestamp>-<app>.log`；
3. **断言与指标提取**：
   - 验证构建退出码为 `0`；
   - 记录编译器完整版本号（如 `emcc 3.1.56` 或 `xtensa-esp32-elf-gcc 13.2.0`）；
   - 在执行报告中记录生成的产物列表与符号表校验结果。

### 4. 场景复位与状态基线断言规范（Reset State Baseline）
针对网络、蓝牙、外设等有状态示例，在场景脚本末尾应包含**软复位与基线断言步骤**：
- 验证应用在执行完成或断开后，系统调用软复位能够干净回到初始态；
- 杜绝因前一个用例未排空后台工作项或未注销事件观察者，导致后续无头用例发生幽灵状态串扰。

### 5. 场景语义防假绿与变异杀伤校验（Anti-False-Green & Canary Mutation）
为彻底杜绝“假绿测试”（False Green / Vacuous Pass），在设计与交付 Wasm 场景（`unisim-scenarios/*.scenario.json`）时，必须执行以下硬性标准：
1. **领域 Target 命中**：断言必须针对所属门面域的核心特性（控制类测 `gpio:*`/`pwm:*`，总线类测 `uart:*`/`ASSERT_BUS_PAYLOAD`，网络类测 `http:client:*`/`mqtt:*` 等）。严禁退化为断言静态电源（`power:VCC_*`）；
2. **状态跳变覆盖**：周期或时序用例必须包含至少 1 次电平或状态跳变断言，严禁仅测 50ms 初始静态值；
3. **引脚拓扑合规**：场景断言中涉及的所有物理引脚与器件，必须已在 `wink-app.json` 的 `devices` 节点中明确声明；
4. **变异杀伤检验（Canary Mutation）**：在交付前，必须通过人为篡改预期值或破坏注入环境，验证断言能够真实失败报红。无法被破坏杀死的断言严禁投入生产；
5. **门禁自动扫描**：提交 PR 前必须执行 `python run_gates.py --mode pr`，确保 `g1.scenario_semantic_integrity` 与 `evidence_verifier` 为 0 Error。具体操作详见 Skill [governance-sop-esp](../../../.agents/skills/governance-sop-esp/SKILL.md) 与 [domain-assertion-guide.md](../../../.agents/skills/governance-sop-esp/references/domain-assertion-guide.md)。

---

## 五、 阶段四：底座 CTest 回归与架构分层三大硬门禁 (Unit & Anti-Decay Gates)

如果当前适配的示例涉及底座 FreeRTOS 调度器、新增 HAL 驱动桩或门面扩充，提交前必须完整跑通以下三大硬门禁：

1. **门禁 1：全门面 CTest 零回归（Full CTest Regression）**：
   严禁只跑当前示例相关的小测试。必须执行全量 ESP-IDF 门面回归，确保既有全部单测保持 100% 全绿：
   ```bash
   ctest -L esp_idf --output-on-failure
   ```
2. **门禁 2：复位因果图与 Delta 零泄漏门禁（Reset & Memory Delta Gate）**：
   - 运行复位双断言测试：验证优雅退出时内存与任务差额（Delta）为 0；
   - 运行并发重启测试：验证在运行期调用 `esp_restart()` 后，各对象池重置为干净基线，旧句柄/回调彻底失效。
3. **门禁 3：架构分层与 API 规范门禁（Layering & Lint Gate）**：
   依据 ADR-0043，在 `wink-ai-embedded` 运行分层门禁检查，严禁任何业务代码破窗引用 `src/freertos/`、`src/wifi/` 等私有实现路径：
   ```bash
   winkcli lint --pack layering --pack api
   ```

---

## 六、 阶段五：数据源 (checklist.data.json) 归档与看板生成 (Checklist Sign-off)

只有在**阶段二（真实编译输出 assets）**与**阶段三（Headless 场景测试 100% 通过）**均成功完成之后，方可触发归档流程。**严禁直接纯手工编辑 `CHECKLIST.md` 看板！** 必须遵循以下单一数据写入与派生渲染标准流程：

### 1. 定位目标配置实例并更新状态
在 [`checklist.data.json`](checklist.data.json) 中查找到当前示例条目（通过语义稳定 `id`），在其 `executions: [...]` 数组中定位本次交付的具体配置项（通过 `config_id`，如 `sim_browser_esp32`）：
1. 将该配置实例的 `delivery_state` 由 `"building"` 或 `"planned"` 更新为 `"verified"`；
2. 确保前置条件满足：`scope.inclusion == "in_scope"`，`audit.verdict == "audited"`（且 `audited_configs` 包含当前 `config_id`），所引用的原子能力依赖图谱处于 `satisfied`。

### 2. 写入真实防伪凭据 (evidence)
在当前配置对象的 `evidence` 字段中回写对应后端格式的真实凭据（严禁留空或填入伪造占位符）。

**A. 仿真后端配置（`backend: "wasm_simulation"`）**：
```json
"evidence": {
  "backend": "wasm_simulation",
  "run_id": "run-20260929-1400-blink-01",
  "assets_sha256": "<三件套规范复合SHA256>",
  "scenario_sha256": "<unisim-scenarios/*.scenario.json校验和>",
  "execution_report_ref": ".governance/reports/esp32/run-20260929-1400-blink-01.json",
  "verified_commit": "<当前工作区 Git HEAD 提交哈希>",
  "verified_at": "2026-09-29T14:00:00Z"
}
```
> **三件套复合哈希计算规范**：
> `assets_sha256` = `SHA256(SHA256(wink_simulator.wasm) + "\n" + SHA256(wink_simulator.js) + "\n" + SHA256(device-tree.json))`。由本地生成脚本统一计算，门禁按相同算法重算复核。

**B. 真实硬件后端配置（`backend: "esp32_hardware"`）**：
```json
"evidence": {
  "backend": "esp32_hardware",
  "run_id": "hw-20260929-1500-blink-01",
  "firmware_elf_sha256": "<交叉编译生成ELF文件哈希>",
  "serial_log_report_ref": ".governance/reports/hw/run-20260929-1500-blink-01.json",
  "board_type": "esp32_devkitc_v4",
  "verified_commit": "<当前工作区 Git HEAD 提交哈希>",
  "verified_at": "2026-09-29T15:00:00Z"
}
```

**C. 纯构建验证配置（`backend: "build_system"`）**：
```json
"evidence": {
  "backend": "build_system",
  "run_id": "build-20260929-1600-blink-01",
  "build_log_ref": ".governance/reports/build/run-20260929-1600-blink-01.log",
  "compiler_version": "emcc-3.1.56",
  "verified_commit": "<当前工作区 Git HEAD 提交哈希>",
  "verified_at": "2026-09-29T16:00:00Z"
}
```

> [!IMPORTANT]
> **本地生成凭据 vs CI 纯只读校验操作边界 (Local Generation vs CI Read-Only Boundary)**：
> 1. **本地开发态（凭据生成与回写）**：
>    - 开发者本地运行带 `-WriteEvidence` 的无头实证脚本：
>      ```powershell
>      powershell -ExecutionPolicy Bypass -File wink-micro-os/frameworks/esp_idf/tools/run_esp32_headless_evidence.ps1 -App <app> -WriteEvidence
>      ```
>    - 脚本会自动重新构建最新资产、计算标准化三件套复合哈希（`SHA256(SHA256(wasm) || "\n" || SHA256(js) || "\n" || SHA256(tree))`）与场景哈希，并将最新运行结果与报告路径安全回写至 `checklist.data.json`；
>    - 接着运行看板生成脚本单向更新 `CHECKLIST.md`：
>      ```bash
>      python wink-micro-app/vendor/esp_idfv61/.governance/tools/generate_checklist_v1_1.py
>      ```
> 2. **CI 门禁态（纯只读核验与强制阻断）**：
>    - **严禁在 CI 流程中传递 `-WriteEvidence` 或执行任何写回操作！**
>    - CI 运行 `run_gates.py`，内置的 `evidence_verifier.py` 纯只读加载工作区资产与 `run-report.json`，独立重算哈希并严格校验：
>      - 资产哈希一致性（`assets_sha256` 必须与工作区三件套精确吻合）；
>      - 场景用例一致性（`scenario_sha256` 必须与用例文件精确吻合）；
>      - 报告执行真实性（报告中的状态必须为 `passed`，且 `failedSteps == 0`、`errorSteps == 0`）；
>    - 任何不匹配、陈旧凭据或报告缺失一律直接 Fail-Closed 阻断 PR 合入。

### 3. 单向重新生成执行看板
在仓库根目录执行看板生成脚本，由脚本基于六要素合取公式 $\text{CanCheckMark}(E, C)$ 自动裁判并渲染打勾：
```bash
python wink-micro-app/vendor/esp_idfv61/.governance/tools/generate_checklist_v1_1.py
```
- 若所有充要条件满足且防伪哈希与本地资产匹配，脚本将自动在 [`CHECKLIST.md`](CHECKLIST.md) 对应行输出 `[x]` 并更新统计汇总；
- 若出现任何凭据缺失、哈希不匹配或测试失败断言，脚本将输出 `[ ]` 或 `[!]` 阻断警告，绝不放行。

### 4. 运行全量 CI 门禁核验
提交 PR 前，运行门禁流水线确保 Gate 1~5 与分层防腐检查全部通过：
```bash
# 1. 运行门禁系统 (PR 模式)
python wink-micro-app/vendor/esp_idfv61/.governance/gates/run_gates.py --mode pr --allow-empty-diff

# 2. 运行分层架构与 API 门禁
winkcli lint --pack layering --pack api

# 3. 运行数据源与看板不变量自洽检查
python .github/scripts/check_ssot_invariants.py
```

---

## 七、 标杆示范案例对照 (Reference Examples)

### 黄金标杆：`#001: blink_gpio`（GPIO Output + FreeRTOS Task Delay）

- **代码位置**：[get-started/blink_gpio/](../../get-started/blink_gpio/)
- **官方源码**：`blink_example_main.c`（ESP-IDF v6.1 官方原始源码，SHA-256: `f22a5003ce...`，一行不改）
- **仿真资产包**：[get-started/blink_gpio/unisim-assets/](../../get-started/blink_gpio/unisim-assets/)
  - `device-tree.json` (362 B)
  - `wink_simulator.js` (122.6 KB)
  - `wink_simulator.wasm` (146.1 KB)
- **场景测试脚本**：[get-started/blink_gpio/unisim-scenarios/blink_gpio.scenario.json](../../get-started/blink_gpio/unisim-scenarios/blink_gpio.scenario.json)
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
