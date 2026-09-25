# ESP-IDF 仿真拦截层 API 覆盖矩阵与降级登记簿 (02-api-coverage-matrix)

> **版本**：v2.2  
> **适用里程碑**：M2~M3 (外设与总线仿真拦截；多 SoC 矩阵、覆盖率与确定性收官)  
> **收割口径**：v6.1@fff9895c vendored（`manifest.hash = 542eb37a5dc3604c`）

---

## 0. 收割 SLA 快照（机器生成，v6.1@fff9895c）

> 本表为门面**人工维护**的 API 覆盖与降级登记；与其互补的**全量机器生成**清单见
> [`../include/api-coverage-matrix.inc.md`](../include/api-coverage-matrix.inc.md)
> （280 生成头 / 450 Supported / 1097 Out-of-scope）。片段由闭源收割器单源反写，**禁止手改**；
> 一致性由闭源 `ci_gate` 与开源 `.github/scripts/check_harvested_headers.py` 双向校验。

| 收割状态 | 生成口径 | 与本文档的关系 |
|:---|:---|:---|
| ✅ Supported | `rules/esp_idf.yaml: sla.supported_prefixes`（v2.13 补齐 `esp_restart`/`esp_random`/`esp_err_to_name`/`spi_bus_*`/`i2c_del_master_bus` 等门面已实现 API） | 应与 §1 门面行一一对应 |
| 🚫 Out-of-scope | `__WINK_SIM__` 注入时编译期 `WINK_SLA_ERROR` 阻断（GCC/Clang `error` 属性；MSVC 空宏 + 链接缺符号 + `winkcli lint` 前移） | 对应 §1「未支持」与 §2 Fail-Loud 条目 |
| ⚠️ 降级支持 | 门面运行时桩/语义弱化（§2 登记） | 收割侧不判定降级，由本文档人工登记 |

> **v6.1 vendoring 变更（2026-09-25）**：
> - `esp_log` 系列随 IDF v6 统一入口：门面新增 `esp_log`/`esp_log_va` 实现（`ESP_LOG*` 宏展开目标）；
> - `gpio_uninstall_isr_service` 官方签名由 `void` 变为 `esp_err_t`（见降级条目 4）；
> - `esp_intr_alloc/free`、`esp_task_wdt_*` 在 `__WINK_SIM__` 注入下为**编译期** Fail-Loud（收割 SLA），
>   未注入时门面运行时桩仍返回 `ESP_ERR_NOT_SUPPORTED`（见降级条目 5）。

---

## 1. M0, M1 & M2 交付 API 覆盖清单

