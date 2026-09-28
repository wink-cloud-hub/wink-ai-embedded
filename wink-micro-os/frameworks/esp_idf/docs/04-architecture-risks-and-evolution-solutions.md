# ESP-IDF 仿真拦截层架构风险、已知局限与演进方案 (04-architecture-risks-and-evolution-solutions)

> **版本**：v2.0 (全景扩充版)  
> **适用目标**：WinkMicroOS ESP-IDF 仿真拦截框架 (Axis B)  
> **触发背景**：M3 里程碑收尾与多 SoC 矩阵支持后的长期可维护性、可扩展性全景架构审计  
> **关联规范**：  
> - [01-architecture-and-governance-guide.md](01-architecture-and-governance-guide.md)  
> - [02-api-coverage-matrix.md](02-api-coverage-matrix.md)  
> - [ADR-0085：SoC 双 SSOT 仲裁](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/decisions/core/0085-esp-idf-facade-soc-caps-vs-pal-caps-dual-ssot.md)  
> - [ADR-0087：资产通道与数据归属](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/decisions/core/0087-esp-idf-asset-channels-and-soc-data-ownership.md)  
> - [ADR-0012：合约诚实与 Fail-Loud](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/decisions/core/0012-contract-honesty-over-silent-degradation.md)  
> - [ADR-0057：PAL 保持对网络与 RF 射频无知](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/docs/decisions/core/0057-pal-adc-subsystem-and-channel-3-analog-contract.md)  

---

## 1. 概述与文档定位

本框架采用**非侵入式门面（Facade）模式**与**基于纤程（Fiber Context）的协作调度机制**，成功实现了 ESP-IDF 原生业务代码在 Host 原生与浏览器 WebAssembly 环境下的高保真运行，且在真机构建时保持零体积侵入（`ESP_PLATFORM` 零增量）。

尽管当前代码依托 **Harvester 收割流水线** 与 **`channels.json` 资产通道登记** 建立了极强的防腐化能力，但随着业务从“微控制器级基础控制”向“复杂物联网/通信级应用”延伸，系统不仅会在外设、并发与内存容量上遇到瓶颈，更会在**编译器机制、CPU 体系结构差异、Wasm 虚拟机执行引擎与微秒级物理时序**等底层深水区面临现实摩擦。

为避免技术负债隐式蔓延，本文档以 **「问题描述 + 解决方案（短期缓解 + 中长期演进）」** 的结构化形式，建立**14 维全景架构风险与防御演进矩阵**。

---

## 2. 核心架构问题全景矩阵

```
+───────────────────────────────────────────────────────────────────────────────────────────────+
|                      WinkMicroOS ESP-IDF 跨靶仿真拦截：14 维全景防御矩阵                      |
+──────────────────────────┬─────────────────────────────────────┬──────────────────────────────+
| 架构领域                 | 核心摩擦与风险暴露                   | 架构演进与防御解决方案       |
+──────────────────────────┼─────────────────────────────────────┼──────────────────────────────+
| [维度 1：外设下沉]       | ISSUE-01: DMA/网络驱动膨胀拖垮 PAL   | 宿主虚拟通道与数据面分离     |
| [维度 2：并发模型]       | ISSUE-02: 多核 SMP 自旋锁假阳性     | 时序扰动注入与死锁检测       |
| [维度 3：运行容量]       | ISSUE-03: FreeRTOS 静态池 8 队列硬顶 | CMake 分级 Profile 参数化    |
| [维度 4：事件驱动]       | ISSUE-04: 外部 GPIO/Timer 中断降级  | 虚拟中断事件泵 (Virtual Pump)|
| [维度 5：供应链]         | ISSUE-05: 原厂 v5 与 v6 签名分叉     | 收割器单一基准 + 标签化兼容  |
| [维度 6：时钟死锁]       | ISSUE-06: 纯忙等死循环导致时钟冻结  | 轮询计数让步与配额看门狗     |
| [维度 7：持久化]         | ISSUE-07: NVS/Flash 掉电刷新全丢失   | 宿主 IndexedDB 与虚拟分区表  |
| [维度 8：动态堆]         | ISSUE-08: 三方库调用 heap_caps_* 失败 | 堆能力降级映射与虚拟配额     |
| [维度 9：软复位]         | ISSUE-09: esp_restart 全局变量脏重入 | Wasm 线性内存快照 (Snapshot) |
| [维度 10：几何失真]      | ISSUE-10: 64位 Host 导致指针截断崩溃| 强制 uintptr_t 与 Wasm32 基准 |
| [维度 11：虚拟机]        | ISSUE-11: Asyncify 栈展开与重入炸弹 | 事件排队安全屏障 / 演进 JSPI |
| [维度 12：微秒时序]      | ISSUE-12: 1-Wire/WS2812 翻转抖动必错| 提升为语义级总线 (Channel 2/4)|
| [维度 13：面向对象]      | ISSUE-13: C++ 全局对象构造时序倒挂  | 子系统自愈式按需冷启动       |
| [维度 14：沙箱安全]      | ISSUE-14: VFS 穿透修改开发者物理硬盘| 符号宏重命名与纯内存沙箱隔离 |
+──────────────────────────┴─────────────────────────────────────┴──────────────────────────────+
```

