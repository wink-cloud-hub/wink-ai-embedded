# 评审报告：ESP-IDF 源码级仿真拦截层实施总纲计划

| 项 | 内容 |
|---|---|
| **评审对象** | [`2026-09-22-esp-idf-simulation-interception-master-plan.md`](./2026-09-22-esp-idf-simulation-interception-master-plan.md)（PLAN-20260922-ESP-IDF-SIM-MASTER v1.0） |
| **评审日期** | 2026-09-22 |
| **评审视角** | 资深嵌入式工程师（ESP-IDF 驱动/FreeRTOS/仿真拦截） + 仓库架构规范 |
| **评审输入** | ① ESP-IDF 官方源码 `D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf`（v6.1）；② 官方 example 语料；③ 仓库 ADR/设计规范/既有实现（`docs/decisions/`、`docs/zh/design/`、`frameworks/mcs51`、`targets/common`、`pal/include`、`wink-tools/tools/lint`） |
| **评审方法** | 逐条对照官方源码宏/头文件包含关系取证；逐条对照仓库现行 SSOT 与既有设计；目录树按 mcs51 先例与《目录架构设计》§4 逐项比对 |
| **评级** | **架构方向 8/10 · IDF 事实准确性 4/10 · 目录树 5/10 · 文档完整性 3/10 · 综合 5.5/10（需修订后方可"就绪"）** |
| **放置说明** | 按用户要求与计划文档同目录存放；按四层文档体系惯例，定稿后可归档至 `docs/reviews/esp32/`（Layer ④） |

---

## 一、总体判断

架构方向（纯 C 驱动门面 + 协作式 FreeRTOS + 汇聚 PAL + SoC 能力矩阵）**正确**，与 `mcu-compat-plan.md` 双轴模型（轴 B 仿真拦截）及 ADR-0004/0065/0070 精神一致，`esp_idf_sources.cmake` 的 SSOT 清单设计也正确复用了 mcs51 先例。但有四类问题会直接导致返工：

1. **4 处 IDF 事实错误**（v6 未移除 legacy I2C、`esp_driver_*` 包含路径不存在、ESP32 LEDC 通道数、C6 I2C 端口数），其中两处会让能力矩阵与真实 `#if SOC_*` 分支行为不符；
2. **目录树 include 闭包严重不足**：缺 `sdkconfig.h`、`hal/`、`soc/gpio_num.h`、`freertos/FreeRTOSConfig.h`、`freertos/idf_additions.h` 等，M0 阶段即会编译失败；
3. **重复造轮子且与既有 SSOT 冲突**：FreeRTOS 调度应基于 `targets/common/wink_sim_scheduler`，资源 claim 违反 ADR-0065（会重新引入 Double-Claim Bug），能力矩阵与 ADR-0064 `pal_target_caps.h` 存在未决冲突；
4. **计划文档缺模板必选章节**（资源预算、风险登记、回滚、L0–L4 验收、验证命令），且现有链接（ADR 路径）全部指向不存在的 `docs/design/`。

**结论**：应保持"总纲（Layer-③ 索引）"定位，但状态需从 `📋 规划/就绪` 降为 `📋 草稿`；先补齐 P0 修订（见 §六），再拆分 M0–M3 子计划。

---

## 二、与 ESP-IDF v6.1 官方事实的偏差（硬伤）

> 取证环境：本机 v6.1 esp-idf 完整源码；以下"行号"指计划文档行号。

### 2.1 `v6 彻底移除了旧驱动头文件` —— 错误（计划 L139）

| 驱动 | v6.1 状态 | 证据 |
|---|---|---|
| legacy I2C | **仍存在，EOL** | `components/driver/i2c/include/driver/i2c.h` 头内 `#pragma message`：*"officially END-OF-LIFE (EOL) as of ESP-IDF v6.0 … WILL BE REMOVED in ESP-IDF v7.0"*；`components/driver/CMakeLists.txt` 在 `CONFIG_SOC_I2C_SUPPORTED` 下始终编译 `i2c/i2c.c` |
| legacy TWAI | 仍存在，EOL | `components/driver/twai/include/driver/twai.h` |
| legacy Touch Sensor | 仍存在，EOL | `components/driver/touch_sensor/esp32/include/driver/touch_sensor.h` |
| legacy Timer / ADC / DAC / RMT / I2S | v6.0 已移除 | 全组件树中 `include/driver/` 下已无 `timer.h/adc.h/dac.h/rmt.h/i2s.h` |