| API 标识符 | 所属头文件 | 实现状态 | 仿真底层对应物 | 降级/弱化登记 |
|:---|:---|:---:|:---|:---|
| `gpio_config` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_init` (逐 pin) | 遇到越界引脚立即返回 `ESP_ERR_INVALID_ARG` |
| `gpio_set_direction` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_init`（幂等且记录模式；host `pal_gpio_set_direction` 为 no-op 不记录模式，故降级经 init 下沉；官方本 API 无 pull 参数） | 遇到越界引脚返回 `ESP_ERR_INVALID_ARG` |
| `gpio_set_level` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_write` | 遇到越界引脚或只读引脚返回 `ESP_ERR_INVALID_ARG` |
| `gpio_get_level` | `driver/gpio.h` | ⚠️ 降级支持 | `pal_gpio_read` | **[降级登记 1]** 越界/读取失败与电平 0 不可区分 |
| `gpio_reset_pin` | `driver/gpio.h` | ✅ 支持 | `pal_gpio_deinit` | 成功返回 `ESP_OK`，越界返回 `ESP_ERR_INVALID_ARG` |
| `gpio_set_pull_mode` / `gpio_pullup_en_dis` / `gpio_pulldown_en_dis` | `driver/gpio.h` | 🚫 未支持 | 无 | **[降级登记 4]** 越界仍 `ESP_ERR_INVALID_ARG`，有效引脚 `ESP_LOGE` + `ESP_ERR_NOT_SUPPORTED` |
| `gpio_set_intr_type` / `gpio_intr_enable_disable` / `gpio_install_isr_service` / `gpio_isr_handler_add_remove` | `driver/gpio.h` | 🚫 未支持 | 无 | **[降级登记 4]** `ESP_LOGE` + `ESP_ERR_NOT_SUPPORTED`；`gpio_uninstall_isr_service`（void 无错误通道）仅 `ESP_LOGW` |
| `esp_task_wdt_*` / `esp_intr_alloc_free` | `esp_task_wdt.h` / `esp_intr_alloc.h` | 🚫 未支持 | 无 | **[降级登记 5]** `ESP_LOGE` + `ESP_ERR_NOT_SUPPORTED`；失败时 `esp_intr_alloc` 回写空句柄 |
| `esp_rom_gpio_pad_select_gpio` | `esp_rom_gpio.h` | ⚠️ 降级支持 | 无 | **[降级登记 5]** void 无错误通道，仅 `ESP_LOGW` 后 no-op |
| `esp_err_from_wink` | `esp_err.h` | ✅ 支持 | 查表转换 | 负数 Wink 错误码转为 ESP 0x101+ 体系 |
| `wink_status_from_esp` | `esp_err.h` | ✅ 支持 | 查表转换 | ESP 错误码转回 Wink 负数错误码 |
| `esp_restart` | `esp_system.h` | ✅ 支持 | `pal_wasm_target_*` 弱钩子族 | 仅置位 reset pending 标志，绝不调用 host exit |
| `esp_get_idf_version` | `esp_system.h` | ✅ 支持 | 静态常量字符串 | 返回 `"v6.1-dev-winksim"` |
| `esp_random` | `esp_random.h` | ✅ 支持 | xorshift32 确定性算法 | 保证仿真 Replay 确定性 |
| `esp_log_write` / `ESP_LOG*` | `esp_log.h` | ⚠️ 降级支持 | `pal_log_e/w/i/d` | **[降级登记 2]** ISR 下 INFO 等级可能被底层 PAL 静默丢弃 |
| `xTaskCreate` / `xTaskCreatePinnedToCore` | `freertos/task.h` | ⚠️ 降级支持 | `sim_scheduler_register` | **[降级登记 6, 7]** 优先级 clamp 0..24 仅存储不抢占；多核 affinity 钳制 core 0；栈深下限自动 clamp |
| `vTaskDelete` | `freertos/task.h` | ✅ 支持 | `sim_scheduler_mark_zombie` + 切出桥 | 自删时立即让出 Fiber 栈，由主循环 GC 释放 |
| `vTaskDelay` | `freertos/task.h` | ✅ 支持 | `sim_scheduler_yield_timed` / 切出桥 | delay 0 为纯让出（保持 READY 态），delay > 0 等待定时器 |
| `vTaskDelayUntil` | `freertos/task.h` | ✅ 支持 | `vTaskDelay` 周期推导 | 自然绕回，防止追赶风暴 |
| `vTaskSuspend` / `vTaskResume` | `freertos/task.h` | ✅ 支持 | `sim_scheduler_block` (SUSPEND tag) / resume | 支持挂起自身及他者任务 |
| `xTaskGetTickCount` / `FromISR` | `freertos/task.h` | ⚠️ 降级支持 | 调度器统一时钟 | **[降级登记 9, 11]** 无任务推进时 Tick 冻结；FromISR 等价透传 |
| `eTaskGetState` | `freertos/task.h` | ⚠️ 降级支持 | `sim_task_state_t` | **[降级登记 12]** 映射 FreeRTOS 六态 |
| `uxTaskGetNumberOfTasks` | `freertos/task.h` | ✅ 支持 | `sim_scheduler_task_count` | 活跃任务计数 |
| `uxTaskPriorityGet` / `vTaskPrioritySet` | `freertos/task.h` | ✅ 支持 | TCB 优先级簿记 | 动态优先级更新 |
| `uxTaskGetStackHighWaterMark` | `freertos/task.h` | ⚠️ 降级支持 | 哨兵值 | **[降级登记 7]** 固定返回 `UINT32_MAX`，无真实栈水位概念 |
| `vTaskList` / `uxTaskGetSystemState` | `freertos/task.h` | ✅ 支持 | 结构化格式化 | 有界快照导出 |
| `taskENTER_CRITICAL` / `EXIT_CRITICAL` | `freertos/task.h` | ⚠️ 降级支持 | no-op | **[降级登记 8]** 单核协作无抢占，临界区天然安全 |
| `xPortGetCoreID` / `xTaskGetSchedulerState` | `freertos/task.h` | ✅ 支持 | 静态常量 | 恒定返回 core 0 与 RUNNING |
| `vTaskStartScheduler` | `freertos/task.h` | ⚠️ 降级支持 | no-op | 调度权由 target 主循环持有，warn 后忽略 |
| `xQueueCreate` / `vQueueDelete` | `freertos/queue.h` | ⚠️ 降级支持 | 静态 FIFO 缓冲池 (8x 512B) | **[降级登记 13]** 容量超过 512B 返回 NULL |
| `xQueueSend` / `xQueueReceive` / `xQueuePeek` | `freertos/queue.h` | ✅ 支持 | 双等待者队列 + `sync_block` | FIFO-one 定向唤醒，双向 waiter 彻底隔离 |
| `xQueueSendFromISR` / `ReceiveFromISR` | `freertos/queue.h` | ⚠️ 降级支持 | 等价任务逻辑 | **[降级登记 9]** `*pxHigherPriorityTaskWoken=pdFALSE` 恒定 |
| `uxQueueMessagesWaiting` / `SpacesAvailable` | `freertos/queue.h` | ✅ 支持 | 队列实时元素统计 | 精确计数 |
| `xQueueReset` | `freertos/queue.h` | ✅ 支持 | 队列清空 | 重置读写指针与计数 |
| `xSemaphoreCreateMutex` / `Binary` / `Counting` | `freertos/semphr.h` | ✅ 支持 | 静态信号量池 (16x) | Priority-one 定向唤醒（最高优先级先醒，同级 FIFO） |
| `xSemaphoreCreateRecursiveMutex` | `freertos/semphr.h` | 🚫 未支持 | Fail-Loud | **[降级登记 10]** 返回 NULL，声明保留 |
| `xSemaphoreTake` / `xSemaphoreGive` / `FromISR` | `freertos/semphr.h` | ✅ 支持 | 信号量原子记数 + `sync_block` | 支持 Mutex / Binary / Counting |
| `xEventGroupCreate` / `vEventGroupDelete` | `freertos/event_groups.h` | ✅ 支持 | 静态事件组池 (8x) | 24-bit 事件标志 |
| `xEventGroupWaitBits` / `SetBits` / `ClearBits` | `freertos/event_groups.h` | ✅ 支持 | Broadcast-all + `sync_block` | 支持 `xWaitForAllBits` 及 `xClearOnExit` |
| `xEventGroup*FromISR` | `freertos/event_groups.h` | ⚠️ 降级支持 | 等价任务逻辑 | **[降级登记 9]** `*pxHigherPriorityTaskWoken=pdFALSE` 恒定 |
| `timers.h` 全系 API | `freertos/timers.h` | 🚫 未支持 | Fail-Loud | **[降级登记 10]** `xTimerCreate` 返 NULL，其余返 `pdFAIL` |
| `ledc_timer_config` / `ledc_channel_config` | `driver/ledc.h` | ✅ 支持 | `pal_pwm_config_pin` | 支持配置 4 定时器 / 8 通道；零浮点定点计算 |
| `ledc_set_duty` / `ledc_update_duty` | `driver/ledc.h` | ✅ 支持 | `pal_pwm_set_duty_bp` | 定点 basis points 转换（0..10000 BP）；ADR-0066 纯整型 |
| `ledc_set_fade_with_time` / `ledc_fade_start` | `driver/ledc.h` | ⚠️ 降级支持 | 即时阶跃 + 同步回调 | **[降级登记 15]** 无硬件斜坡生成器，降级为目标值直接生效 |
| `ledc_set_freq` / `ledc_get_freq` / `ledc_get_duty` | `driver/ledc.h` | ✅ 支持 | `pal_pwm_set_freq` | 支持通道/定时器参数动态查询与频率重设 |
| `ledc_stop` | `driver/ledc.h` | ✅ 支持 | `pal_pwm_set_duty_bp(0)` | 强制输出空闲电平并关断输出 |
| `i2c_param_config` / `i2c_driver_install` / `delete` | `driver/i2c.h` (Legacy) | ✅ 支持 | `pal_i2c_init` / `deinit` | 记录主从模式；从机模式 Fail-Loud 拒绝 |
| `i2c_cmd_link_create` / `delete` | `driver/i2c.h` (Legacy) | ✅ 支持 | 静态 8 命令槽位池 | 支持单链最高 32 个命令节点（Zero Malloc） |
| `i2c_master_start` / `stop` / `write*` / `read*` | `driver/i2c.h` (Legacy) | ✅ 支持 | 内存连续缓冲区校验 | 仅记录操作序列，为 CMD Folding 收集参数 |
| `i2c_master_cmd_begin` | `driver/i2c.h` (Legacy) | ⚠️ 降级支持 | `pal_i2c_transfer_timeout` | **[降级登记 19]** 状态机折叠为单次/复合 PAL 传输，支持 Repeated START |
| `i2c_new_master_bus` / `i2c_del_master_bus` | `driver/i2c_master.h` (Modern) | ✅ 支持 | `pal_i2c_init` / `deinit` | 静态总线池分配与资源隔离 |
| `i2c_master_bus_add_device` / `rm_device` | `driver/i2c_master.h` (Modern) | ✅ 支持 | 静态器件槽位分配 | 支持 16 器件同时挂载于虚拟总线 |
| `i2c_master_transmit` / `receive` / `probe` | `driver/i2c_master.h` (Modern) | ✅ 支持 | `pal_i2c_transfer_timeout` | 阻塞式主模式标准数据收发与 ACK 探测 |
| `uart_param_config` / `uart_set_pin` | `driver/uart.h` | ✅ 支持 | 延迟引脚与波特率簿记 | 支持 `UART_PIN_NO_CHANGE` (-1) 参数 |
| `uart_driver_install` / `uart_driver_delete` | `driver/uart.h` | ✅ 支持 | `pal_uart_init` / `deinit` | 内置 512B 环形缓冲区，支持 FreeRTOS 事件队列 |
| `uart_write_bytes` | `driver/uart.h` | ✅ 支持 | `pal_uart_write` | 支持定额字节阻塞发送 |
| `uart_read_bytes` | `driver/uart.h` | ⚠️ 降级支持 | 环形缓冲区读取 + 协作切出 | **[降级登记 17]** 超时等待调用 `sim_scheduler_yield_context` 协作让出 |
| `uart_flush` / `uart_get_buffered_data_len` | `driver/uart.h` | ✅ 支持 | 环形缓冲区指针重置与计数 | 准确返回当前待读字节数 |
| `gptimer_new_timer` / `gptimer_del_timer` | `driver/gptimer.h` | ✅ 支持 | 静态 4 定时器槽位池 | 参数校验与分辨率记录 |
| `gptimer_set_alarm_action` | `driver/gptimer.h` | ⚠️ 降级支持 | `pal_hwtimer_init` | **[降级登记 16]** 周期自动 clamp 不小于 10ms (10000us) |
| `gptimer_enable` / `disable` / `start` / `stop` | `driver/gptimer.h` | ✅ 支持 | `pal_hwtimer_start` / `stop` | 状态机校验（必须先 enable 再 start） |
| `gptimer_get_raw_count` / `set_raw_count` | `driver/gptimer.h` | ✅ 支持 | `pal_os_get_us()` + 偏移推导 | 支持动态修改/查询定时器计数值 |
| `gptimer_register_event_callbacks` | `driver/gptimer.h` | ✅ 支持 | 警报中断上下文直调 | 警报触发时同步调用用户 `on_alarm` 回调 |
| `spi_bus_initialize` / `spi_bus_free` | `driver/spi_common.h` | ✅ 支持 | `pal_spi_init` / `deinit` | 支持 SPI2_HOST (HSPI) 与 SPI3_HOST (VSPI)；SPI1 Fail-Loud 拒绝 |
| `spi_bus_add_device` / `remove_device` | `driver/spi_common.h` | ✅ 支持 | `pal_spi_add_device` | 静态 8 器件池；支持极性/相位/CS 高低有效映射 |
| `spi_device_transmit` | `driver/spi_master.h` | ⚠️ 降级支持 | `pal_spi_transfer_device` | **[降级登记 18]** 支持 `SPI_TRANS_USE_TXDATA/RXDATA`，全双工同步轮询 |
| `nvs_flash_init` / `erase` / `deinit` | `nvs_flash.h` | ✅ 支持 | 内存 KV 清空与初始化 | 支持模拟 Flash 初始化与格式化 |
| `nvs_open` / `nvs_close` / `nvs_commit` | `nvs.h` | ✅ 支持 | 静态 8 句柄槽位分配 | 命名空间隔离与深拷贝；commit 为 no-op 确认 |
| `nvs_set_*` / `nvs_get_*` 全类型原语 | `nvs.h` | ⚠️ 降级支持 | 静态 32 键值项存储池 | **[降级登记 20]** 纯内存态存储，无跨进程物理持久化 |
| `nvs_erase_key` / `nvs_erase_all` | `nvs.h` | ✅ 支持 | 句柄命名空间匹配擦除 | 精确支持单键擦除与空间批量擦除 |

---

## 2. 降级与弱化登记簿 (ADR-0012 合约诚实)

根据 ADR-0012《合约诚实与降级登记》规范，所有由于目标环境限制、官方 C-ABI 缺陷或仿真简化导致的语义弱化必须显式登记：

### 降级条目 1：`gpio_get_level` 越界与低电平不可区分
- **受影响 API**：`int gpio_get_level(gpio_num_t gpio_num)`
- **官方缺陷**：ESP-IDF 官方 C-ABI 返回 `int` 电平值 (0 或 1)，无独立错误码返回通道。
- **拦截层行为**：
  - 当传入无效引脚时，记录 `ESP_LOGE` 错误日志，并返回 0。
  - 当底层 `pal_gpio_read` 失败时，记录 `ESP_LOGE` 错误日志，并返回 0。
- **风险与影响**：符合官方现有行为，通过日志暴露可见性。

### 降级条目 2：`ESP_LOG*` 级别与 ISR 上下文限制
- **受影响 API**：`ESP_LOGI`, `ESP_LOGD`, `ESP_LOGV`
- **底层限制**：WinkMicroOS 的 PAL 日志系统（`pal_log.h`）在中断上下文（ISR）下对 INFO 等级有静默丢弃策略。
- **拦截层行为**：转调 `pal_log_*`，遵循 PAL 底层过滤规则。

### 降级条目 3：输出引脚电平回读走门面缓存
- **受影响 API**：`int gpio_get_level(gpio_num_t gpio_num)`（输出模式引脚）
- **底层限制**：Host/Wasm PAL 的 `pal_gpio_read` 对输出模式引脚报告模式空闲电平，而非最后一次驱动电平。
- **拦截层行为**：门面维护 `s_is_output` / `s_output_levels` 回读缓存（`src/drivers/esp_gpio.c`），`gpio_set_level` 成功即更新；输入模式引脚直读 PAL。
- **M1 复核**：协作单线程及协作多任务环境下，`set_level` 与 `get_level` 之间无抢占分叉风险，M1 复核通过。

### 降级条目 4：GPIO 上拉/中断/ISR 全家桶未支持（Fail-Loud）
- **受影响 API**：`gpio_set_pull_mode`、`gpio_pullup_en/dis`、`gpio_pulldown_en/dis`、`gpio_set_intr_type`、`gpio_intr_enable/disable`、`gpio_install_isr_service`、`gpio_isr_handler_add/remove`、`gpio_uninstall_isr_service`
- **拦截层行为**：有效引脚一律 `ESP_LOGE` + 返回 `ESP_ERR_NOT_SUPPORTED`。
- **v6.1 vendoring（2026-09-25）**：`gpio_uninstall_isr_service` 官方签名由 `void` 变为 `esp_err_t`，门面同步返回 `ESP_ERR_NOT_SUPPORTED`；`esp_intr_alloc`/`esp_intr_free` 的**调用点**在 `__WINK_SIM__` 注入下由收割 SLA 编译期 `WINK_SLA_ERROR` 阻断（见降级条目 5）。

### 降级条目 5：看门狗/中断分配/ROM 垫片未支持（Fail-Loud）
- **受影响 API**：`esp_task_wdt_*`、`esp_intr_alloc/free`、`esp_rom_gpio_pad_select_gpio`
- **拦截层行为**：前两组 `ESP_LOGE` + 返回 `ESP_ERR_NOT_SUPPORTED`；`esp_rom_gpio_pad_select_gpio` 仅 `ESP_LOGW` 后 no-op。
- **v6.1 vendoring（2026-09-25）**：`esp_task_wdt_*`/`esp_intr_alloc/free` 在 `__WINK_SIM__` 注入时改为**编译期 Fail-Loud**（收割 Out-of-scope；MSVC 空宏 + 链接缺符号 + lint 前移），未注入时保留上述运行时桩；`esp_rom_gpio_pad_select_gpio` 已入 SLA 白名单（纯声明 + 运行时 no-op）。

### 降级条目 6：优先级存储但不抢占调度、多核 Affinity 钳制 Core 0
- **受影响 API**：`xTaskCreate`, `xTaskCreatePinnedToCore`, `vTaskPrioritySet`, `uxTaskPriorityGet`
- **设计权衡**：WinkMicroOS 仿真内核基于单虚拟核心协作式调度器（`wink_sim_scheduler`），采用 Round-Robin 轮转分发，不引入带抢占的时间片中断；Mutex 唤醒遵守 Priority-one，但就绪队列不作绝对高优先级打断。
- **拦截层行为**：优先级 clamp 至 0..24 正常存储并在 Mutex 唤醒序生效；`xCoreID` 非 0 / 非 `tskNO_AFFINITY` 时打 `ESP_LOGW` 警告并钳制至 core 0。

### 降级条目 7：任务栈深度忽略与 HighWaterMark 哨兵
- **受影响 API**：`xTaskCreate`, `xTaskCreatePinnedToCore`, `uxTaskGetStackHighWaterMark`
- **设计权衡**：仿真环境协程基于宿主纤程（Windows Fiber / POSIX ucontext / Emscripten Asyncify），分配宿主堆栈；无嵌入式物理栈指针。
- **拦截层行为**：`usStackDepth` 由底座自动 clamp 下限；`uxTaskGetStackHighWaterMark` 固定返回 `UINT32_MAX` 哨兵值。

### 降级条目 8：临界区空操作（单核协作式无抢占）
- **受影响 API**：`taskENTER_CRITICAL`, `taskEXIT_CRITICAL`, `taskENTER_CRITICAL_ISR`, `taskEXIT_CRITICAL_ISR`
- **设计权衡**：单虚拟核协作调度下，任意代码段在执行到显式切出点（如 `vTaskDelay` / `sync_block`）前天然具备排他原子性。
- **拦截层行为**：宏定义展开为 `((void)0)`，无死锁开销。

### 降级条目 9：FromISR 系列 API 协作式等价处理
- **受影响 API**：`xQueueSendFromISR`, `xQueueReceiveFromISR`, `xSemaphoreGiveFromISR`, `xEventGroupSetBitsFromISR`, `xTaskGetTickCountFromISR`
- **设计权衡**：仿真环境中中断例程与普通协程同属单线程事件驱动链路。
- **拦截层行为**：`FromISR` 复用任务上下文逻辑；`*pxHigherPriorityTaskWoken` 恒定赋值为 `pdFALSE`（协作调度器在下一切出点自发评估，无即时上下文强占）。

### 降级条目 10：FreeRTOS 定时器与递归互斥 Fail-Loud
- **受影响 API**：`xTimerCreate`, `xTimerStart`, `xTimerStop`, `xTimerReset`, `xTimerChangePeriod`, `xSemaphoreCreateRecursiveMutex`, `xSemaphoreTakeRecursive`, `xSemaphoreGiveRecursive`
- **设计权衡**：定时器守护任务（Daemon Task）与软件定时器排期 M2+；递归互斥在嵌入式开发中属反模式。
- **拦截层行为**：`xTimerCreate` 与 `xSemaphoreCreateRecursiveMutex` 返回 `NULL` 并 `ESP_LOGE`；各操作函数（含 `xSemaphoreTakeRecursive/GiveRecursive`）返回 `pdFAIL` 并 `ESP_LOGE`。严禁静默假装成功。

### 降级条目 11：Tick 时钟冻结与自然绕回
- **受影响 API**：`xTaskGetTickCount`, `xTaskGetTickCountFromISR`
- **设计权衡**：仿真虚拟时间由任务延时与外设定时驱动。
- **拦截层行为**：当前无任务推进虚拟时间时，Tick 计数保持冻结；底层 `uint32` 自然绕回（约 49.7 天），与 ESP-IDF 硬件行为一致。

### 降级条目 12：`eTaskState` 状态映射
- **受影响 API**：`eTaskGetState`
- **拦截层行为**：
  - `SIM_TASK_STATE_READY` -> 当前正在执行任务映射为 `eRunning`，其余映射为 `eReady`；
  - `SIM_TASK_STATE_WAITING` / `SIM_TASK_STATE_BLOCKED` -> 挂起 tag 映射为 `eSuspended`，其余映射为 `eBlocked`；
  - `SIM_TASK_STATE_ZOMBIE` / `TERMINATED` / `INVALID` -> 映射为 `eDeleted`。

### 降级条目 13：Queue 超过 512 字节存储上限拒绝创建
- **受影响 API**：`xQueueCreate`
- **设计权衡**：坚持零堆内存分配（Zero Dynamic Malloc）原则，每个队列控制块内置定额 512 字节环形存储。
- **拦截层行为**：当 `uxQueueLength * uxItemSize > 512` 时，打出 `ESP_LOGW` 警告并直接返回 `NULL`。

### 降级条目 14：Queue 头部入队操作 Fail-Loud
- **受影响 API**：`xQueueSendToFront`, `xQueueSendToFrontFromISR`
- **设计权衡**：仿真层队列采用单向定额环形缓冲区（Ring Buffer）实现 FIFO 语义，暂不支持双端逆向头插操作（LIFO）。
- **拦截层行为**：打出 `ESP_LOGE` 错误日志并返回 `errQUEUE_FULL`；ISR 变体同时将 `*pxHigherPriorityTaskWoken` 赋为 `pdFALSE`。提示开发者改用标准 `xQueueSend` / `xQueueSendToBack`。

### 降级条目 15：LEDC 渐变降级为即时阶跃与同步回调
- **受影响 API**：`ledc_set_fade_with_time`, `ledc_set_fade_with_step`, `ledc_fade_start`, `ledc_cb_register`
- **设计权衡**：Wasm/Host 仿真环境无独立高频 PWM 硬件斜坡发生器。
- **拦截层行为**：`ledc_fade_start` 立即将占空比设置为目标值；若注册了 `fade_cb`，在设置后直接同步调用该回调（`LEDC_FADE_STOPPED` 事件），无动态渐变延时过程。占空比计算严格遵循 ADR-0066 纯整型定点基点（0..10000 BP），严禁浮点运算。

### 降级条目 16：GPTimer 仿真周期下限钳制 10ms
- **受影响 API**：`gptimer_set_alarm_action`
- **设计权衡**：微秒级硬件定时器中断若在宿主无限制投递，会导致事件队列被软中断淹没，阻塞 Fiber/协作调度步进。
- **拦截层行为**：当用户传入的 `alarm_count` 对应周期小于 10000 微秒（10ms）时，底层下发至 `pal_hwtimer_init` 时自动 clamp 为 10000 微秒。

### 降级条目 17：UART 硬件流控忽略与定额缓冲协作阻塞
- **受影响 API**：`uart_set_pin`, `uart_read_bytes`
- **设计权衡**：Host 仿真器无 RTS/CTS 物理连线，且零动态内存分配；单端口同时仅维持单一协作阻塞读取者。
- **拦截层行为**：`uart_set_pin` 中 RTS/CTS 仅记录参数后忽略；`uart_read_bytes` 在空缓冲且 `ticks_to_wait > 0` 时，调用 `sim_scheduler_block` 协作挂起并切出 Fiber，直到底层 PAL 收到数据并注入环形缓冲区后 resume 唤醒；若同一端口已有任务在阻塞，次发任务直接拒绝返回错误以绝孤儿覆写。

### 降级条目 18：SPI Master 同步轮询与从机映射
- **受影响 API**：`spi_bus_initialize`, `spi_device_transmit`
- **设计权衡**：仿真环境目前不模拟 DMA 异步中断队列与半双工模式。
- **拦截层行为**：`spi_device_transmit` 采用全双工同步轮询；主机 ID 仅支持 `SPI2_HOST` (0) 与 `SPI3_HOST` (1)，`SPI1_HOST`（Flash 专用总线）Fail-Loud 报错拒绝。

### 降级条目 19：I2C 降级折叠引擎与从机拒绝
- **受影响 API**：`i2c_master_cmd_begin`, `i2c_driver_install`
- **设计权衡**：ESP-IDF Legacy 命令链 API（Start-Write-Read-Stop）在 PAL 侧对应单次组合原子事务 `pal_i2c_transfer_timeout`。
- **拦截层行为**：状态机引擎在 `i2c_master_cmd_begin` 遍历并折叠链表为 `tx_buf` 与 `rx_buf`（支持单次 Repeated START 复合传输）；同一个 `cmd_handle` 内若包含多个独立 `START...STOP` 事务则 Fail-Loud 报错 `ESP_ERR_NOT_SUPPORTED`；`i2c_driver_install` 若请求 `I2C_MODE_SLAVE` 则立即 Fail-Loud 返回 `ESP_ERR_NOT_SUPPORTED`。

### 降级条目 20：NVS 键值存储内存态模拟
- **受影响 API**：`nvs_set_*`, `nvs_get_*`, `nvs_commit`
- **设计权衡**：单元测试与仿真环境默认隔离宿主文件系统，静态内存严格控制在 3KB 预算内。
- **拦截层行为**：内置静态 16 项 KV 存储池（单项最大 128B 数据），`nvs_commit` 为空操作；键值在当前进程生命周期内跨复位持久（ADR-0082），`nvs_flash_erase` 显式抹除。

---

## 3. 官方语料验证集

| 语料标识 | 官方路径 | 覆盖功能点 | 目标形态 |
|:---|:---|:---|:---:|
| `corpus_blink` | `examples/get-started/blink/main/blink_example_main.c` | GPIO 配置、输出电平控制、FreeRTOS 延迟与多任务协作 | `OBJECT` compile-only 100% 通过；`test_esp_idf_blink_run` 200 ticks 有界运行 + Replay 确定性双跑验证 |
| `corpus_ledc_basic` | `examples/peripherals/ledc/ledc_basic/main/ledc_basic_example_main.c` | LEDC 4 定时器/通道配置、PWM 占空比设置、渐变 API | `OBJECT` compile-only 100% 通过；Wasm compile check 通过 |
| `corpus_i2c_basic` | `examples/peripherals/i2c/i2c_basic/main/i2c_basic_example_main.c` | Modern I2C Master 总线/器件注册、Transmit/Receive 事务 | `OBJECT` compile-only 100% 通过；Wasm compile check 通过 |
| `corpus_legacy_i2c` | `components/driver/test_apps/legacy_i2c_driver/main/test_i2c.c` | Legacy I2C 接口集（配置、命令链、时序、从机） | Tier-B stub 闭包 `OBJECT` compile-only 100% 通过；Wasm compile check 通过 |

---

## 4. M3 验收快照（2026-09-25）

### 4.1 SoC 能力矩阵

| SoC | 引脚上限 | LEDC | HP I2C | UART(HP) | SPI | 数据归属 |
|:---|:---:|:---:|:---:|:---:|:---:|:---|
| esp32 | 40（24/28~31 不存在，34~39 输入专用） | 8ch + HS | 2 | 3 | 3 | `chips/esp32`（vendored 迁移） |
| esp32s3 | 49（22~25 不存在） | 8ch（无 HS） | 2 | 3 | 3 | `chips/esp32s3`（手写，登记 channels.json） |
| esp32c3 | 22 | 6ch（无 HS） | 1 | 2 | 2 | `chips/esp32c3`（手写） |
| esp32c6 | 31 | 6ch（无 HS） | 1（LP 不暴露） | 2（LP 不暴露） | 2 | `chips/esp32c6`（手写） |

越界行为：门面按 `SOC_*` 运行期 Fail-Loud（`ESP_ERR_INVALID_ARG`），由 `test_esp_soc_matrix` 及各驱动单测覆盖
（TC-SOC-01~07：C3 GPIO 22+ / HS LEDC / I2C_NUM_1 / UART_NUM_2 / SPI3、ESP32 GPIO34 输出、越界引脚）。

### 4.2 覆盖率与确定性

| 指标 | 实测 | 门禁 |
|:---|:---|:---|
| `frameworks/esp_idf/src` 行覆盖率 | **85.71%**（gcov 聚合 1650/1925，17 个已链接 TU） | ≥85%（`check_coverage.py`，CI 以 lcov 为准） |
| `ctest -L esp_idf` | esp32/s3/c3/c6 各 **31/31** | 100% |
| Headless 确定性回放 | `test_esp_idf_blink_run` 3 次 SHA-256 bit-exact | `esp_idf_headless_replay` ctest |
| Vendor 行为套件（L2） | `wink-micro-app/vendor/esp_idfv61/` 5 域（GPIO/LEDC/I2C/UART/GPTimer）上游逐字源 + normalized 哈希 pin；`esp_idfv61_*` host/wasm 编译全绿 | `esp_idfv61_vendor_upstream` + Nightly IDF 树 diff |
| 框架库编译告警 | `--clean-first` 0 warning | L0 |

> 注：覆盖率数据为 2026-09-25 本地 gcov 基线；CI 的 lcov 管道为最终权威值。

