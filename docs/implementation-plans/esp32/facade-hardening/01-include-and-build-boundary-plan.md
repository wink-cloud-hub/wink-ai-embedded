<!-- SPDX-License-Identifier: Apache-2.0 -->
# 实施计划 01：ESP-IDF 头文件资产归属与构建边界硬隔离实施计划

| 字段 | 内容 |
|---|---|
| 计划编号 | PLAN-20260930-ESP-IDF-BOUNDARY-AND-CHANNELS-v1.3 |
| 状态 | 📝 **Ready for Execution（待公开头闭包解耦验证）** |
| 日期 | 2026-09-30 |
| 周期估算 | 1~1.5 个工作日 |
| 优先次序 | **_esp_error_check_failed 对齐与 esp_check.h 解耦 → sim_internal 物理隔离 → CMake PRIVATE/PUBLIC include 边界收敛 → 隔离负测与自包含正测 → channels.json 双向等价门禁 → Fail-Loud 宏规范 → 冲突显式决议** |
| 决策依据 | [ADR-0002：双 Target 同源编译机制](../../../decisions/unisim/0002-dual-target-compilation.md)<br>[ADR-0012：契约诚实优于静默降级（Fail-Loud 原则）](../../../decisions/core/0012-contract-honesty-over-silent-degradation.md)<br>[ADR-0043：分层门禁规范与 API 边界](../../../decisions/core/0043-layering-lint-rules.md)<br>[ADR-0085：SoC 双 SSOT 仲裁与静态选片](../../../decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)<br>[ADR-0087：资产通道登记与数据归属](../../../decisions/core/0087-esp-idf-asset-channels-and-soc-data-ownership.md) |
| 管辖数据源 | [`channels.json`](../../../../wink-micro-os/frameworks/esp_idf/channels.json)、[`esp_idf_sources.cmake`](../../../../wink-micro-os/frameworks/esp_idf/esp_idf_sources.cmake)、[`CMakeLists.txt`](../../../../wink-micro-os/frameworks/esp_idf/CMakeLists.txt)、[`include/README.md`](../../../../wink-micro-os/frameworks/esp_idf/include/README.md)、[`.github/scripts/check_harvested_headers.py`](../../../../.github/scripts/check_harvested_headers.py)、[`include/wink_sla.h`](../../../../wink-micro-os/frameworks/esp_idf/include/wink_sla.h) |
| 实施目标文件 | `include/esp_check.h`、`src/core/esp_err.c`、`esp_idf_sources.cmake`、`CMakeLists.txt`、`channels.json`、`include/README.md`、`check_harvested_headers.py`、`test/CMakeLists.txt` |
| 验收门禁 | 单头自包含编译门禁、CMake `try_compile` 隔离负测、Fail-Loud 阻断负测、`python .github/scripts/check_harvested_headers.py`、`python .github/scripts/check_license_map.py`、`winkcli lint --pack layering --pack api`、双 Target 真实编译回归 |

---

## 一、 治理范围与核心承诺

### 1.1 核心承诺（铁律）
* **公开头文件 100% 单头自包含与路径兼容（铁律 1）**：
  应用编写的 `#include "esp_err.h"`、`#include "esp_log.h"`、`#include "driver/gpio.h"`、`#include "freertos/task.h"`、`#include "driver/uart.h"` 等所有乐鑫原厂标准相对路径，严禁发生任何破坏性改动。
  每一个公开头文件必须能够在**消费者获得的完整公共依赖闭包（含 PAL PUBLIC 目录）下，不预先包含其他头文件、不暴露 Wink 内部私有路径**的前提下单头独立编译成功；