**修订要求**：
- 表述改为"**v5.x legacy 与新驱动共存 → v6.x legacy 标记 EOL 但保留（可编译，带告警） → v7.0 计划移除**"；
- 版本矩阵明确 v7 删除点与门面退场策略（例如 v7 配置下删除 `esp_i2c_legacy.c` 门面 TU）；
- 计划元数据"ESP-IDF v5.1+ ~ v6.0+"应更新为实测基线（仓库既有验证为 v5.1.3 LTS + v6.0.1，本机为 v6.1）。

### 2.2 `<esp_driver_gpio/gpio.h>` 式包含路径 —— 不存在（计划 L80-82、L151）

- `components/esp_driver_gpio/CMakeLists.txt`：`idf_component_register(... INCLUDE_DIRS include ...)`，`include/` 下仅有 `driver/*.h`；
- 全仓库官方 examples 中 `#include "esp_driver_gpio/…"` 出现次数为 **0**；
- v6 组件化的实质是 **CMake 依赖粒度拆分**，对用户包含路径 **零变化**（仍为 `driver/gpio.h`）。

**修订要求**：删除 `include/esp_driver_*` 转发层；如需兼容部分第三方库的非常规写法，只在 `docs/02-api-coverage-matrix.md` 里登记为 known-issue，不预置假路径。

### 2.3 SoC 能力数字错误（计划 L85、L87-91）

| SoC | 计划值 | v6.1 `soc_caps.h` 实测 | 判定 |
|---|---|---|---|
| esp32 | GPIO 40 / I2C 2 / **LEDC 16** | 40 / 2 / **`SOC_LEDC_CHANNEL_NUM=8`**（16 为硬件 HS+LS 总通道；IDF 暴露 8，`SOC_LEDC_TIMER_NUM=4`，另有 `SOC_LEDC_SUPPORT_HS_MODE`） | ❌ LEDC 错误 |
| esp32s3 | 49 / 2 / 8 | 49 / 2 / 8 | ✅ |
| esp32c3 | 22 / 1 / 6 | 22 / 1 / 6 | ✅ |
| esp32c6 | 31 / **1** / 6 | 31 / **`SOC_I2C_NUM=2`** / 6 | ❌ I2C 错误 |

**修订要求**：能力值必须"以 IDF 宏名为准逐条抄录"，并在 SoC manifest 中记录对应宏名（见 §四.3），防止再次凭硬件手册记忆写错。

### 2.4 仅用 `SOC_GPIO_PIN_COUNT` 做范围拦截 —— 不充分（计划 L132）

- esp32：`SOC_GPIO_VALID_GPIO_MASK` 排除 24/28/29/30/31，`SOC_GPIO_IN_RANGE_MAX=39`，`SOC_GPIO_OUT_RANGE_MAX=33`（即 34–39 仅输入）；
- esp32s3/s2：`VALID_GPIO_MASK` 排除 22–25；
- v6.1 中 `gpio_num_t` 定义在 `components/esp_hal_gpio/<chip>/include/soc/gpio_num.h`（含 `GPIO_NUM_NC`、input-only 注释、`GPIO_NUM_MAX`），**该头是"引脚编号与能力"的真实载体**；
- 计划树中的 `include/soc/gpio_pins.h` 被注释为"引脚编号与别名宏定义"，但 v6.1 esp32 版实际只有 `GPIO_MATRIX_CONST_ONE_INPUT/ZERO_INPUT` 两个常量，属误标。

**修订要求**：门面校验用 `GPIO_IS_VALID_GPIO / GPIO_IS_VALID_OUTPUT_GPIO` 同源语义（`hal/gpio_types.h` 内联宏），SoC 包同时提供 `soc_caps.h` 与 `gpio_num.h`。

### 2.5 其余措辞级偏差

