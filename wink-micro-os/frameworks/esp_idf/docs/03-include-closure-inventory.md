# ESP-IDF 仿真拦截层 Include 闭包追踪清单 (03-include-closure-inventory)

> **版本**：v1.1  
> **适用里程碑**：M1 (FreeRTOS 调度器与并发原语 Shim)  
> **设计依据**：Task M1-1 编译驱动增量 include 闭包策略

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

---

## 3. M1 新增头文件闭包登记表 (FreeRTOS 调度与同步)

| 头文件路径 | 官方组件来源 | 桩策略 | 驱动语料 / 依赖项 | 说明 |
|:---|:---|:---:|:---:|:---|
| `include/freertos/task.h` | `freertos` | 全量任务声明扩展 | Task 生命周期与延时 | 扩展创建/挂起/删除/Tick/状态/自删/切出等完整 API |
| `include/freertos/queue.h` | `freertos` | 同步声明桩 | 队列通信 (Tier-B UART 等) | 导出 `xQueueCreate/Send/Receive/Peek` 与 `FromISR` 全家桶 |
| `include/freertos/semphr.h` | `freertos` | 同步声明桩 | 互斥量与信号量 | 导出 Mutex/Binary/Counting 与 `FromISR`，Recursive 标废弃 |
| `include/freertos/event_groups.h` | `freertos` | 同步声明桩 | 24-bit 事件组 | 导出 `xEventGroupWaitBits/SetBits/ClearBits` 与广播 |
| `include/freertos/timers.h` | `freertos` | Fail-Loud 声明桩 | 软件定时器 | 声明完整原型，运行时一律 Fail-Loud 报错返回 (递延 M2+) |

---

## 4. M2 新增头文件闭包登记表 (LEDC, I2C, UART, GPTimer, SPI, NVS)

| 头文件路径 | 官方组件来源 | 桩策略 | 驱动语料 / 依赖项 | 说明 |
|:---|:---|:---:|:---:|:---|
| `include/hal/ledc_types.h` | `hal` | 原生枚举与类型 | `driver/ledc.h` | LEDC 模式/定时器/通道/时钟/占空比位宽枚举与配置结构体 |
| `include/driver/ledc.h` | `driver` | 驱动门面声明 | `corpus_ledc_basic` | 导出 LEDC 核心配置、占空比设置、渐变与查询原型 |
| `include/hal/i2c_types.h` | `hal` | 原生枚举与类型 | `driver/i2c*.h` | I2C 端口/主从模式/寻址位数/ACK 模式枚举 |
| `include/driver/i2c_types_legacy.h` | `driver` | 历史类型声明 | `driver/i2c.h` | 历史配置体 `i2c_config_t` 与句柄 `i2c_cmd_handle_t` |
| `include/driver/i2c.h` | `driver` | 历史驱动门面 (Deprecated) | 历史 I2C 语料与设备驱动 | 历史命令链装配与折叠执行原型（带有迁移建议警示） |
| `include/driver/i2c_types.h` | `driver` | 现代类型声明 | `driver/i2c_master.h` | 现代总线句柄 `i2c_master_bus_handle_t` 与器件句柄声明 |
| `include/driver/i2c_master.h` | `driver` | 现代总线驱动门面 | `corpus_i2c_basic` | 导出现代 master bus 注册、器件挂载与收发事务原型 |
| `include/hal/uart_types.h` | `hal` | 原生枚举与类型 | `driver/uart.h` | UART 端口/数据位/停止位/校验位/流控/事件类型枚举 |
| `include/driver/uart.h` | `driver` | 驱动门面声明 | UART 通信语料 | 导出配置/引脚设置/驱动安装/读写缓冲与事件队列原型 |
| `include/driver/gptimer_types.h` | `driver` | 类型声明 | `driver/gptimer.h` | GPTimer 句柄、时钟源、计数方向、事件数据与回调定义 |
| `include/driver/gptimer.h` | `driver` | 驱动门面声明 | 通用定时器语料 | 导出通用定时器创建/使能/启动/计数值读写/警报配置原型 |
| `include/hal/spi_types.h` | `hal` | 原生枚举与类型 | `driver/spi_*.h` | SPI 主机标识 `spi_host_device_t`、DMA 通道与采样模式 |
| `include/driver/spi_common.h` | `driver` | 总线驱动门面 | `driver/spi_master.h` | SPI 总线初始化 `spi_bus_initialize` 与释放原型 |
| `include/driver/spi_master.h` | `driver` | 主机驱动门面 | SPI 通信语料 | SPI 器件挂载 `spi_bus_add_device` 与数据收发事务原型 |
| `include/nvs.h` | `nvs_flash` | 存储门面声明 | 配置存储语料 | 错误码、句柄类型及全部原生类型 `nvs_set/get_*` 声明 |
| `include/nvs_flash.h` | `nvs_flash` | 分区管理门面 | Flash 引导语料 | 导出 `nvs_flash_init`, `nvs_flash_erase`, `deinit` 原型 |
| `chips/esp32/include/soc/soc_caps.h` | `soc/esp32` | 芯片能力增量扩展 | 驱动门面能力断言 | 增补 `SOC_LEDC_*`, `SOC_I2C_*`, `SOC_UART_*`, `SOC_GPTIMER_*`, `SOC_SPI_*` |

---

## 5. Tier-B 官方语料 Stub 闭包头文件清单 (M2)

为支持 upstream Tier-B 语料（`components/driver/test_apps/legacy_i2c_driver/main/test_i2c.c`）在不修改原文前提下完成编译闭环，建立以下测试专属 stub 闭包头文件：

| 头文件路径 (相对于 corpus 目录) | 对应 upstream 组件 | 闭包职责与内容 |
|:---|:---|:---|
| `include/sdkconfig.h` | `sdkconfig` | 定义 `CONFIG_IDF_TARGET_ESP32 1`, `SOC_HP_I2C_NUM 2`, `SOC_I2C_SUPPORT_SLAVE 1` |
| `include/unity_config.h` | `unity` | 包含 `sdkconfig.h`，提供编译期配置预置 |
| `include/test_utils.h` | `unity/test_utils` | `TEST_CASE`, `TEST_CASE_MULTIPLE_DEVICES`, `TEST_ESP_OK`, 信号与内存泄漏打桩 |
| `include/hal/i2c_periph.h` | `soc/hal` | `i2c_dev_t`（`ctr`, `fifo_conf`, `rxfifo_st`）、`I2C0/1` 外部符号与 `i2c_periph_signal` |
| `include/soc/uart_struct.h` | `soc` | `uart_dev_t` 结构体与 `UART1` 外部符号 |
| `include/hal/uart_periph.h` | `hal` | `UART_PERIPH_SIGNAL`, `SOC_UART_PERIPH_SIGNAL_RX` 宏映射 |
| `include/hal/uart_ll.h` | `hal` | `uart_ll_enable_bus_clock`, `uart_ll_get_rxd_edge_cnt` 等硬件底座 inline 模拟打桩 |
| `include/esp_private/periph_ctrl.h` | `esp_hw_support` | `PERIPH_RCC_ATOMIC` 宏及外设时钟启停打桩 |
| `include/esp_private/gpio.h` | `esp_driver_gpio` | `gpio_func_sel` 与 `PIN_FUNC_GPIO` 宏映射 |