* **坚决对齐原厂错误检查契约，消除内核头污染（铁律 2）**：
  经白盒核查，当前公开头 [`esp_check.h:8-9`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/esp_check.h#L8) 包含了 `wink_runtime.h` 和 `wink_fault.h`，生成的 [`driver/uart.h:13`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/driver/uart.h#L13) 又包含了 `esp_check.h`。而原厂标准的错误拦截函数 [`_esp_error_check_failed()`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/core/esp_err.c#L67) 已在门面内部实现（调用 `abort()`）。**本次解耦彻底对齐原厂语义与签名，宏展开统一调用 `_esp_error_check_failed`，彻底斩断对 Wink 内核头的包含**；
* **报告片段原位保留，不搞无谓协同搬迁（铁律 3）**：
  `include/` 根目录下的两个生成报告片段（`api-coverage-matrix.inc.md`、`include-closure-inventory.inc.md`）与 `NOTICE.inc`、`manifest.json` 及现有门禁 [`check_harvested_headers.py`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/.github/scripts/check_harvested_headers.py) 强绑定。**本次治理绝不移动这两个片段文件**，保持编译闭包与流水线自锚完整；
* **双 Target（Wasm 仿真 vs 物理 ESP32）同源无损**：
  重构后的头文件布局与宏守卫，必须同时保证在 Wasm 仿真器及真实 ESP-IDF 物理芯片工具链下编译零报错。

### 1.2 现状痛点
1. **公开头向私有内核破窗反向穿透**：`esp_check.h` 将断言直接宏展开为 `wink_runtime_raise_fault()`，倒逼外部应用必须感知 Wink 内核私有头；
2. **构建边界比物理摆放更宽**：[`CMakeLists.txt:12`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/CMakeLists.txt#L12) 将整组 `ESP_IDF_FRAMEWORK_INCLUDES` 作为 `PUBLIC` 暴露，导致 `${CMAKE_CURRENT_LIST_DIR}/src/freertos`、`src/core`、`src/wifi`、`src/network` 内部路径泄漏，外部应用可随意引用 `freertos_sync.h`、`sim_wifi_env.h`；
3. **`sim_internal` 头文件缺乏物理访问边界**：部分仅用于内部测试的仿真桩头混在公开 `include/` 下，即便元数据标记为私有，编译器依然可以任意引用；
4. **资产通道文档漂移**：`channels.json` 中手写头实际已扩充至 **42 个**，而 `include/README.md:58` 仍滞留在“21 个手写头”；缺少性质与子系统属性；
5. **收割规则存在隐式覆盖盲区**：Harvester 在遇到不同组件中的同名相对路径时采用“先遇到先得”，SDK 升级或源根顺序微调易产生静默覆盖。

---

## 二、 详细实施任务拆解 (Action Items)

### 阶段 1：公开头解耦、物理隔离与构建边界硬收敛 (Header Decoupling & Boundary Hardening)

- [ ] **任务 T1.0**：对齐原厂 `_esp_error_check_failed` 签名与契约，解耦 [`include/esp_check.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/esp_check.h)：
  - 改造措施：
    1. 移除 `#include "wink_runtime.h"` 和 `#include "wink_fault.h"`；
    2. 对齐乐鑫原厂规范的断言函数声明：
       ```c
       void _esp_error_check_failed(esp_err_t rc, const char *file, int line, const char *function, const char *expression) __attribute__((__noreturn__));
       void _esp_error_check_failed_without_abort(esp_err_t rc, const char *file, int line, const char *function, const char *expression);
       ```
    3. `ESP_ERROR_CHECK` 宏展开统一调用 `_esp_error_check_failed(__err_rc, __FILE__, __LINE__, __ASSERT_FUNC, #x)`；
    4. 在 [`src/core/esp_err.c:67`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/core/esp_err.c#L67) 完善实现：输出详细错误日志 $\to$ 通知测试 Harness 捕获钩子 $\to$ 调用标准 `abort()` 终止；
    5. 编写单测 `test/core/test_esp_check.c`：验证 `ESP_ERROR_CHECK` 成功时不终止，失败时准确触发 `_esp_error_check_failed` 并被测试 Harness 拦截。

- [ ] **任务 T1.1**：仿真内部专有桩头物理迁出公开目录（Physical Relocation of `sim_internal`）：
  - 目标文件：仅用于仿真环境驱动而未在乐鑫官方 SDK 导出的纯内部桩头（如 `include/sim_net_responder.h`）；
  - 改造措施：
    1. 物理迁入 `wink-micro-os/frameworks/esp_idf/src/sim_include/` 或对应 `src/network/` 内部目录；
    2. 在公开 `include/` 中仅保留属于乐鑫官方 API 垫片（如 `esp_wifi.h`）或必须跨靶暴露的桥接头（如 `esp_idf_wink.h`）；
    3. 对于保留在公开目录的桥接头，加装仿真环境专用防呆守卫（见 T2.4）。

- [ ] **任务 T1.2**：拆分 [`esp_idf_sources.cmake`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/esp_idf_sources.cmake) 与收紧 [`CMakeLists.txt`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/CMakeLists.txt)：
  - **`ESP_IDF_FRAMEWORK_INCLUDES`（对外 PUBLIC）**：严格缩减为仅允许对外暴露的目录：
    ```cmake
    set(ESP_IDF_FRAMEWORK_INCLUDES
        ${WINK_ESP_TARGET_INCLUDE_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/shim/include
    )
    ```
  - **`ESP_IDF_FRAMEWORK_PRIVATE_INCLUDES`（内部 PRIVATE）**：收纳内部实现路径：
    ```cmake
    set(ESP_IDF_FRAMEWORK_PRIVATE_INCLUDES
        ${CMAKE_CURRENT_LIST_DIR}/src/freertos
        ${CMAKE_CURRENT_LIST_DIR}/src/core
        ${CMAKE_CURRENT_LIST_DIR}/src/wifi
        ${CMAKE_CURRENT_LIST_DIR}/src/network
        ${CMAKE_CURRENT_LIST_DIR}/src/sim_include
        ${CMAKE_CURRENT_LIST_DIR}/../../targets/common/include
        ${CMAKE_CURRENT_LIST_DIR}/../../trace/include
        ${CMAKE_CURRENT_LIST_DIR}/../../runtime/include
    )
    ```
  - `CMakeLists.txt` 中将私有目录以 `PRIVATE` 注入 `wink_framework_esp_idf`，彻底屏蔽内部目录对消费者的泄漏。

- [ ] **任务 T1.3**：全量消费者破窗依赖清查与收敛：
  - 静态检索 `wink-micro-app/` 与 `test/` 下的所有源文件，排查是否存在应用层或非核心测试单元违规引用 `freertos_sync.h`、`sim_wifi_env.h`、`sim_network_broker.h` 等内部头；
  - 若存在非内部测试 Harness 依赖，将其显式收敛至测试专属编译配置或标准化门面接口。

- [ ] **任务 T1.4**：隔离子工程负向测试（CMake `try_compile` Negative Boundary Test）：
  - 废弃在 CTest 中使用 `WILL_FAIL` 标记普通构建目标的错误设计（因编译错误会导致整个父工程在构建期直接挂死，无法进入测试阶段）；
  - 编写专门的 CMake 隔离子工程负测试：
    - 在 `test/core/negative_boundary_test/` 放置一个独立的微型 CMake 工程；
    - 测试源文件 `negative_boundary_consumer.c` 仅链接 `wink_framework_esp_idf`，尝试 `#include "freertos_sync.h"`；
    - 顶层 CMake 通过 `try_compile(RESULT_VAR ...)` 或 CTest 脚本调用独立子构建，断言子工程构建失败（`RESULT_VAR FALSE`），并在日志输出中匹配到 `fatal error: freertos_sync.h: No such file or directory`。

- [ ] **任务 T1.5**：自动化单头自包含测试与头文件分类测试（Standalone Header Validation）：
  - 新增 CMake 验证目标 `check_esp_idf_headers_standalone`：
    - 运行环境：使用消费者实际获得的完整 PUBLIC 依赖闭包（`wink_framework_esp_idf` PUBLIC Includes + `wink_pal` PUBLIC Includes）；
    - 针对不同类别头文件实施精准验证：
      - **A 类（Vendor Public 原厂公开头）**：遍历 `esp_err.h`, `driver/*.h`, `freertos/*.h` 等，生成临时单元独立编译，验证 100% 自包含；
      - **B 类（Sim Bridge 跨靶桥接头）**：针对 `esp_idf_wink.h`，验证在 `SIMULATION` 宏定义下可正常单头编译，并增加真机交叉编译负测（断言 `#error` 守卫精确触发）；
      - **C 类（Vendor Internal 原厂内部头）**：对 `esp_private/*.h` 建立独立白名单，明确其供内部组件调用，不向 App 做出单独自包含承诺。

---

### 阶段 2：资产通道元数据升级与门禁强等价校验 (Channels & Strict SSOT Alignment)

- [ ] **任务 T2.1**：升级 [`channels.json`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/channels.json) Schema：
  - 严格保持现有 `handwritten` 字符串数组向下兼容，在其旁边新增结构化属性列表 `handwritten_entries`：
    ```json
    {
      "schema_version": 2,
      "framework": "esp_idf",
      "default_soc": "esp32",
      "handwritten": [
        "esp_wifi.h",
        "esp_idf_wink.h"
      ],
      "handwritten_entries": [
        {
          "path": "esp_wifi.h",
          "nature": "vendor_public",
          "subsystem": "wifi",
          "upstream_component": "esp_wifi"
        },
        {
          "path": "esp_idf_wink.h",
          "nature": "sim_bridge",
          "subsystem": "core",
          "upstream_component": "none"
        }
      ]
    }
    ```
  - 规范性质分类：
    - `vendor_public`：乐鑫原厂标准公开 API 垫片；
    - `sim_bridge`：仿真必须保留的跨靶桥接与控制头（如 `esp_idf_wink.h`）；
    - `sim_internal`：必须已迁出 `include/` 至内部私有路径的辅助头。

- [ ] **任务 T2.2**：修正 [`include/README.md`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/README.md) 文档漂移：
  - 将第 58 行过期的“21 个手写头”更新为与当前代码一致的真实数量；
  - 按子系统（Core, Driver, FreeRTOS, Storage, WiFi, Network, BLE）列出手写资产清单，彻底消除文档漂移。

- [ ] **任务 T2.3**：强化 [check_harvested_headers.py](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/.github/scripts/check_harvested_headers.py) 门禁：
  - **集合严格 1:1 双向等价校验（防双 SSOT 漂移）**：
    新增断言逻辑：提取 `handwritten` 列表与 `handwritten_entries` 中的 `path` 字段，断言两个集合严格完全相等：
    ```python
    entries_paths = {e["path"] for e in channels.get("handwritten_entries", [])}
    if set(channels["handwritten"]) != entries_paths:
        errors.append("channels.json SSOT divergence: 'handwritten' and 'handwritten_entries' must be strictly identical sets")
    ```
  - **文档统计数值自校验**：增加校验项，自动解析 `include/README.md` 中声明的手写数量并与 `channels.json` 比对，数字不匹配直接阻断 CI；
  - 确认保持现有的未标记孤儿头双向差集拦截（`unmarked_set - handwritten`）；
  - **多 SSOT 自洽性扫描（T2.3.4）**：校验 `channels.json` 中手写头与物理文件 1:1 吻合，并核验与 `capability-catalog.yaml` 的状态对齐。

- [ ] **任务 T2.4**：【关键防呆】纯仿真桥接头加装真机硬守卫（Simulation Leakage Guard）：
  - 目标文件：[`include/esp_idf_wink.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/esp_idf_wink.h) 等跨靶头；
  - 注入防误调宏守卫：
    ```c
    #if !defined(SIMULATION) && !defined(WINK_SIM_TEST)
    #error "FATAL: This header is a WinkMicroOS simulation-only header! It cannot be included in physical ESP-IDF hardware builds."
    #endif
    ```

- [ ] **任务 T2.5**：规范 Out-of-Scope API 的 Fail-Loud 宏拦截机制（ADR-0012 落地）：
  - 目标文件：[`include/wink_sla.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/include/wink_sla.h) 及对应驱动桩头；
  - 规范原则：对于 `checklist.md` 中 187 项明确声明为 `[-] Out-of-Scope` 或暂缓实现的特性 API（如 eFuse 熔丝操作、特定外部 PHY 等）：
    1. 编译期阻断：在头文件中使用 `WINK_SLA_ERROR("API is Out-of-Scope under Wink Wasm SLA")` 声明；
    2. 运行期防呆：若函数有占位定义，内部必须调用 `wink_runtime_raise_fault(WINK_FAULT_UNSUPPORTED)` 并返回 `ESP_ERR_NOT_SUPPORTED`；
    3. 严禁任何不支持的 API 存在静默返回 `ESP_OK` 的伪实现。

---

### 阶段 3：收割规则显式冲突裁决机制 (Harvester Collision Precedence Map)

- [ ] **任务 T3.1**：审查闭源收割规则 `rules/esp_idf.yaml`：
  - 排查各组件在单一 include 根打平时可能发生路径碰撞的头文件（如 `esp_system` 与 `esp_common` 重叠项）；
  - 增加 `collision_precedence` 显式裁决字段；
- [ ] **任务 T3.2**：产出逐头来源组件清单（Source Provenance Map）：
  - 在 `docs/03-include-closure-inventory.md` 固化逐头来源清单，逐头记录相对路径、所属原厂组件及对应 SHA-256；
  - 对未显式登记冲突的多源同名头，Harvester 在执行期直接以 `FATAL_ERROR` 退出，彻底消除“先到先得”的隐式覆盖风险；
  - *注：报告片段文件原位保留在 `include/`，不进行任何破坏性移动*。

---

## 三、 风险评估与回滚方案 (Risk & Rollback Matrix)

| 风险项 | 触发场景 | 预防与缓解措施 | 回滚操作 (Rollback Action) |
|---|---|---|---|
| **R-01 公开头单头编译失败** | 某头文件隐式依赖其他被收紧的头文件 | 严格依赖 T1.5 单头自包含测试，在 CI 本地完整跑通后再提 PR | 检查断链节点，在门面内部以纯 C-ABI 或垫片补齐，严禁粗暴回退为全局 PUBLIC |
| **R-02 双 Target 编译分化** | Wasm 编译通过但在 ESP32 交叉编译下报宏冲突 | 本地使用 `wink.py esp32 --app devkitc_smoke` 验证真实交叉编译 | 检查条件宏分支（`#ifdef SIMULATION`），保持原厂头宏声明不变 |
| **R-03 门禁自校验挂起** | `channels.json` 格式不一致导致门禁拦截 | 严格实施 T2.3 的集合等价校验；本地先行运行脚本确认 0 错误 | `git checkout -- wink-micro-os/frameworks/esp_idf/channels.json` |

---

## 四、 全局验收标准 (Definition of Done)

1. **DoD-1（构建隔离有效性）**：
   - CMake `try_compile` 隔离负测尝试 `#include "freertos_sync.h"` 时必须 100% 发生编译报错并被测试脚本精确捕获；
2. **DoD-2（单头自包含性与错误检查契约）**：
   - 自动化单头编译测试 `check_esp_idf_headers_standalone` 遍历所有公开头，编译通过率 100%；
   - `esp_check.h` 内部不再包含任何 `wink_runtime.h` 或 `wink_fault.h`，完全对齐 `_esp_error_check_failed` 签名与 `abort()` 语义并通过行为测试；
3. **DoD-3（资产清单一致性与双 SSOT 防漂移）**：
   - `python .github/scripts/check_harvested_headers.py` 执行输出 `errors=0`；
   - `channels.json` 中 `handwritten` 与 `handwritten_entries` 集合严格等价；
   - `include/README.md` 中的手写统计数值与 `channels.json` 完全一致；
4. **DoD-4（Fail-Loud 宏拦截有效性）**：
   - 至少抽样 2 个 Out-of-Scope API 编写测试，断言编译期报错或运行期抛出 `ESP_ERR_NOT_SUPPORTED`，禁止静默成功；
5. **DoD-5（代码分层与许可门禁）**：
   - `winkcli lint --pack layering --pack api` 检查通过；
   - `python .github/scripts/check_license_map.py` 检查通过；
6. **DoD-6（双 Target 真实构建全绿）**：
   - Native Wasm 单测全量绿灯（保存真实构建与测试结果日志）；
   - ESP32 物理硬件交叉编译（`wink.py esp32 --app devkitc_smoke`）零报错。