| 计划位置 | 问题 | 建议 |
|---|---|---|
| L152 | `ESP_IDF_VERSION_VAL(5,3,0)` 表述不严谨；真实机制是定义 `ESP_IDF_VERSION_MAJOR/MINOR/PATCH` 再合成 `((major<<16)|(minor<<8)|patch)`；另需提供 `esp_get_idf_version()` 函数 | 默认值建议 v6（对齐仓库实测基线），支持 `-DWINK_ESP_IDF_VERSION=5` 回退 |
| L139 | "v5 引入 handle 体系（`i2c_master.h` 代替 `i2c.h`）"时间线不精确 | 新 I2C master 为 v5.2/5.3 引入；`gptimer` 为 v5.0 引入；`esp_driver_*` 组件拆分在 v5.x 已启动 |
| L42 | "ESP-IDF 100% C-ABI" | 驱动 API 成立；但 IDF 官方支持 C++ 用户码（`.cpp` + designated initializer + `extern "C"`），门面须保证 C++17 可编译 |
| L158 | "Pthreads 需 COOP/COEP，不可移植" | 结论对，但应引用仓库既有决策 ADR-0013/0014/0019（协作式调度 + Asyncify），避免自造理由 |

---

## 三、与仓库既有 SSOT / ADR 的冲突与遗漏

### 3.1 资源治理：计划写法重新引入 Double-Claim Bug（P0）

- 计划 L132/L219 要求门面调 `pal_resource_claim()`；
- **ADR-0065** 已裁决：物理硬件单元（GPIO/PWM/I2C_PORT/SPI_BUS/ADC_CHANNEL）在 **PAL Init 时自动 claim、Deinit 时自动 release**；DAL 及以上一律**禁止再次 claim**（该 ADR 的背景正是修复 Arduino shim 等三处重复 claim 导致的 `WINK_ERR_RESOURCE_BUSY`）。
- **修订**：门面只做 ① 参数合法性/范围校验；② 错误码翻译（`WINK_ERR_RESOURCE_BUSY → ESP_ERR_INVALID_STATE`、`WINK_ERR_INVALID_ARG → ESP_ERR_INVALID_ARG`）；③ 引脚能力检查（valid mask / in-out range）。

### 3.2 错误码映射策略缺失（P0）

IDF 与仓库错误码约定相反且不对称：`ESP_OK=0`、`ESP_FAIL=-1`、`ESP_ERR_*=正数(0x102…)` vs ADR-0001 的"负数=错误"。计划通篇未定义：

- 双向翻译表（建议单一函数 `esp_err_t esp_err_from_wink(wink_status_t)` + 反向内联）；
- `ESP_ERROR_CHECK` 的 abort 语义 → 仿真中应走 Fault/Trace 通道（ADR-0009/0012/0045），而非 host `abort()`；
- **不支持的 API 必须编译期显式报错（`#error` 或链接期 `undefined`）**，禁止静默 no-op（ADR-0012 合约诚实 + L230 红线的自洽性）。

### 3.3 能力 SSOT 冲突未决（P0，直接影响 SoC 矩阵方案）

- `pal/include/hal/pal_target_caps.h`（**ADR-0064**）在 wasm/host 下硬编码 `PAL_PWM_CHANNEL_MAX=8 / PAL_I2C_PORT_MAX=2 / PAL_GPIO_PIN_MAX=50`；
- 计划新增按 SoC 变化的能力（C3 I2C=1 等）。二者将产生"PAL 允许 2 个 I2C、门面按 SoC 只允许 1 个"的漂移；
- **修订**：动工前必须先决策并立 ADR——(A) 扩展 `pal_target_caps.h` 使仿真侧按 `WINK_ESP_IDF_SOC` 走 `chips/<soc>/soc_caps.h`（推荐，保持 SSOT）；或 (B) 门面能力从 PAL 能力反向派生（牺牲单 SoC 真实感）。

### 3.4 FreeRTOS 适配：忽略既有仿真调度器与硬预算（P0）

