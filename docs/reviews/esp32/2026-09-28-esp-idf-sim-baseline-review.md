# ESP-IDF 仿真基线审计（H0）

- 审计日期：2026-09-28
- 嵌入式仓 HEAD：78b6c4a03c9bac4865a86624daa7d5a5dfe203fc
- wink-ai 工具仓 HEAD：36eb90fff16120f42f69fbde491739092a26f2e7
- vendored SDK：ESP-IDF v6.1，sdk_sha fff9895c；manifest 另含 v5.1.3 配置。
- 基线仅表示当前 Windows MinGW Host 环境；编译、行为运行、官方语义差分和真机构建分开记证。

## 仓库与工具基线

嵌入式仓 HEAD 之外存在既存未提交修改：.github/workflows/esp_idf_ci.yml、docs/implementation-plans/esp32/00-README.md、frameworks/esp_idf 的 target/queue/FreeRTOS/NVS/Wasm/coverage 文件及 wink-micro-os/test/CMakeLists.txt。均保留且未纳入 HEAD 结论。工具仓 HEAD 为 36eb90fff16120f42f69fbde491739092a26f2e7，另有既存未跟踪的 unisim/docs/plans/... 与 unisim/docs/real-model/...，未触碰。

工具版本：Python 3.11.15；CMake 4.4.3；MinGW GCC 16.2.0；GNU Make 4.4.1；Emscripten 6.0.9；Node 22.23.2。idf.py 缺失，当前环境不能做真实 ESP-IDF 构建。STANDARD cache 为 host/esp32/STANDARD/MinGW Makefiles；LITE 与 PRO 有独立 cache。

交付计划已同步到 docs/implementation-plans/esp32/2026-09-28-esp-idf-simulation-hardening-plan.md，SHA-256 65743D06C991E8215CA17F86A0162D1A1E8C314E7948816D0CB6700AC85E2138。覆盖前旧副本备份：C:\Users\77174\Documents\Codex\2026-09-28\repository-plan-before-sync.md。

## CTest 基线

build_esp_hardening_standard 共发现 243 项。四个计划标签按序列执行，合计 74/74 通过，0 失败、0 跳过。

| 标签 | 发现 | 通过 | 失败 | 跳过 |
|---|---:|---:|---:|---:|
| esp_idf | 23 | 23 | 0 | 0 |
| esp_idf_corpus | 8 | 8 | 0 | 0 |
| esp_idf_wasm | 33 | 33 | 0 | 0 |
| esp_idfv61_vendor | 10 | 10 | 0 | 0 |

esp_idf_wasm 是 Wasm 编译检查，不代表运行。当前 Host 运行证据含 blink run 和一个通用 headless replay；无逐 API Wasm 输出 oracle。Corpus/vendor 通过不证明与上游行为等价。WINK_BUILD_WASM_TESTS 关闭时 CMake 不注册 Wasm 编译测试；本 STANDARD cache 下 33 项均已运行。

## CI 与未验证门禁

当前 workflow 对固定 SHA 的私有 wink-tools 源码、repo variable 和只读 token 缺失采取 fail-closed；fork PR 无 token，需可信环境复跑。工作流矩阵是每 OS 6 个配对，共 12 job；24 组合计划尚未落地。Actions secrets、required checks、Linux/Windows 远端运行和 85% coverage 均无法从本地读取，状态是未验证，归属 A/H8 与 F/H8。

## 启动、复位与静态状态所有权

调用路径：框架初始化注册 app_main_trampoline；其进入用户 app_main；FreeRTOS facade 注册任务并连接 PAL scheduler；PAL 推进任务与虚拟时间。GPIO/总线 facade 下沉 PAL；Wi-Fi/Netif、event、MQTT/HTTP、NimBLE 各持有静态状态。