---

### [ISSUE-01] 驱动复杂度的代偿压力与 PAL 的边界危机

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  当前已实现的 GPIO、LEDC(PWM)、GPTimer、I2C Master 等外设结构简单，其语义与 Wink PAL 抽象层具备近乎 1:1 的直接映射关系。但 ESP-IDF 生态中包含大量重型、状态复杂的驱动：
  1. **SPI DMA / 事务队列**：`spi_device_transmit` 包含轮询/中断传输、DMA 链表描述符与多设备事务排队。
  2. **流媒体与环形缓冲外设**：I2S、TWAI (CAN 总线)、USB-CDC，强依赖异步环形缓冲区与 DMA 乒乓中断。
  3. **网络与协议栈**：`esp_netif`、LwIP、Wi-Fi 事件循环与 TLS 证书链。
- **潜在摩擦与风险**：  
  - **PAL 膨胀反噬**：若为了满足这些高级接口而要求 PAL 逐个实现对应功能，PAL 将失去“轻量跨平台抽象”的本质，甚至退化为“另一个 ESP-IDF 底层”。
  - **门面代码爆炸**：若反过来由 `frameworks/esp_idf/src/` 自建复杂的软件模拟，门面实现的代码量与状态机复杂度将呈指数级增长，丧失长期可维护性。

#### 2. 解决方案 (Solutions)
- **短期应对（明确边界与收割 SLA 拦截）**：
  1. **收割期 Fail-Loud 阻断**：利用收割器的 `sla.supported_prefixes` 明确将 Wi-Fi、蓝牙、USB 等目前无仿真需求的重型组件划归为 `Out-of-scope`，通过 `WINK_SLA_ERROR` 在编译期阻断，绝不留半成品空桩。
  2. **同步简化模型降级**：对 SPI 仅支持同步轮询模式；遇到含 DMA 标志的配置直接在门面层返回 `ESP_ERR_NOT_SUPPORTED` 并通过 `02-api-coverage-matrix.md` 显式登记。
- **中长期演进（宿主协议通道代理 - Virtual Host Tunneling）**：
  1. **网络层走 Host/Wasm 隧道**：网络驱动不向下走 PAL 硬件接口，而是实现一套轻量的 `esp_netif` 虚拟驱动，通过 WebSocket / POSIX Socket 将网络包透明转发给宿主（Host 原生网络或浏览器网络接口）。
  2. **总线模拟与数据面分离**：将外设驱动划分为“控制面（寄存器/配置）”与“数据面（数据流传输）”。数据面采用统一的共享环形缓冲管道（RingBuffer Pipe）与前端/宿主交互，防止将具体芯片的 DMA 细节搬入仿真层。

---

### [ISSUE-02] 多核 (SMP) 并发语义降级与假阳性风险

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  ESP32 与 ESP32-S3 原生为双核（Xtensa Dual-Core）架构，原生 ESP-IDF 代码重度依赖多核原子操作、`portMUX_TYPE` 自旋锁、跨核临界区保护（`taskENTER_CRITICAL`）以及任务核绑定（`xTaskCreatePinnedToCore`）。
- **潜在摩擦与风险**：  
  - **并发假阳性（False Positives）**：当前仿真环境运行在单虚拟核（Single Virtual Core）与协作式纤程（Cooperative Fibers）调度之下。所有任务被强制钳制在 Core 0，临界区和自旋锁降级为 `no-op`。
  - **硬件落地即崩溃**：用户在仿真器中调试通过的代码，可能内部存在未加保护的共享资源竞争，或者自旋锁在双核抢占环境下的死锁风险。由于仿真环境“天然串行”，这些致命并发缺陷会被完美掩盖，一烧录进真机固件便立即暴露。