- 仓库已有 `targets/common/include/wink_sim_scheduler.h`（ADR-0013/0014）：
  - `sim_scheduler_register(func, arg, name, priority, core_id, stack_depth, out_id)` —— 参数刚好覆盖 `xTaskCreate` 语义；
  - `sim_scheduler_yield_timed / block / resume / next_wakeup_us`；
  - **`WINK_SIM_MAX_TASKS=8`、单 fiber 栈 ≥32KB**（8×32KB 已触及 ADR-0045 内存配额量级）；
  - wasm 侧硬性不变量：`pal_os_sleep_ms/us()` **内禁止推进虚拟时钟**（`osal/wasm/pal_osal_wasm.c:6-10`）；fiber 上下文中 `pal_os_sleep_ms` 会转发 `sim_scheduler_yield_timed`（同文件 L137-149）。
- 计划 L156-166 从零设计"Fiber/时间轮"，且使用仓库中不存在的 `co_yield/co_resume` 术语。
- **修订**：改写为"在既有单虚拟核调度器上做 FreeRTOS 语义映射"，并补齐 API 面：`configTICK_RATE_HZ=100`（v6.1 Kconfig 默认）、`portTICK_PERIOD_MS`、`pdMS_TO_TICKS`、`xTaskGetTickCount`、`vTaskDelayUntil`、25 级优先级（`configMAX_PRIORITIES=25`）、`xTaskCreatePinnedToCore`（声明在 `freertos/idf_additions.h`，单虚拟核下 core_id 校验后忽略、`xTaskGetCoreID` 返回 0）。

### 3.5 入口桥接范式漏引（P0）

mcs51 已确立框架接入运行时的标准契约：`wink_app_get_callbacks()` 返回 `wink_app_callbacks_t`（init/loop）并把用户 `main` 注册为 fiber（`frameworks/mcs51/src/mcs51_bridge.cpp:351`，ADR-0070）。计划的 `app_main_entry.c` 应实现同一契约；且文件命名违反《目录架构设计》§4.2——`<plat>_entry.c` 只属于 `targets/`，框架内建议 `esp_idf_runtime.c` + `esp_idf_bridge.c`。

### 3.6 既有 I2C 双版本设计未引用（P1）

`docs/decisions/core/0006-esp-idf-v6-i2c-compatibility.md`、`docs/zh/tech-designs/core/pal-i2c-v6-compatibility.md`、`2026-06-27-esp-idf-v6-i2c-compat-plan.md` 已完成 `pal_i2c_transfer` 契约、legacy/modern 冲突检测、真机验证。计划 §4.2 属平行再设计，应改为"引用既有设计 + 仅补门面→PAL 参数映射表"。

### 3.7 门禁声明无效（P1）

`wink-tools/tools/lint/rules/layering.yaml` 的 `layers` 仅覆盖 `bal / dal / apps / runtime / targets/wasm`——**`frameworks/**` 不在扫描范围**。mcs51 的解法是自带外部 lint pack（`frameworks/mcs51/tools/lint/lint_*.py`，ADR-0080）。计划 L230"必须执行 `winkcli lint --pack layering --pack api`"目前无法约束框架代码。

### 3.8 ADR 链接与编号错误（P1，机械性但全篇）

- 计划与 `docs/implementation-plans/esp32/00-README.md` 均写 `../../design/decisions/...`；`docs/design/` **不存在**，真实路径为 `docs/decisions/{core,unisim,tools,frontend}/`；
- "ADR-0040（资源独占治理）"错误：core 无 0040；资源治理是 **ADR-0065**；ADR-0040（unisim）是 Arduino JSON gate；
- 漏引：ADR-0030（ESP-IDF 永不自动安装——直接决定门面必须自带全部头）、ADR-0082（reset/重入语义 → `esp_restart`）、ADR-0067（I2C 超时恢复）、ADR-0042（headless 执行模式）、ADR-0064（能力 SSOT）、ADR-0035/0036（C++ 沙箱边界，即使门面纯 C 也应说明为何不适用）。

### 3.9 "轴 B"术语歧义（P1）

"轴 B = 仿真拦截层"来自 `mcu-compat-plan.md` 双轴模型；但 UniSim 3.0 现行 SSOT 中 **轴 B = 时间基**（`docs/zh/design/04-wasm-simulation/01-overview/02-axes-af.md`）。计划应显式消歧："轴 B（mcu-compat 双轴模型；≠ UniSim A~F 的 B 时间基）"。

---

