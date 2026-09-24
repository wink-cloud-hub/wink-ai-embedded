# ESP-IDF 仿真拦截层 API 覆盖矩阵与降级登记簿 (02-api-coverage-matrix)

> **版本**：v1.1  
> **适用里程碑**：M1 (FreeRTOS 调度器与并发原语 Shim)

---

## 1. M0 & M1 交付 API 覆盖清单

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

### 降级条目 5：看门狗/中断分配/ROM 垫片未支持（Fail-Loud）
- **受影响 API**：`esp_task_wdt_*`、`esp_intr_alloc/free`、`esp_rom_gpio_pad_select_gpio`
- **拦截层行为**：前两组 `ESP_LOGE` + 返回 `ESP_ERR_NOT_SUPPORTED`；`esp_rom_gpio_pad_select_gpio` 仅 `ESP_LOGW` 后 no-op。

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

---

## 3. 官方语料验证集

| 语料标识 | 官方路径 | 覆盖功能点 | 目标形态 |
|:---|:---|:---|:---:|
| `corpus_blink` | `examples/get-started/blink/main/blink_example_main.c` | GPIO 配置、输出电平控制、FreeRTOS 延迟与多任务协作 | `OBJECT` compile-only 100% 通过；`test_esp_idf_blink_run` 200 ticks 有界运行 + Replay 确定性双跑验证 |