#### 2. 解决方案 (Solutions)
- **短期应对（锁状态跟踪与重入/死锁告警）**：
  1. **自旋锁状态模拟**：不再使用完全空的 `no-op` 宏。在 `src/freertos/` 中为 `portMUX_TYPE` 提供轻量簿记结构：
     ```c
     typedef struct {
         uint32_t owner_task_id;
         uint32_t nesting_count;
     } esp_sim_spinlock_t;
     ```
  2. **非法嵌套与重入拦截**：当同一任务重复非法获取不可重入自旋锁，或任务在持有临界区自旋锁期间调用可能触发切出让步的 API（如 `vTaskDelay`、`xQueueReceive`）时，立即抛出运行时断言或 `ESP_LOGE` 报警，前移暴露不合规的并发代码。
- **中长期演进（时序扰动注入与 Chaos 测试）**：
  1. **主动时序洗牌（Chaos Yield Injection）**：在 debug 仿真模式下，允许在各个任务的协作切换点随机注入微小的虚拟延时或调整就绪队列轮转顺序，打破因协作调度过于规律而掩盖的隐性竞态。
  2. **双虚拟核时分轮转模型**：未来若需高保真验证双核逻辑，可引入逻辑双核标记，将任务分配给 Core 0 与 Core 1 两个就绪列表，交替调度并仿真跨核 IPC 通知。

---