## 四、目录树专项评审（重点）

> 评价基准：① 《WinkMicroOS 内核目录架构设计》§4（`docs/zh/design/02-wink-micro-os/03-directory-architecture.md`，第 171-181 行明确 `frameworks/<eco>/` 的子结构惯例）；② `frameworks/mcs51/` 先例；③ ESP-IDF v6.1 头文件真实依赖闭包。

### 4.1 现有树的七类问题

**（1）include 闭包严重不足，M0 即无法编译典型 IDF 代码**

实测官方 example 与头依赖链：

| 用户/头 | 真实依赖（v6.1） |
|---|---|
| `blink_example_main.c` | `freertos/FreeRTOS.h`、`freertos/task.h`、`driver/gpio.h`、`esp_log.h`、**`sdkconfig.h`** |
| `ledc_basic_example_main.c` | `driver/ledc.h`、`esp_err.h`、**`sdkconfig.h`**、`esp_pm.h` |
| `driver/gpio.h` | `esp_err.h`、`esp_intr_alloc.h`、`soc/soc_caps.h`、`hal/gpio_types.h`、`esp_rom_gpio.h`、`driver/gpio_etm.h` |
| `driver/uart.h` | `freertos/FreeRTOS.h`、`freertos/queue.h`、`hal/uart_types.h`、`esp_check.h`、`soc/soc_caps.h` |
| `driver/ledc.h` | `hal/ledc_types.h`、`esp_intr_alloc.h`、`driver/ledc_etm.h` |

计划树**缺失**（按影响排序）：`sdkconfig.h`（几乎全量出现）、`esp_attr.h`（`IRAM_ATTR`）、`esp_check.h`、`esp_intr_alloc.h`、`esp_heap_caps.h`、`esp_random.h`、`esp_chip_info.h`、`hal/`（`gpio_types/ledc_types/uart_types/i2c_types`）、`soc/gpio_num.h`、`freertos/FreeRTOSConfig.h`、`freertos/idf_additions.h`（`xTaskCreatePinnedToCore`）、`freertos/timers.h/event_groups.h/stream_buffer.h`。

**（2）`include/esp_driver_*/` 是错误抽象**（见 §2.2）——整块删除。

**（3）`soc/` 一名两义，且"包含路径首位"机制技术上不成立**

- `soc/<chip>/soc_caps.h`（能力数据）与 `include/soc/soc_caps.h`（用户可见路径）同名同层；
- 计划 L129-130 想用 `-I soc/${WINK_ESP_TARGET}/` 让 `#include "soc/soc_caps.h"` 命中所选芯片——但芯片目录下没有 `soc/` 相对路径，除非改为 `chips/<soc>/include/soc/...`，否则只能退化为宏分支，方案自相矛盾。

**（4）不符合仓库框架目录惯例（对照 mcs51）**

| 能力 | mcs51 先例 | 计划 | 判定 |
|---|---|---|---|
| 源/头清单 SSOT | `mcs51_sources.cmake` | `esp_idf_sources.cmake` | ✅ |
| 芯片/SoC 包 | `chips/<family>/` + CMake 自动发现 + `tools/manifests/chips/*.yaml` 事实源 | `soc/<chip>/` 硬编码枚举 | ❌ 建议对齐 |
| 工具与门禁 | `tools/{manifests,lint,sdcc_gate,run_*_headless_evidence.ps1}` | 无 | ❌ |
| 分层测试 | `test/{core,chips,samples,wasm}` | 单个 `test/` 平铺 3 文件 | ❌ |
| 框架内文档 | `docs/01-architecture-and-governance-guide.md` | 仅 `README.md` | 🟡 |
| CMake 纪律 | `ESP_PLATFORM` 早退、`STATIC EXCLUDE_FROM_ALL`、inject 包 | 未声明 | ❌ |

**（5）头/源不对称与职责错置**

- `driver/gptimer.h` 在树中，`src/drivers/` 无 `esp_gptimer.c`；
- `nvs.h/nvs_flash.h` 在树中，无任何 NVS 门面实现文件（M0–M3 也未列任务）；
- `esp_timer.c` 归入 `drivers/`（实为系统服务），且映射目标 `pal_time_*` **不存在**（真实为 `pal_os_get_us/get_ms/sleep_ms`）；
- M3 只列 esp32/s3/c3（漏 C6），与树、元数据不一致。