| 状态域 | 状态/所有者 | 入口 | 发现与任务 |
|---|---|---|---|
| 框架/app_main | esp_idf_runtime.c: s_app_main_slot、s_framework_inited | ensure_framework_ready; framework_init | ready 在 pool reset 完成前已置 true；重复 framework_init 会 reset pool：B1/B2 |
| FreeRTOS pools | 各 freertos_*.c 静态池、waiter/task 槽 | esp_freertos_pools_reset | 全量清池、无 generation：H2/H4/H6 |
| Event loop | esp_event.c handler slots | esp_event_loop_sim_reset | bridge 未统一调用；槽位复用与异步语义：H2/H5 |
| GPIO/LEDC/I2C/UART/GPTimer/SPI | 驱动静态槽/缓存 | esp_peripherals_reset | 句柄复用和 reset 次序：H2/H6 |
| Heap/NVS | heap tracker、KV/handle、沙箱镜像 | heap reset; nvs deinit/reset | 当前 API 矩阵关于 libc free/精确水位需 D1/H3 校正；B4 隔离 |
| Wi-Fi/Netif | 状态、token、虚拟接口 | 各自私有 reset | unified reset 未调用：H2 |
| MQTT/HTTP | 客户端池、回调、task/token | 各自私有 reset | unified reset 未调用；MQTT 直接回调：H2/H5 |
| NimBLE | GATT/GAP/连接池 | deinit/private reset | unified reset 未调用：H2/H6 |

esp_restart 设置 pending；调度器内让出并防止返回，无调度器时落空返回；公开头文件没有 noreturn。bridge reset 未形成网络→协议栈→event loop→外设→任务池的统一拓扑，对应 B3。

## API × 语义 × 场景 × 环境证据

下表逐行覆盖 docs/02-api-coverage-matrix.md 的人工 API 覆盖表。Host 测试按模块关联，不表示行内每个 API 单独覆盖。所有未验证格已分配至后续任务。