### [ISSUE-03] 静态资源池（Static Pool Budget）与大型应用的容量摩擦

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  为保证仿真环境安全与避免内存碎片，框架严格践行“零运行期堆分配（0 malloc）”。在 [`freertos_sync.h`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/freertos/freertos_sync.h) 与 [`freertos_queue.c`](file:///d:/workspaces/ai-coding/wink-ai/wink-ai-embedded/wink-micro-os/frameworks/esp_idf/src/freertos/freertos_queue.c) 中硬编码了全局静态资源池：
  - `FREERTOS_MAX_TASKS = 8`
  - `FREERTOS_MAX_QUEUES = 8`
  - `FREERTOS_QUEUE_STORAGE_SIZE = 512B`
  - `FREERTOS_MAX_SEMAPHORES = 16`
  - `FREERTOS_MAX_EVENT_GROUPS = 8`
- **潜在摩擦与风险**：  
  - 针对微控制器入门教学、基础小车、轻量传感器（Blink、I2C 测温），该预算极为轻盈（总消耗 < 8KB）。
  - 但一旦引入中大型 ESP-IDF 业务应用（如带 MQTT 客户端、HTTP WebServer、多传感器聚合任务、cJSON 深度解析），任务数轻松突破 12~16 个，队列单包易超 512B。这会导致 `xQueueCreate` 或 `xTaskCreate` 静默返回 `NULL`，触发应用初始化崩溃。

#### 2. 解决方案 (Solutions)
- **短期应对（CMake 分级配置 Profile）**：
  - 维持“零运行期 malloc”底线，但将硬编码常数解耦为 CMake 编译配置项。提供三种标准化容量档位（Profile）：
    ```cmake
    # CMakeLists.txt 配置档位示例
    set(WINK_ESP_SIM_PROFILE "LITE" CACHE STRING "Sim memory footprint profile (LITE, STANDARD, PRO)")
    
    if(WINK_ESP_SIM_PROFILE STREQUAL "LITE")
        add_compile_definitions(CONFIG_FREERTOS_MAX_TASKS=8 CONFIG_FREERTOS_MAX_QUEUES=8 CONFIG_FREERTOS_QUEUE_STORAGE=512)
    elseif(WINK_ESP_SIM_PROFILE STREQUAL "STANDARD")
        add_compile_definitions(CONFIG_FREERTOS_MAX_TASKS=16 CONFIG_FREERTOS_MAX_QUEUES=24 CONFIG_FREERTOS_QUEUE_STORAGE=2048)
    elseif(WINK_ESP_SIM_PROFILE STREQUAL "PRO")
        add_compile_definitions(CONFIG_FREERTOS_MAX_TASKS=32 CONFIG_FREERTOS_MAX_QUEUES=64 CONFIG_FREERTOS_QUEUE_STORAGE=8192)
    endif()
    ```
- **中长期演进（构建期静态容量推导）**：
  - 在前端或打包工具链中静态扫描应用工程的 FreeRTOS 原语声明，自动生成与该应用尺寸完全贴合的 `sdkconfig_sim_caps.h`，实现按需生成静态池。

---

### [ISSUE-04] 虚拟中断 (ISR) 语义降级与异步事件注入难题

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  真实嵌入式设备是强事件/中断驱动的。社区大量的开源设备驱动（如旋转编码器、MPU6050 中断脚、超声波捕获、红外遥控接收器）均依赖：
  1. `gpio_install_isr_service()` 与 `gpio_isr_handler_add()` 挂载中断回调；
  2. 中断服务程序中调用 `xQueueSendFromISR` / `xSemaphoreGiveFromISR` 并依据 `pxHigherPriorityTaskWoken` 触发即时上下文切换。
- **潜在摩擦与风险**：  
  当前矩阵中中断系列全部降级或未支持，`xHigherPriorityTaskWoken` 恒定返回 `pdFALSE`。若直接将依赖硬件中断的第三方原生 C 库拖入仿真，业务代码将无法正常收到事件触发，状态机卡死。

#### 2. 解决方案 (Solutions)
- **短期应对（轮询垫片替代与文档说明）**：
  - 对于测试语料和常用传感器（如外部按键触发），优先提供基于定时器/轮询的降级示例；
  - 在 `02-api-coverage-matrix.md` 中维持降级条目的透明诚实披露，禁止伪造“中断成功安装”。
- **中长期演进（虚拟中断事件泵 - Virtual ISR Event Pump）**：
  1. **构建中断分发表**：
     门面层维护按引脚索引的中断回调链表：
     ```c
     typedef struct {
         gpio_isr_t isr_func;
         void*      args;
         gpio_int_type_t intr_type;
     } esp_sim_gpio_isr_slot_t;
     ```
  2. **事件泵调度集成（ISR Event Pump）**：
     在 `sim_scheduler` 每次推进虚拟时钟前，允许外部仿真宿主（如前端浏览器 UI 点击按键、滑块拖动）向事件队列推入“GPIO 边沿跳变事件”；调度器前置执行已绑定的 `isr_func`，若 `xHigherPriorityTaskWoken` 被置为 `pdTRUE`，调度器主循环立即切入被唤醒的高优先级纤程。

---

### [ISSUE-05] 原厂 SDK 跨大版本升级的 API 签名分叉

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  ESP-IDF 在重大版本升级时会重构驱动 API：
  - **v5 迁移至 v6**：`gpio_uninstall_isr_service` 的官方返回值从 `void` 变更为 `esp_err_t`；
  - **I2C 驱动重构**：v5.1 起引入全新的 `driver/i2c_master.h`（对象句柄化），逐步废弃 `driver/i2c.h`（Legacy 端口号驱动）；
  - **Log 系统统一**：v6 起统一收拢为 `esp_log()` / `esp_log_va()`。
- **潜在摩擦与风险**：  
  如果门面垫片与官方原厂 SDK 版本未建立严格的映射锚点，手写或收割的头文件可能出现与用户业务代码预期不匹配的编译错误（如函数签名冲突）。

#### 2. 解决方案 (Solutions)
- **短期应对（明确单一锚点与双通道兼容）**：
  1. **严格绑定单一基准**：当前框架坚定以 `v6.1@fff9895c` 为单一真相源（`manifest.hash` 强制校验），`esp_get_idf_version()` 准确上报 `v6.1-dev-winksim`。
  2. **保留 Legacy 桥接层**：如当前工程同时提供 `src/drivers/esp_i2c_legacy.c` 与 `src/drivers/esp_i2c_master.c`，确保老代码与新式 API 均能向下收拢至同一个 PAL I2C 实体。
- **中长期演进（基于收割器的版本化矩阵）**：
  在闭源 Harvester 中预设版本标签（Tag-based Generation）。若未来需向前兼容长期维护的 v5.1 LTS 语料，通过切换 Harvester 规约生成对应的兼容垫片。

---

### [ISSUE-06] 纯忙等死循环导致虚拟时钟冻结死锁 (Spin-Wait Starvation)

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  在真实物理硬件上，单片机外设是独立时钟硬件推进的。C 代码中充斥着这种写法：
  ```c
  // 典型传感器/电平轮询（如等待传感器就绪、读取脉冲响应）：
  while (gpio_get_level(READY_PIN) == 0) {
      // 纯死循环轮询，在物理芯片上毫秒级内硬件就会把引脚拉高
  }
  // 或者基于硬件定时器的短延时：
  int64_t start = esp_timer_get_time();
  while (esp_timer_get_time() - start < 100) {} // 忙等 100 微秒
  ```
- **潜在摩擦与风险**：  
  在 Host/Wasm 的单虚拟核协作调度（Cooperative Fiber）模型下，**只有在任务显式调用调度让步原语（如 `vTaskDelay`、`xQueueReceive`）时，虚拟时钟才会推进，其他任务或前端事件才有机会执行**！
  如果用户代码写了一个不带 `vTaskDelay()` 的纯忙等 `while()`，整个线程将陷入死循环；`READY_PIN` 永远不会被外部拉高，`esp_timer_get_time()` 虚拟时钟永远不会前进一步；导致浏览器标签页彻底无响应（卡死假死）或 CPU 跑满被杀。

#### 2. 解决方案 (Solutions)
- **短期应对（忙等检测与防饿死让步）**：  
  在门面层的只读轮询 API（如 `gpio_get_level`、`esp_timer_get_time`、`esp_rom_delay_us`）中引入**“忙等检测计数器”**。一旦检测到同一纤程在未切出状态下连续读取超 500 次，立即强制触发一次 `sim_scheduler_yield_context()` 让出纤程，并向前推移 10µs 虚拟时间。
- **中长期演进（执行配额看门狗）**：  
  引入类似 ADR-0072 的“执行配额片看门狗（Execution Slice Quota Watchdog）”。当单个纤程连续执行超过 10ms 物理墙钟而未让步时，强行注入异步异常断言，直接打印源码行号报警：“检测到无让步忙等死锁”。

---

### [ISSUE-07] 非易失存储与文件系统的持久化断层 (NVS & FS Disconnect)

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  任何严肃的 ESP-IDF 业务（Wi-Fi 配网凭据保存、传感器校准参数、设备配对密钥、以及 LittleFS / SPIFFS 托管网页前端资源），必须读写 Flash 分区和 NVS。
- **潜在摩擦与风险**：  
  1. 当前 `esp_nvs.c` 仅硬编码了一个 16 条目的内存数组 `s_nvs_storage`，**没有持久化**：用户在仿真网页中配好了 Wi-Fi 或改了参数，只要一按 F5 刷新网页，配置全部归零；
  2. **缺乏分区表（Partition Table）概念**：如果应用调用 `esp_partition_find()` 或尝试挂载 SPIFFS 读取前端资源文件，直接报错阻断。

#### 2. 解决方案 (Solutions)
- **短期应对（宿主持久化桥）**：  
  将 `src/core/esp_nvs.c` 对接宿主持久化：
  - 在 Host 原生环境下落盘为本地 JSON/二进制文件；
  - 在 Wasm 浏览器环境下，通过 `wasm_bridge` 对接浏览器的 `localStorage` 或 `IndexedDB`，实现浏览器刷新后参数依然保留。
- **中长期演进（虚拟分区镜像 Blob）**：  
  引入**虚拟分区镜像（Virtual Partition Blob）**机制。在构建期允许把网页资源打包为只读虚拟 SPIFFS 镜像，作为二进制资源挂载到 Wasm 内存中，完全还原官方文件系统读写 API。

---

### [ISSUE-08] 用户态与三方库的堆能力语义缺失 (`heap_caps_*` & PSRAM)

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  框架门面源码贯彻了“0 malloc”红线，但这管不住上层业务代码。一旦用户引入 ESP-IDF 常用三方库（如 cJSON、mbedtls、LVGL 图形库、各类协议解析器），这些库内部充斥着动态内存分配，更关键的是乐鑫特有的内存能力 API：
  ```c
  void* buf = heap_caps_malloc(4096, MALLOC_CAP_SPIRAM); // 强制在片外 PSRAM 分配
  void* dma_buf = heap_caps_malloc(1024, MALLOC_CAP_DMA); // 分配 DMA 对齐内存
  size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  ```
- **潜在摩擦与风险**：  
  Wasm 只有一个平坦的线性内存空间（Linear Memory），根本没有物理 PSRAM、SRAM 和 DMA 物理对齐概念。如果当前框架不提供这套头文件与桩函数，包含这些代码的官方示例在编译期就会直接全军覆没。

#### 2. 解决方案 (Solutions)
- **短期应对（堆能力降级映射）**：  
  提供完整的 `esp_heap_caps.h` 门面：
  - 将 `heap_caps_malloc()` 透传降级映射为标准 `malloc()`；
  - 对于 `MALLOC_CAP_SPIRAM`，若模拟芯片具备该能力（如 S3 配置带 PSRAM），正常分配并扣减虚拟 PSRAM 配额计数；若目标芯片不支持则返回 `NULL`（合约诚实）。
- **当前契约（ADR-0089，取代本节旧的“全能力统一 malloc”描述）**：普通分配走 libc 且不进入有限 tracker；DMA、SPIRAM 与超基线对齐使用分类簿记。普通域水位是容量提示，不是实测 SSOT。特殊分配必须通过 `heap_caps_free` 释放，以兼容 MSVC 的 `_aligned_malloc/_aligned_free` 配对；本机未安装 MSVC 时，该释放路径仍需 Windows CI 验证。
- **中长期演进（内存水位监控仪）**：  
  建立仿真内存水位监控（Memory Watermark Inspector），在前端仪表盘实时展示应用在不同内存能力域的消耗波形，帮助开发者在仿真期就提前发现“物理硬件内存泄漏”。

---

### [ISSUE-09] 仿真软复位 (`esp_restart`) 导致的用户全局状态脏重入

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  物理单片机调用 `esp_restart()` 时，硬件电路触发 POR（上电复位），引导程序执行：**将 `.bss` 段重新全清零，将 `.data` 段的初值从 Flash 重新拷贝覆盖到 SRAM**。
- **潜在摩擦与风险**：  
  在 Wasm / Host 仿真环境下，当前 `esp_restart()` 触发的是**局部软复位**：清理 HTTP/MQTT、Wi-Fi/Netif/NimBLE、事件循环、scheduler fibers、外设、易失 NVS 句柄及 FreeRTOS 池；它不重置进程/Wasm 实例中的用户静态存储：
  - **用户业务代码中的全局变量、静态局部变量（如 `static int s_state = 1;`）根本没有被重置**！
  - 如果用户代码在复位前把全局变量修改成了错误状态，复位后重入 `app_main`，代码读取到的依然是复位前的“脏数据”；
  - 开发者在仿真环境中会百思不得其解：“为什么我的单片机重启了，变量状态却没有恢复初始值？”

#### 2. 解决方案 (Solutions)
- **当前契约**：
  文档与 UniSim 输出必须称作局部软复位，不能声称复位了用户全局变量。宿主在 scheduler 主边界处理 pending reset；复位先撤销异步 producer/token，再销毁 scheduler fibers 和易失资源。已提交 NVS 数据保留。
- **Phase 4 正确性 oracle**：
  Wasm 完整复位以销毁旧模块并重新实例化为准，重新运行 `.bss/.data` 初始化和 C++ 构造器。暂不以线性内存快照代替；只有证明 `memory.grow`、宿主闭包/句柄、异步任务、持久化存储及初始化次序等价后，才重新评估快照方案。

---

### [ISSUE-10] 架构几何失真与 64 位指针截断 (ABI & Pointer Width Mismatch)

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  当前框架支持多编译目标：ESP32 物理硬件 (32位 ILP32)、浏览器 Wasm (32位 ILP32) 以及 本地开发调试/CI 测试的 Host Native (64位 LP64/LLP64)。
- **潜在摩擦与风险**：  
  1. **指针强转整型截断**：FreeRTOS 开发者常将指针强转为 `uint32_t` 传参（如队列投递）。在 64 位 Host 环境下，`sizeof(void*) = 8`，强转导致指针高 32 位被无声截断，解引用直接 SIGSEGV 段错误；
  2. **结构体对齐空洞（Padding）**：64 位系统按 8 字节对齐，32 位按 4 字节对齐。二进制协议结构体在两端 `sizeof` 和内部字段偏移量不相等，导致协议反序列化错位。

#### 2. 解决方案 (Solutions)
- **短期应对（类型强制与严格告警）**：  
  门面全面推行 `uintptr_t` 严格审计。在编译 Host 目标时强制开启 `-Wpointer-to-int-cast` 和 `-Wint-to-pointer-cast` 警告并视为错误（`-Werror`）。
- **中长期演进（Wasm32 黄金基准）**：  
  将 CI 测试与仿真的黄金标准锚定在 **wasm32-unknown-emscripten**。因为 Wasm 是纯正的 32 位平坦地址空间，其数据对齐、指针长度与 ESP32 硬件 100% 几何同构，物理消除 64 位脏数据。

---

### [ISSUE-11] Wasm Asyncify 深度栈展开与重入炸弹 (Asyncify Trap & Re-entrancy Bomb)

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  在浏览器 WebAssembly 虚拟机中，Wasm 规范禁止直接操纵控制流栈指针，目前 Emscripten 依赖 **Asyncify** 机制模拟协程切换。
- **潜在摩擦与风险**：  
  1. **展开开销（Unwind Tax）**：每次 `vTaskDelay` 会从当前函数逐层退出并序列化栈上变量，恢复时重跑压栈，导致代码体积膨胀 30%~50%，调度开销放大百倍；
  2. **重入炸弹（Re-entrancy Bomb）**：若在 Asyncify 正在展开调用栈的微秒瞬间，外部前端事件或宿主定时器尝试调用 Wasm 导出函数，Wasm 会直接抛出 `RuntimeError: unreachable` 或 `Asyncify: already sleeping/unwinding`，导致仿真彻底崩溃且无法被 JS 捕获。

#### 2. 解决方案 (Solutions)
- **短期应对（状态锁与事件暂存屏障）**：  
  在 `wasm_bridge` 边界维护状态锁。检测到 Wasm 处于 Unwind/Rewind 状态时，外部所有注入事件强制进入暂存队列，直到 Wasm 完全静止才派发。
- **中长期演进（演进至 Wasm JSPI）**：  
  迁移至浏览器最新的原生 **Wasm JSPI (JavaScript Promise Integration)** 规范。利用 V8/SpiderMonkey 原生协程机制替代编译期插桩展开，彻底解决重入炸弹并恢复原生满速性能。

---

### [ISSUE-12] 微秒级单总线协议的“时钟抖动破产” (Microsecond Bit-Banging & Jitter)

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  创客常用传感器（DHT11/DHT22 温湿度、WS2812 幻彩灯带）依赖微秒甚至纳秒级 GPIO 翻转与延时（`esp_rom_delay_us(30)`）。
- **潜在摩擦与风险**：  
  在浏览器或宿主操作系统中，事件循环与线程调度抖动（Jitter）至少在毫秒量级（1~10ms）。微秒级引脚模拟会严重漂移，导致单总线协议 100% 出现 CRC 校验错误或超时失败。

#### 2. 解决方案 (Solutions)
- **架构裁决**：严禁在仿真器中以“原始引脚翻转”的方式模拟微秒级时序敏感协议。
- **中长期演进（外设级语义提升 - Semantic Peripheral Elevation）**：  
  将 WS2812 与 DHT11 提升为事务/数据帧总线（Channel 2/4）。门面层直接提取 RGB 颜色数组或温湿度结构体，一次性抛给前端渲染引擎，绕开底层的微秒引脚翻转。

---

### [ISSUE-13] C++ 全局对象静态构造的时序倒挂 (Static Constructors Inversion)

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  现代 ESP-IDF 与 Arduino-ESP32 库高度依赖 C++。全局作用域下的静态对象构造函数（Static Initializers）由 CRT 在进入 `main` / `app_main` 之前隐式执行。
- **潜在摩擦与风险**：  
  如果用户在 C++ 全局构造函数中调用了 `gpio_config()` 或 `xSemaphoreCreateMutex()`，此时 WinkMicroOS 的调度器与资源池尚未初始化，代码将在进入 `app_main` 之前在底层触发空指针解引用崩溃。

#### 2. 解决方案 (Solutions)
- **短期应对（防御性自愈初始化）**：  
  在 `src/freertos/` 和 `src/drivers/` 的公开 API 入口处加入原子级判定：
  ```c
  static bool s_inited = false;
  static inline void ensure_subsystem_ready(void) {
      if (!s_inited) { esp_idf_framework_init(); s_inited = true; }
  }
  ```
  确保即使全局构造函数提前触发，底层资源池也能实现“即时自愈式按需冷启动”。
- **中长期演进**：在收割器或构建门禁中增加对 ELF `.init_array` 段的符号扫描，建立准入检查机制。

---

### [ISSUE-14] 宿主 POSIX 符号污染与本地磁盘穿透风险 (Host POSIX Collision)

#### 1. 问题描述 (Problem Description)
- **核心机理**：  
  ESP-IDF 提供了 POSIX 兼容虚拟文件系统（VFS）：开发者可以直接调用标准 C 函数 `open("/spiffs/cfg.json", O_RDWR)`、`read()`、`write()`。
- **潜在摩擦与风险**：  
  当编译为 Host Native 时，宿主操作系统的 `libc`（Linux glibc / Windows MSVCRT）本身就导出了这些同名符号。若符号未隔离，仿真代码调用 `open("/etc/config", O_WRONLY)` 将直接绑定到宿主真实系统调用，**直接击穿沙箱，篡改或删除开发者电脑物理硬盘上的真实文件**，造成重大安全与数据风险。

#### 2. 解决方案 (Solutions)
- **短期应对（符号重命名隔离）**：  
  在 Host 构建时，所有 VFS 接口必须强行重命名（通过宏定义注入，如 `#define open esp_vfs_open`），严禁使用全局裸符号暴露给链接器。
- **中长期演进（纯沙箱化隔离）**：  
  所有文件与存储操作统一收拢到底层的虚拟块设备抽象层，在 Host 下仅映射到固定的隔离沙箱目录（如 `.sim_sandbox/`），在 Wasm 下严格受限于浏览器的虚拟内存文件系统（MEMFS）。

---

## 3. 演进路线图与阶段规划 (Roadmap)

| 阶段 | 关注瓶颈 | 核心交付物 | 验收准则 / 状态 |
|:---|:---|:---|:---|
| **Phase 1 (M3收尾~P1)** | ISSUE-03 静态池容量<br>ISSUE-10 64位指针截断 | CMake `WINK_ESP_SIM_PROFILE` (LITE/STANDARD)<br>全面引入 `uintptr_t` 与 `-Wpointer-to-int-cast` 门禁 | ✅ **已验收合入**（编译宏可控；大应用队列不返 NULL；Host 64位无指针截断告警） |
| **Phase 2 (P2)** | ISSUE-02 并发假阳性<br>ISSUE-06 忙等死锁<br>ISSUE-13 C++ 构造时序 | 自旋锁状态记录、跨上下文让步断言报警 (`portMUX_TYPE`)<br>门面忙等计数自愈让步 (`esp_sim_spin_wait_account`)<br>公开 API 增加幂等按需冷启动 (`esp_idf_ensure_framework_ready`) | ✅ **已验收合入**（捕获临界区非法阻塞；纯死等推进虚拟时钟不卡死；C++ 全局对象安全构造；全量单测 100% 通过） |
| **Phase 3 (P3)** | ISSUE-04 虚拟中断支持<br>ISSUE-07 NVS 持久化<br>ISSUE-08 堆能力门面<br>ISSUE-14 沙箱文件隔离 | 门面级虚拟中断注册表 + 双上下文事件泵 (`esp_sim_gpio_inject_edge`)<br>NVS 二进制快照 CRC32 守护 + 受控沙箱原子落盘 (`.sim_sandbox/`)<br>`esp_heap_caps.h` 零指针侵入静态簿记门面 (libc `free` 兼容)<br>FreeRTOS 原语优先级唤醒与即时纤程抢占 | ✅ **已验收合入**（虚拟 GPIO 中断支持电平/边沿检测与双上下文安全分发；NVS 具备完整 CRUD 与掉电冷重启恢复；堆能力提供 DMA 32 字节对齐与 PSRAM 诚实鉴权；全量 48 项 CTest 100% 通过） |
| **Phase 4 (长期预研)** | ISSUE-01 复杂外设代偿<br>ISSUE-09 软复位快照<br>ISSUE-11 演进 JSPI<br>ISSUE-12 语义级单总线 | 宿主网络隧道（WebSocket 代理至 `esp_netif`）<br>Wasm 线性内存快照 (Snapshot) 热重载<br>迁移至 Wasm JSPI 规范<br>WS2812/DHT 提升为 Channel 2/4 语义总线 | 📋 待预研 |

> 📌 **Phase 3 实施结项声明**：
> Phase 3 成功闭环了 ISSUE-04、ISSUE-07、ISSUE-08 与 ISSUE-14：
> 1. **虚拟中断与抢占安全**：基于 `s_gpio_isr_slots` 建立引脚服务表，打通 `pal_os_set_sim_isr_context()`，FreeRTOS 原语全面升级为最高优先级唤醒与 `portYIELD_FROM_ISR` 即时抢占；外部主线程注入严禁调用 `sim_scheduler_yield_context()` 彻底杜绝崩溃（R-302）。
> 2. **受控沙箱持久化**：NVS 升级为跨平台原子落盘（Windows `unlink` + `rename`），引入 streaming CRC32 完整性校验，数据收敛于受控目录 `.sim_sandbox/`。
> 3. **零指针侵入堆能力**：`esp_heap_caps.c` 采用独立静态簿记表，返回合法原始 `malloc` 裸指针，彻底消除系统 `free()` 崩溃（R-304），并与 `esp_get_free_heap_size()` 实现 SSOT 水位统一。

---

## 4. 结论与架构师守则

ESP-IDF 仿真拦截层的本质是**“业务行为的忠实映射器”**，而非**“芯片物理特性的像素级模拟器”**。

在后续演进中，必须坚守以下守则：
1. **绝不为实现单一高级驱动而打破分层红线**（严禁门面侵入 PAL 内部，严禁真机构建产生增量）；
2. **坚持“合约诚实（ADR-0012）”**：宁可在编译期通过 SLA 拒绝或运行时 Fail-Loud 报错，也绝不为了跑通某个语料而编写虚假的静默空函数；
3. **保持资产通道与机器生成不可被手写污染（ADR-0087）**：所有新驱动的类型声明必须由收割器产出，手写垫片必须严谨登记入 `channels.json`；
4. **守住物理与虚拟沙箱边界**：严禁微秒级裸引脚翻转污染事件循环，严禁未沙箱化的 POSIX 符号穿透破坏宿主安全。