**（6）缺"覆盖矩阵 + 非目标"文档**

IDF 应用面远超这 12 个头（`esp_wifi.h/esp_event.h/esp_lcd/esp_netif/esp_vfs/spiffs/console/esp_pm/esp_sleep`…）。没有 must/should/out-of-scope 三档矩阵与"不支持即显式报错"策略，M0–M3 范围必然失控，也无法对齐 ADR-0003 的保真声明边界。

**（7）测试与仓库接轨不足**

- host 测试需接入中央注册（`wink-micro-os/test/CMakeLists.txt`），harness 与 `esp_idf_sources.cmake` 同源；
- 无 `test/wasm/`（emcc + Node，复用 `add_wink_wasm_mcs51_test.cmake` 模式）；
- **缺最有价值的 compile-only 语料测试**：把 `examples/get-started/blink`、`peripherals/gpio/generic_gpio`、`peripherals/ledc/ledc_basic`、`peripherals/i2c/i2c_basic` 的 main 源文件**原文**过门面编译（这是"用户代码零改动"承诺的唯一硬证据）。

### 4.2 建议修订树

```
wink-micro-os/frameworks/esp_idf/
├── CMakeLists.txt                  # ESP_PLATFORM 早退；host/wasm STATIC EXCLUDE_FROM_ALL
│                                   # （esp_idf 与 mcs51 同为 sim-only，真机固件零增量）
├── esp_idf_sources.cmake           # 源/头清单 SSOT（host/wasm harness 复用）✅ 保留
├── README.md                       # 拦截原理 + 支持矩阵 + 不支持清单
├── docs/
│   ├── 01-architecture-and-governance-guide.md
│   └── 02-api-coverage-matrix.md   # must/should/out-of-scope + 错误码映射表 + 版本存在性表
├── include/                        # 虚拟 include 根 = 用户 #include 的确切路径
│   ├── sdkconfig.h                 # ★ CONFIG_IDF_TARGET_*、CONFIG_FREERTOS_HZ=100、日志级别
│   ├── esp_err.h  esp_log.h  esp_attr.h  esp_check.h  esp_system.h
│   ├── esp_timer.h  esp_intr_alloc.h  esp_heap_caps.h  esp_random.h
│   ├── esp_chip_info.h  esp_idf_version.h  esp_task_wdt.h
│   ├── nvs.h  nvs_flash.h
│   ├── freertos/
│   │   ├── FreeRTOS.h  FreeRTOSConfig.h  task.h  queue.h  semphr.h
│   │   ├── timers.h  event_groups.h  stream_buffer.h
│   │   └── idf_additions.h         # xTaskCreatePinnedToCore / xTaskGetCoreID
│   ├── driver/                     # gpio uart ledc i2c i2c_master spi_master gptimer
│   │                               #   （按覆盖矩阵扩展 adc/rmt/mcpwm…）
│   ├── hal/                        # gpio_types.h / ledc_types.h / uart_types.h / i2c_types.h
│   └── soc/
│       ├── soc_caps.h              # 按 WINK_ESP_IDF_SOC 转发 chips/<soc>/
│       └── gpio_num.h              # 同源转发（GPIO_NUM_NC / GPIO_NUM_x / GPIO_NUM_MAX）
├── chips/                          # SoC 能力包（对齐 mcs51 chips/ 惯例 + 自动发现）
│   ├── esp32/    include/soc/soc_caps.h, include/soc/gpio_num.h
│   ├── esp32s3/    include/soc/...
│   ├── esp32c3/    include/soc/...
│   └── esp32c6/    include/soc/...
├── src/
│   ├── esp_idf_runtime.c           # wink_app_get_callbacks + app_main→fiber 注册（ADR-0070 同构）
│   ├── esp_idf_bridge.c            # 生命周期 / esp_restart / esp_get_idf_version
│   ├── core/                       # esp_err.c(映射表) esp_log.c esp_system.c nvs 门面
│   ├── freertos/                   # freertos_task.c queue.c semphr.c idf_additions.c
│   │                               #   （全部桥接 sim_scheduler_*，不新建调度器）
│   └── drivers/                    # esp_gpio.c esp_uart.c esp_ledc.c esp_i2c_legacy.c
│                                   #   esp_i2c_master.c esp_spi.c esp_gptimer.c
├── tools/
│   ├── manifests/socs/*.yaml       # SoC 事实唯一源（宏名→值，对齐 mcs51 tools/manifests）
│   ├── lint/                       # 本框架门禁（ADR-0080 外部 pack）或明确扩展 layering.yaml
│   └── run_esp_idf_headless_evidence.ps1
└── test/
    ├── core/                       # Unity host 单测（接入中央 test/CMakeLists.txt）
    ├── samples/                    # 每门面最小闭环（Blinky / 多任务 / I2C 双版本 / PWM 定点）
    ├── corpus/                     # ★ IDF examples 原文 compile-only 语料（保真门禁）
    └── wasm/                       # emcc + Node harness
```