| API 行 | 声明/人工状态 | 实现源 | Host 行为测试 | Wasm 编译 | Wasm 运行 | 官方对照 | 真机 IDF | 待办 |
|---|---|---|---|---|---|---|---|---|
| `gpio_config` | `driver/gpio.h` / ✅ 支持 | src/drivers/esp_gpio.c / src/core/esp_system.c | test_esp_gpio, test_esp_soc_matrix | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gpio_set_direction` | `driver/gpio.h` / ✅ 支持 | src/drivers/esp_gpio.c / src/core/esp_system.c | test_esp_gpio, test_esp_soc_matrix | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gpio_set_level` | `driver/gpio.h` / ✅ 支持 | src/drivers/esp_gpio.c / src/core/esp_system.c | test_esp_gpio, test_esp_soc_matrix | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gpio_get_level` | `driver/gpio.h` / ⚠️ 降级支持 | src/drivers/esp_gpio.c / src/core/esp_system.c | test_esp_gpio, test_esp_soc_matrix | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gpio_reset_pin` | `driver/gpio.h` / ✅ 支持 | src/drivers/esp_gpio.c / src/core/esp_system.c | test_esp_gpio, test_esp_soc_matrix | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gpio_set_pull_mode` / `gpio_pullup_en_dis` / `gpio_pulldown_en_dis` | `driver/gpio.h` / 🚫 未支持 | src/drivers/esp_gpio.c / src/core/esp_system.c | test_esp_gpio, test_esp_soc_matrix | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gpio_set_intr_type` / `gpio_intr_enable_disable` / `gpio_install_isr_service` / `gpio_isr_handler_add_remove` / `gpio_uninstall_isr_service` | `driver/gpio.h` / ✅ 支持 | src/drivers/esp_gpio.c / src/core/esp_system.c | test_esp_gpio, test_esp_soc_matrix | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `esp_task_wdt_*` / `esp_intr_alloc_free` | `esp_task_wdt.h` / `esp_intr_alloc.h` / 🚫 未支持 | src/drivers/esp_gpio.c / src/core/esp_system.c | test_esp_gpio, test_esp_soc_matrix | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `esp_rom_gpio_pad_select_gpio` | `esp_rom_gpio.h` / ⚠️ 降级支持 | src/drivers/esp_gpio.c / src/core/esp_system.c | test_esp_gpio, test_esp_soc_matrix | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `esp_err_from_wink` | `esp_err.h` / ✅ 支持 | src/core/esp_err.c | test_esp_err | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `wink_status_from_esp` | `esp_err.h` / ✅ 支持 | src/core/esp_err.c | test_esp_err | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `esp_restart` | `esp_system.h` / ✅ 支持 | src/core/esp_system.c, src/esp_idf_bridge.c | test_esp_idf_runtime | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2/B3 |
| `esp_get_idf_version` | `esp_system.h` / ✅ 支持 | src/core/esp_system.c, src/esp_idf_bridge.c | test_esp_idf_runtime | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `esp_random` | `esp_random.h` / ✅ 支持 | src/core/esp_system.c, src/esp_idf_bridge.c | test_esp_idf_runtime | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `esp_log_write` / `ESP_LOG*` | `esp_log.h` / ⚠️ 降级支持 | src/core/esp_log.c | test_esp_log | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `xTaskCreate` / `xTaskCreatePinnedToCore` | `freertos/task.h` / ⚠️ 降级支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `vTaskDelete` | `freertos/task.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `vTaskDelay` | `freertos/task.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `vTaskDelayUntil` | `freertos/task.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `vTaskSuspend` / `vTaskResume` | `freertos/task.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xTaskGetTickCount` / `FromISR` | `freertos/task.h` / ⚠️ 降级支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `eTaskGetState` | `freertos/task.h` / ⚠️ 降级支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `uxTaskGetNumberOfTasks` | `freertos/task.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `uxTaskPriorityGet` / `vTaskPrioritySet` | `freertos/task.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `uxTaskGetStackHighWaterMark` | `freertos/task.h` / ⚠️ 降级支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `vTaskList` / `uxTaskGetSystemState` | `freertos/task.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `taskENTER_CRITICAL` / `EXIT_CRITICAL` / `portENTER_CRITICAL` / `portEXIT_CRITICAL` / `taskENTER_CRITICAL_ISR` / `taskEXIT_CRITICAL_ISR` | `freertos/task.h` / `freertos/portmacro.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xPortGetCoreID` / `xTaskGetSchedulerState` | `freertos/task.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `vTaskStartScheduler` | `freertos/task.h` / ⚠️ 降级支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xQueueCreate` / `vQueueDelete` | `freertos/queue.h` / ⚠️ 降级支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xQueueSend` / `xQueueReceive` / `xQueuePeek` | `freertos/queue.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xQueueSendFromISR` / `ReceiveFromISR` | `freertos/queue.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `uxQueueMessagesWaiting` / `SpacesAvailable` | `freertos/queue.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xQueueReset` | `freertos/queue.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xSemaphoreCreateMutex` / `Binary` / `Counting` | `freertos/semphr.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xSemaphoreCreateRecursiveMutex` | `freertos/semphr.h` / 🚫 未支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xSemaphoreTake` / `xSemaphoreGive` / `FromISR` | `freertos/semphr.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xEventGroupCreate` / `vEventGroupDelete` | `freertos/event_groups.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xEventGroupWaitBits` / `SetBits` / `ClearBits` | `freertos/event_groups.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `xEventGroup*FromISR` | `freertos/event_groups.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `timers.h` 全系 API | `freertos/timers.h` / 🚫 未支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `ledc_timer_config` / `ledc_channel_config` | `driver/ledc.h` / ✅ 支持 | src/drivers/esp_ledc.c | test_esp_ledc | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `ledc_set_duty` / `ledc_update_duty` | `driver/ledc.h` / ✅ 支持 | src/drivers/esp_ledc.c | test_esp_ledc | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `ledc_set_fade_with_time` / `ledc_fade_start` | `driver/ledc.h` / ⚠️ 降级支持 | src/drivers/esp_ledc.c | test_esp_ledc | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `ledc_set_freq` / `ledc_get_freq` / `ledc_get_duty` | `driver/ledc.h` / ✅ 支持 | src/drivers/esp_ledc.c | test_esp_ledc | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `ledc_stop` | `driver/ledc.h` / ✅ 支持 | src/drivers/esp_ledc.c | test_esp_ledc | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `i2c_param_config` / `i2c_driver_install` / `delete` | `driver/i2c.h` (Legacy) / ✅ 支持 | src/drivers/esp_i2c_legacy.c, esp_i2c_master.c | test_esp_i2c | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `i2c_cmd_link_create` / `delete` | `driver/i2c.h` (Legacy) / ✅ 支持 | src/drivers/esp_i2c_legacy.c, esp_i2c_master.c | test_esp_i2c | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `i2c_master_start` / `stop` / `write*` / `read*` | `driver/i2c.h` (Legacy) / ✅ 支持 | src/drivers/esp_i2c_legacy.c, esp_i2c_master.c | test_esp_i2c | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `i2c_master_cmd_begin` | `driver/i2c.h` (Legacy) / ⚠️ 降级支持 | src/drivers/esp_i2c_legacy.c, esp_i2c_master.c | test_esp_i2c | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `i2c_new_master_bus` / `i2c_del_master_bus` | `driver/i2c_master.h` (Modern) / ✅ 支持 | src/drivers/esp_i2c_legacy.c, esp_i2c_master.c | test_esp_i2c | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `i2c_master_bus_add_device` / `rm_device` | `driver/i2c_master.h` (Modern) / ✅ 支持 | src/drivers/esp_i2c_legacy.c, esp_i2c_master.c | test_esp_i2c | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `i2c_master_transmit` / `receive` / `probe` | `driver/i2c_master.h` (Modern) / ✅ 支持 | src/drivers/esp_i2c_legacy.c, esp_i2c_master.c | test_esp_i2c | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `uart_param_config` / `uart_set_pin` | `driver/uart.h` / ✅ 支持 | src/drivers/esp_uart.c | test_esp_uart | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `uart_driver_install` / `uart_driver_delete` | `driver/uart.h` / ✅ 支持 | src/drivers/esp_uart.c | test_esp_uart | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `uart_write_bytes` | `driver/uart.h` / ✅ 支持 | src/drivers/esp_uart.c | test_esp_uart | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `uart_read_bytes` | `driver/uart.h` / ⚠️ 降级支持 | src/drivers/esp_uart.c | test_esp_uart | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `uart_flush` / `uart_get_buffered_data_len` | `driver/uart.h` / ✅ 支持 | src/drivers/esp_uart.c | test_esp_uart | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gptimer_new_timer` / `gptimer_del_timer` | `driver/gptimer.h` / ✅ 支持 | src/drivers/esp_gptimer.c | test_esp_gptimer | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gptimer_set_alarm_action` | `driver/gptimer.h` / ⚠️ 降级支持 | src/drivers/esp_gptimer.c | test_esp_gptimer | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gptimer_enable` / `disable` / `start` / `stop` | `driver/gptimer.h` / ✅ 支持 | src/drivers/esp_gptimer.c | test_esp_gptimer | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gptimer_get_raw_count` / `set_raw_count` | `driver/gptimer.h` / ✅ 支持 | src/drivers/esp_gptimer.c | test_esp_gptimer | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `gptimer_register_event_callbacks` | `driver/gptimer.h` / ✅ 支持 | src/drivers/esp_gptimer.c | test_esp_gptimer | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `spi_bus_initialize` / `spi_bus_free` | `driver/spi_common.h` / ✅ 支持 | src/drivers/esp_spi.c | test_esp_spi | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `spi_bus_add_device` / `remove_device` | `driver/spi_common.h` / ✅ 支持 | src/drivers/esp_spi.c | test_esp_spi | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `spi_device_transmit` | `driver/spi_master.h` / ⚠️ 降级支持 | src/drivers/esp_spi.c | test_esp_spi | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `nvs_flash_init` / `erase` / `deinit` | `nvs_flash.h` / ✅ 支持 | src/core/esp_nvs.c | test_esp_nvs | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `nvs_open` / `nvs_close` / `nvs_commit` | `nvs.h` / ✅ 支持 | src/core/esp_nvs.c | test_esp_nvs | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `nvs_set_*` / `nvs_get_*` 全类型原语 | `nvs.h` / ✅ 支持 | src/core/esp_nvs.c | test_esp_nvs | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `nvs_erase_key` / `nvs_erase_all` / `nvs_open_from_partition` | `nvs.h` / ✅ 支持 | src/core/esp_nvs.c | test_esp_nvs | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H7 |
| `heap_caps_malloc` / `heap_caps_free` / `heap_caps_calloc` / `heap_caps_realloc` | `esp_heap_caps.h` / ✅ 支持 | src/core/esp_heap_caps.c | test_esp_idf_phase3 (indirect) | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H3/D1 |
| `heap_caps_get_free_size` / `heap_caps_get_minimum_free_size` | `esp_heap_caps.h` / ✅ 支持 | src/core/esp_heap_caps.c | test_esp_idf_phase3 (indirect) | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H3/D1 |
| `esp_wifi_init` / `start` / `stop` / `connect` / `disconnect` / `deinit` | `esp_wifi.h` / ✅ 支持 | src/wifi/esp_wifi.c, src/wifi/esp_netif.c | test_esp_wifi | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_wifi_get_mac` / `set_mac` / `set_config` / `get_config` | `esp_wifi.h` / ✅ 支持 | src/wifi/esp_wifi.c, src/wifi/esp_netif.c | test_esp_wifi | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_wifi_set_mode` | `esp_wifi.h` / ⚠️ 降级支持 | src/wifi/esp_wifi.c, src/wifi/esp_netif.c | test_esp_wifi | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_wifi_scan_*` | `esp_wifi.h` / 🚫 未支持 | src/wifi/esp_wifi.c, src/wifi/esp_netif.c | test_esp_wifi | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_event_loop_create_default` / `delete_default` | `esp_event.h` / ✅ 支持 | src/core/esp_event.c | test_esp_wifi, test_esp_mqtt (indirect) | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_event_handler_register` / `unregister` | `esp_event.h` / ✅ 支持 | src/core/esp_event.c | test_esp_wifi, test_esp_mqtt (indirect) | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_event_handler_instance_register` / `unregister` | `esp_event.h` / ✅ 支持 | src/core/esp_event.c | test_esp_wifi, test_esp_mqtt (indirect) | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_event_post` | `esp_event.h` / ✅ 支持 | src/core/esp_event.c | test_esp_wifi, test_esp_mqtt (indirect) | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_netif_init` / `esp_netif_create_default_wifi_sta` | `esp_netif.h` / ✅ 支持 | src/wifi/esp_wifi.c, src/wifi/esp_netif.c | test_esp_wifi | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_netif_get_ip_info` | `esp_netif.h` / ⚠️ 降级支持 | src/wifi/esp_wifi.c, src/wifi/esp_netif.c | test_esp_wifi | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_mqtt_client_init` / `start` / `stop` / `reconnect` / `disconnect` / `destroy` | `mqtt_client.h` / ✅ 支持 | src/network/esp_mqtt.c | test_esp_mqtt | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_mqtt_client_publish` / `subscribe` / `unsubscribe` | `mqtt_client.h` / ⚠️ 降级支持 | src/network/esp_mqtt.c | test_esp_mqtt | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_mqtt_client_register_event` | `mqtt_client.h` / ✅ 支持 | src/network/esp_mqtt.c | test_esp_mqtt | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_mqtt_sim_*` | `mqtt_client.h` / ✅ 支持 | src/network/esp_mqtt.c | test_esp_mqtt | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_http_client_init` / `perform` / `cleanup` | `esp_http_client.h` / ⚠️ 降级支持 | src/network/esp_http.c | test_esp_http_client | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_http_client_set_url` / `set_method` / `set_post_field` | `esp_http_client.h` / ✅ 支持 | src/network/esp_http.c | test_esp_http_client | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_http_client_set_header` / `get_header` / `delete_header` | `esp_http_client.h` / ✅ 支持 | src/network/esp_http.c | test_esp_http_client | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_http_client_open` / `fetch_headers` / `read` / `read_response` / `write` / `close` | `esp_http_client.h` / ✅ 支持 | src/network/esp_http.c | test_esp_http_client | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_http_client_sim_*` | `esp_http_client.h` / ✅ 支持 | src/network/esp_http.c | test_esp_http_client | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `nimble_port_init` / `deinit` / `run` / `stop` | `nimble/nimble_port.h` / ✅ 支持 | src/bluetooth/esp_nimble.c | test_esp_nimble | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `nimble_port_freertos_init` / `deinit` | `nimble/nimble_port_freertos.h` / ✅ 支持 | src/freertos/*.c, src/esp_idf_runtime.c | test_esp_idf_freertos, phase2/phase3 | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H4, H6 |
| `ble_svc_gap_init` / `ble_svc_gap_device_name*` / `appearance*` | `services/gap/ble_svc_gap.h` / ✅ 支持 | src/bluetooth/esp_nimble.c | test_esp_nimble | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `ble_svc_gatt_init` | `services/gatt/ble_svc_gatt.h` / ✅ 支持 | src/bluetooth/esp_nimble.c | test_esp_nimble | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `ble_gatts_count_cfg` / `ble_gatts_add_svcs` / `ble_gatts_start` | `host/ble_gatt.h` / ✅ 支持 | src/bluetooth/esp_nimble.c | test_esp_nimble | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `ble_gatts_chr_updated` / `ble_gatts_notify` / `notify_custom` | `host/ble_gatt.h` / ✅ 支持 | src/bluetooth/esp_nimble.c | test_esp_nimble | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `ble_gap_adv_start` / `stop` / `active` / `set_fields` / `rsp_set_fields` | `host/ble_gap.h` / ✅ 支持 | src/bluetooth/esp_nimble.c | test_esp_nimble | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `ble_gap_conn_find` / `ble_gap_terminate` | `host/ble_gap.h` / ✅ 支持 | src/bluetooth/esp_nimble.c | test_esp_nimble | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `ble_uuid_cmp` / `ble_uuid_to_str` / `ble_uuid_u16` | `host/ble_uuid.h` / ✅ 支持 | src/bluetooth/esp_nimble.c | test_esp_nimble | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `os_mbuf_append` / `copydata` / `ble_hs_mbuf_*` | `os/os_mbuf.h` / `host/ble_hs.h` / ✅ 支持 | src/bluetooth/esp_nimble.c | test_esp_nimble | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |
| `esp_nimble_sim_*` | `host/ble_hs.h` / ✅ 支持 | src/bluetooth/esp_nimble.c | test_esp_nimble | 33/33 compile | generic replay only | 18 compile/vendor items; no API diff | idf.py missing | H2, H5, H6, H7 |

## 已发现的不一致与任务

1. Queue 降级登记仍写 512B，但当前 STANDARD 2048B/PRO 8192B 边界测试据计划记录已通过：A/H1 同步活矩阵。
2. heap_caps 矩阵仍写 100% libc free 互通与精确水位，D1 仿真契约要降级并改 ADR/活文档：C/H3。
3. ready 标志发布时序与重复 pool reset 直接影响构造期资源：B1/B2。
4. esp_restart 非调度器 fall-through，bridge reset 漏掉 event/network 子系统：B3。
5. 当前 CTest 不能证明逐 API 失败语义、真实 Wasm 运行或官方行为相等：H2-H7。
6. idf.py 缺失，真机 Xtensa/RISC-V 载体待准备：H7/H8。
7. 远端凭据、required check、全 24 组合无证据：A/H8、F/H8。

## H0 结论

H0 基线快照已形成；未验证格均映射到 A/H1、B/H2、C/H3、D/H4、E/H5、D/H6、F/H7/H8 或明确降级。H0 可退出；A 仍因远端 CI/coverage/required check 未验证而未完成。下一项按计划为 B1 构造期红测试。本快照不将既存 dirty worktree 改动归入 HEAD。
## 后续更正（2026-09-28）

本快照的“idf.py missing / H7”环境栏是在错误的 Python/工具上下文中采集，不能据此判断 ESP-IDF 未安装。使用 wink-tools 源码入口和 pyenv Python 3.12.10 运行 doctor 后确认 ESP-IDF 6.1 已安装。该纠正只更新环境事实，不把 IDF 6.1 安装状态误报成原生 IDF 构建已验证。此快照属于 wink-micro-os/frameworks/esp_idf 仿真计划；wink-firmware-carriers/esp32 原生 Wink role-action carrier 构建不属于本计划门槛。
