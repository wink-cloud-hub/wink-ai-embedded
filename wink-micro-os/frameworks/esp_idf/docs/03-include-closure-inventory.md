# ESP-IDF 仿真拦截层 Include 闭包追踪清单 (03-include-closure-inventory)

> **版本**：v1.0  
> **适用里程碑**：M0 (最小 GPIO 闭环与 Tier-A Blink 语料)  
> **设计依据**：Task M0-2 (T-004) 编译驱动增量 include 闭包策略

---

## 1. 设计原则与约定

1. **编译驱动增量 (Compilation-Driven Incremental Closure)**：
   杜绝盲目全量铺设 ESP-IDF 官方庞大的头文件树。所有桩头均以官方示例（Tier-A Blink）编译过程中的传递 include 依赖树实测为准逐步补全。
2. **前缀式自包含约定 (Prefix Self-Containment)**：
   本仓桩头之间一律采用前缀式自包含（例如 `#include "freertos/portmacro.h"`），不复刻官方多组件 include 根路径拆分，以维持最小侵入性与宿主环境一致性。
3. **合约诚实 (ADR-0012)**：
   桩头分为四类策略：宏消解、类型映射、透传垫片、声明桩。严禁提供静默空实现函数。

---

## 2. M0 头文件闭包登记表

| 头文件路径 | 官方组件来源 | 桩策略 | 驱动语料 / 依赖项 | 说明 |
|:---|:---|:---:|:---:|:---|
| `include/sdkconfig_base.h` | `esp_hw_support` | 基础平台宏 | 全部语料 | 基础目标宏（禁定义 `ESP_PLATFORM`） |
| `include/esp_attr.h` | `esp_common` | 宏消解为空 | 全部语料 | 消解 `IRAM_ATTR`, `DRAM_ATTR` 等 |
| `include/esp_compiler.h` | `esp_common` | 工具宏 | 全部语料 | 提供 `likely`, `unlikely` 等工具宏 |
| `include/esp_err.h` | `esp_common` | 类型与常量映射 | 全部语料 | 定义 `esp_err_t`, `ESP_OK`, `ESP_FAIL` 等错误码 |
| `include/esp_check.h` | `esp_common` | 断言宏映射 | 全部语料 | `ESP_ERROR_CHECK` 转调 `wink_runtime_raise_fault` |
| `include/esp_log.h` | `log` | 门面转调 | `blink_example_main.c` | 转调 Wink PAL 日志 (`pal_log_*`) |
| `include/esp_log_*.h` (9个分片) | `log` | 官方透传分片 | `esp_log.h` | 补全 level/color/buffer/timestamp 等官方分解头 |
| `include/esp_private/log_attr.h` | `log` | 私有属性消解 | `esp_log.h` | 消解日志属性宏 |
| `include/esp_system.h` | `esp_system` | 声明桩与转调 | 系统控制 | 导出 `esp_restart`, `esp_get_idf_version` 等 |
| `include/esp_timer.h` | `esp_timer` | 声明桩 | 时间查询 | 时间系统查询桩 |
| `include/esp_random.h` | `esp_hw_support` | 声明桩与转调 | 随机数 | 导出 `esp_random()` |
| `include/esp_chip_info.h` | `esp_hw_support` | 声明桩 | 芯片信息 | 芯片基础信息查询原型 |
| `include/esp_idf_version.h` | `esp_common` | 版本定义宏 | 版本检测 | 设定 `ESP_IDF_VERSION_VAL(6, 1, 0)` |
| `include/esp_pm.h` | `esp_pm` | 宏消解桩 | 电源管理 | 电源管理 no-op 桩 |
| `include/esp_intr_alloc.h` | `esp_hw_support` | 声明桩 | 中断分配 | 中断注册相关桩声明 |
| `include/esp_rom_gpio.h` | `esp_rom` | 宏与声明桩 | ROM 操作 | ROM 引导与引脚操作声明 |
| `include/esp_rom_sys.h` | `esp_rom` | 宏与声明桩 | ROM 操作 | ROM 系统基础操作声明 |
| `include/esp_task_wdt.h` | `esp_system` | 声明桩 | 看门狗 | 看门狗管理原型 |
| `include/freertos/FreeRTOS.h` | `freertos` | 基础定义 + 透传 | 全部 FreeRTOS 语料 | 基础宏并在尾部无条件包含 `idf_additions.h` |
| `include/freertos/FreeRTOSConfig.h` | `freertos` | 平台常量冻结 | `FreeRTOS.h:63` | 冻结 `configTICK_RATE_HZ=100`, `configMAX_PRIORITIES=25` |
| `include/freertos/projdefs.h` | `freertos` | 状态常量定义 | `FreeRTOS.h:66` | 定义 `pdTRUE`, `pdFALSE`, `pdPASS`, `pdFAIL` |
| `include/freertos/portable.h` | `freertos` | 中转包含 | `FreeRTOS.h:69` | 转含 `freertos/portmacro.h` |
| `include/freertos/portmacro.h` | `freertos` | 基础类型与常量 | `FreeRTOS.h` | 定义 `BaseType_t`, `TickType_t`, `portTICK_PERIOD_MS` |
| `include/freertos/task.h` | `freertos` | 最小声明桩 | `blink_example_main.c` | 导出 `vTaskDelay`, `vTaskDelayUntil` 原型 |
| `include/freertos/idf_additions.h` | `freertos` | 扩展声明桩 | `FreeRTOS.h` | 乐鑫 FreeRTOS 扩展定义 |
| `include/led_strip.h` | 外部托管组件 | 声明级 stub | `blink_example_main.c` | 避免未启用分支时报缺失头文件 |
| `include/driver/gpio.h` | `driver` | 驱动门面 | `blink_example_main.c` | `gpio_config`, `gpio_set_direction`, `gpio_set_level`, `gpio_get_level`, `gpio_reset_pin` |
| `include/hal/gpio_types.h` | `hal` | 类型映射 | `driver/gpio.h` | `gpio_num_t`, `gpio_mode_t`, `gpio_config_t` 等 |
| `include/soc/soc_caps.h` | `soc` | 中转层 | 硬件断言 | 中转至 `chips/${WINK_ESP_TARGET}/include/soc/soc_caps.h` |
| `include/soc/gpio_num.h` | `soc` | 中转层 | 硬件引脚定义 | 中转至 `chips/${WINK_ESP_TARGET}/include/soc/gpio_num.h` |
| `chips/esp32/include/soc/soc_caps.h` | `soc/esp32` | 芯片原生能力宏 | GPIO 门面断言 | 经典 ESP32 引脚与输出能力掩码原式 (ADR-0085) |
| `chips/esp32/include/soc/gpio_num.h` | `soc/esp32` | 原生引脚枚举 | GPIO 驱动 | 官方经典 ESP32 引脚枚举 `GPIO_NUM_0` ~ `GPIO_NUM_39` |