### 4.3 与目录树配套的三个前置决策（建议先立 ADR）

1. **能力 SSOT 归属**：`chips/<soc>/` 与 `pal_target_caps.h` 的关系（§3.3）——决定 M3 全部工作量形态；
2. **版本轴策略**：不是"两套源码"，而是"共享语义内核 + 两个薄门面 TU + 版本→API 存在性表"，含 v7 删除点；
3. **门面/PAL 边界**：门面只做翻译/校验/映射；claim、时钟、物理一律在 PAL；包含路径遵循 ADR-0068（`hal/...` 前缀）。

---

## 五、计划文档完整性（对照 `00-IMPLEMENTATION-PLAN-TEMPLATE.md`）

| 模板章节 | 计划现状 | 判定 |
|---|---|---|
| §1 元数据 | 缺 `工具链/SDK 版本`、`关联设计规范`、`前置依赖计划`、`负责人`；ADR 链接断链、编号错 | ❌ |
| §2.2/2.3 量化目标与成功指标 | 只有里程碑一行验收，无指标表（如 corpus 通过率、wasm 0 告警、任务数上限） | ❌ |
| §3.1/3.2 文件变更清单与接口影响 | 无 | ❌ |
| §3.4 系统资源与并发约束 | 无（`WINK_SIM_MAX_TASKS=8`、32KB×N 栈、wasm 内存配额 ADR-0045 必须评估） | ❌ |
| §4 依赖与风险登记册 | 无（外部依赖至少含：unisim JS 桥、wink.py 构建集成、v6.1 CI 环境） | ❌ |
| §5 关键路径/冲突矩阵 | 只有 Gantt，无关键路径与文件冲突矩阵 | 🟡 |
| §6 Task 级步骤与验证命令 | M0–M3 全为一行 bullet，无精确变更与可执行验证 | ❌ |
| §7 L0–L4 验收 | 无；L0 至少应含 host `wink.py test`、wasm 构建 0 告警、corpus 编译 | ❌ |
| §8 回滚方案 | 无（建议：CMake 开关摘除 + 框架目录整体 revert 两层） | ❌ |
| §9 参考资料 | 无官方迁移指南/ADR/技术设计链接 | ❌ |
| 变更日志/问题记录 | 无 | ❌ |

另：按模板注记，本文件定位"总纲"，应明确它派生的子计划（`M0`~`M3` 各自独立 plan 文件名）并保持索引；在补齐上述内容前，状态宜为 `📋 草稿`。

---

## 六、修订优先级清单

| 优先级 | 修订项 | 落点 |
|---|---|---|
| **P0** | 更正 4 处 IDF 事实（§2.1–2.4）与两处措辞（§2.5） | 计划 §3.1 树注释、§4.1/4.2、元数据 |
| **P0** | 删除 `include/esp_driver_*/`；补齐 `sdkconfig.h/hal/soc/gpio_num.h/FreeRTOSConfig.h/idf_additions.h` 等闭包 | 计划 §3.1 |
| **P0** | 资源 claim 改为"PAL 独占 + 门面错误码翻译"（ADR-0065）；补错误码映射策略（ADR-0001） | 计划 §4.1、§6 红线 |
| **P0** | FreeRTOS 适配改写为 `sim_scheduler_*` 语义映射；补 tick/优先级/pinned-core/栈预算 | 计划 §4.3、M1 |
| **P0** | 能力 SSOT 冲突决策（ADR-0064 vs SoC 矩阵），先立 ADR 再动工 | 新增 ADR + 计划前置依赖 |
| **P1** | 目录树按 §4.2 重构（chips/tools/docs/test 分层 + 自动发现）；入口命名 `esp_idf_runtime.c` | 计划 §3.1 |
| **P1** | 新增覆盖矩阵与非目标（must/should/out-of-scope + 显式报错策略） | 新增 `docs/02-api-coverage-matrix.md` |
| **P1** | 修正全部 ADR 链接与编号；补引 ADR-0006/0012/0030/0042/0045/0064/0065/0067/0082/0080 | 计划全文 + `00-README.md` |
| **P1** | 门禁落地：`tools/lint/` 外部 pack 或扩展 `layering.yaml`；补 corpus compile-only 测试 | 计划 M3 + §6 红线 |
| **P2** | 补模板必选章节（风险/资源/回滚/L0–L4/验证命令/变更日志）；拆分 M0–M3 子计划 | 计划全文 |

---

## 附录：取证清单（可复现）

**ESP-IDF v6.1（`D:\software\embedded-tools\esp-idf\.espressif\v6.1\esp-idf`）**

- `components/driver/i2c/include/driver/i2c.h`（EOL 告警原文，移除计划 v7.0）
- `components/driver/CMakeLists.txt`（legacy i2c/touch/twai 仍编入）
- `components/esp_driver_gpio/CMakeLists.txt`（`INCLUDE_DIRS include` → `driver/gpio.h`）
- `components/soc/<chip>/include/soc/soc_caps.h`（`SOC_GPIO_PIN_COUNT / SOC_I2C_NUM / SOC_LEDC_CHANNEL_NUM / SOC_GPIO_VALID_GPIO_MASK / *_RANGE_MAX`）
- `components/esp_hal_gpio/<chip>/include/soc/gpio_num.h`、`components/esp_hal_gpio/include/hal/gpio_types.h`
- `components/freertos/config/include/freertos/FreeRTOSConfig.h`（`configMAX_PRIORITIES=25`）
- `components/freertos/Kconfig`（`configTICK_RATE_HZ` 默认 100）
- `components/freertos/esp_additions/include/freertos/idf_additions.h`（`xTaskCreatePinnedToCore`）
- `examples/get-started/blink`、`examples/peripherals/{gpio/generic_gpio,ledc/ledc_basic,i2c/i2c_basic}`（包含面实测）

**仓库**

- `docs/zh/design/02-wink-micro-os/03-directory-architecture.md` §4/§4.2
- `docs/zh/tech-designs/mcs51/mcu-compat-plan.md` §0（双轴模型）
- `docs/zh/design/04-wasm-simulation/01-overview/02-axes-af.md`（UniSim 轴 B=时间基）
- `docs/decisions/core/{0001,0006,0012,0030,0042?,0045?,0064,0065,0067,0070,0080,0082}.md`、`docs/decisions/tools/0043-*.md`、`docs/decisions/unisim/{0013,0014,0019,0042,0045,0064}.md`
- `wink-micro-os/pal/include/hal/pal_target_caps.h`、`pal_resource.h`、`pal_i2c.h`、`pal_pwm.h`
- `wink-micro-os/targets/common/include/wink_sim_scheduler.h`、`wink-micro-os/osal/wasm/pal_osal_wasm.c`
- `wink-micro-os/frameworks/mcs51/{CMakeLists.txt,mcs51_sources.cmake,tools/,test/,docs/}`、`src/mcs51_bridge.cpp`
- `wink-tools/tools/lint/rules/layering.yaml`（无 `frameworks/**` 层）
- `docs/implementation-plans/00-IMPLEMENTATION-PLAN-TEMPLATE.md`

---

## 评审签署

| 角色 | 结论 | 日期 |
|---|---|---|
| 架构评审（AI Agent，资深嵌入式视角） | **修订后可进入分步实施**；P0 未闭环前不建议开工 M0 | 2026-09-22 |
