# 时序基础设施与 DAL Roadmap —— 代码评审报告

- **评审日期**: 2026-08-23
- **评审基线**: `master` @ `099759d`（feat(dal): Stage 4 DAL peripheral rollout）
- **评审范围**: `docs/todolist/timing-infrastructure-and-dal-roadmap/00~05` 全部 Stage 0–4
- **评审方法**: 逐文件源码核查 + 跨文件符号 grep 验证 + 三个并行子代理深度审计（Stage 0 / Stage 1-2 / Stage 3-4）
- **状态图例**: ✅ DONE / ⚠️ PARTIAL / ❌ MISSING / 🐛 BUG

---

## 0. 总体结论

提交历史（5 个 stage commit）给人"全部完成"的印象，但跨文件验证显示：

> **骨架工程质量高，但 Stage 2 三大硬件驱动形似实无，Stage 3 存在致命接线缺陷，Stage 4 多项 DAL 无法链接。**

- **Stage 0/1/2 的基础设施层**（spinlock、resource、atomic、lockfree pipeline、IRAM 宏、错误码、位域纪律）完成度高，是全 roadmap 质量最好的部分。
- **Stage 2 的 hwtimer/mcpwm/adc continuous** 处于"API 头文件齐全、但 ESP32 实现忽略关键配置、安全功能未接通"的状态。其中 MCPWM 死区为 0 + 硬件异步刹车未连接，**若接到真实电机桥臂有直通炸管风险**。
- **Stage 3 的 Wasm 异步完成队列与 hwtimer 软步进本身实现正确，但从未被虚拟时钟 gate 调用**——真实 wasm 运行中所有 DMA 回调和定时器回调永不触发。这是两行代码的接线遗漏，但导致整个 Stage 3 功能不可用。
- **Stage 4** 实际只新增了 4 个 DAL（gps/eeprom/ws2812/audio），其中 ws2812 引用全代码库不存在的符号，gps 是无解析的轮询骨架；约 16 类计划中的 DAL 未实现；CI 三层门禁基本缺失。

**八条红线体检**: 3 条破线（临界区 WCET 无举证且 SPI 持锁阻塞、DAL 出现 `#ifdef ESP_PLATFORM`、快环 Core1 亲和未实现），1 条以"功能不发生"的方式虚假满足（wasm 同步回调红线——回调根本不触发），4 条守住。

---

## 1. Stage 0 —— PAL 硬件加速引擎

### 1.1 T0.1a pal_spinlock —— ✅ DONE

- `wink-micro-os/pal/include/pal_spinlock.h`，header-only `static inline`。
- ESP32 分支使用 `portMUX_TYPE` + `taskENTER_CRITICAL`；lock/unlock 均带 `configASSERT(!xPortInIsrContext())`；另有 `_isr` 变体使用 `taskENTER_CRITICAL_ISR`。
- Host/Wasm 退化为 signal fence；额外覆盖 `_WIN32`。
- grep 确认 `portMUX_TYPE` / `portMUX_INITIALIZER` 仅出现在该头文件内，原有 5 个散落文件已全部收敛。
- `targets/esp32/pal_resource_esp32.c:25` 正确使用 `pal_spinlock_t s_resource_mux = PAL_SPINLOCK_INITIALIZER`。

### 1.2 T0.1b IRAM/DMA 宏 —— ⚠️ PARTIAL

- `pal/include/wink_compiler.h:34-38`：`PAL_IRAM_TEXT/DATA/RODATA/DMA_ATTR/DMA_BUF_ATTR` 五个宏齐全，ESP32 上映射到 `IRAM_ATTR/IRAM_DATA_ATTR/WORD_ALIGNED_ATTR DRAM_ATTR`，其他 target 为空。
- `pal/include/pal_irq.h:85`：`#define PAL_ISR PAL_IRAM_TEXT` 正确。
- 🐛 **4 处 ESP32 ISR 仍使用裸 `IRAM_ATTR`**，未统一为 `PAL_ISR`/`PAL_IRAM_TEXT`：
  - `targets/esp32/pal_hal_hwtimer_esp32.c:31`
  - `targets/esp32/pal_hal_pcnt_esp32.c:41`
  - `targets/esp32/pal_hal_rmt_esp32.c:48` 和 `:64`
- 无裸 `DRAM_ATTR` 散落到 `wink_compiler.h` 之外（良好）。

### 1.3 T0.2 pal_resource 仲裁 —— ✅ DONE（小缺口）

- `pal/include/pal_resource.h:33-44`：枚举扩到 18 类（SPI_BUS=7 … GDMA_CHAN=18）。
- `pal_resource_max(type)` 在 `targets/common/src/pal_resource.c:24`（host/wasm）与 `targets/esp32/pal_resource_esp32.c:33` 均实现；claim 对 `id >= max` 返回 `WINK_ERR_INVALID_ARG`。
- Host/wasm 返回硬件真实最大值（SPI=2/PCNT=8/RMT=8/HWTIMER=4/MCPWM=2/UART=3），非 SIM_MAX 放大值。
- `pal_resource.h:65-72` 的 `_Static_assert` 块覆盖 SPI_BUS=2、PCNT_UNIT=8、MCPWM_UNIT=2、RMT_CHAN=8、HWTIMER=4、UART=3，比计划要求更全。
- `PAL_RESOURCE_MAX_CLAIMS = 64`（行 106），满足 ≥64。
- `pal_resource_i2c_id()`、`pal_resource_mcpwm_id(unit, sub_id)`（行 90-92）辅助函数到位。
- 小缺口：计划描述的 `PAL_RESOURCE_HOST_MAX_OVERRIDE` CMake 逃生口未实现（host 始终用硬件真实上限）。这是更安全的默认值，但文档化的逃生口缺失。
- 注：`SPI_CS`、`MCPWM_SYNC_GPIO`、`GDMA_CHAN` 返回 `UNLIMITED_MAX`，可接受（GPIO-backed 或非 Stage 0 claim）。

### 1.4 T0.3 PAL SPI —— ⚠️ PARTIAL / 🐛 BUG

**API 与基础设施（到位）**:
- `pal/include/hal/pal_spi.h`：总线配置字段为 `spi_bus`（非 `bus_id`，行 32）；设备配置含全部六个字段（含 `cs_setup_ns/cs_hold_ns`）；`pal_spi_add_device(uint8_t bus, const pal_spi_device_config_t *cfg, handle*)`（行 93）；`transfer_dma(dev, tx, rx, len, cb, arg)` + `transfer_polling`。
- 静态句柄池，热路径无 malloc：ESP32 `s_buses[PAL_SPI_BUS_MAX]` 内嵌 `devices[PAL_SPI_DEV_MAX_PER_BUS]`；host 对等结构。
- 第三路总线返回 `WINK_ERR_INVALID_ARG`（esp32:61, host:97）。
- Host 测试桩 `stub_spi_inject_rx/get_last_tx/force_failure/reset` 到位，RX 来自注入环 + 0xFF 空闲回退，非 naive loopback。

**🐛 BUG（wasm 无法编译）**:
`targets/wasm/pal_wasm_ch2_spi.c` 与当前头文件不匹配：
- 行 30：读 `cfg->bus_id`，但 `pal_spi_bus_config_t` 字段是 `spi_bus`。
- 行 49：定义 `pal_spi_add_device(const pal_spi_device_config_t *cfg, handle*)`，**缺少头文件要求的第一个参数 `uint8_t bus`**。
- 行 50、66：读设备配置里不存在的 `cfg->bus_id`。

该翻译单元在当前头文件下**无法编译**。

**🐛 BUG（ESP32 并发/延迟）**:
- `targets/esp32/pal_hal_spi_esp32.c:256`：在持有 `s_spi_lock`（`taskENTER_CRITICAL` 自旋锁）期间调用 `spi_device_queue_trans(..., portMAX_DELAY)`，无限期阻塞。在自旋锁/临界区内阻塞是死锁与延迟尖峰隐患，直接冲击红线 2（临界区 <100µs）。

**🐛 BUG（配置静默丢弃）**:
- `cs_setup_ns/cs_hold_ns` 被接收但从未映射到 `spi_device_interface_config_t.cs_setup_cycles/cs_hold_cycles`（esp32:172-178）。

**次要**:
- Host `pal_spi_deinit_bus`（行 142）对每个在用设备释放 `SPI_CS` 时未检查 `cfg.cs_pin >= 0`；`cs_pin = WINK_PIN_NC`（-1 转 uint32 = 65535）会释放一个假 claim。对比 `add_device` 只在 `cs_pin >= 0` 时 claim。
- wasm 完成回调适配器用 per-device 静态槽 `s_adapt[slot_idx]`（行 115-121）：同一设备上两个重叠 DMA 传输会互相覆盖 `user_cb/user_arg`，第一个完成会调第二个的回调。完成队列契约是 32 个独立项，不是每设备 1 个。
- 行 117 防御性 `if (slot_idx >= WASM_SPI_DEV_MAX) slot_idx = 0;` 会在不可达分支里别名到槽 0，危险。

### 1.5 T0.4 PAL RMT —— ⚠️ PARTIAL / 🐛 BUG

**到位**:
- 多通道 handle API（`acquire_channel/release_channel/tx_send/rx_set_callback/rx_start/rx_stop`）。
- `pal_rmt_symbol_t` 使用独立 `uint16_t duration0/duration1` + `uint8_t level0/level1` + `_pad[2]`，**无 C 位域**（符合 ADR-0002）。
- `pal_rmt_make_reset_symbol(resolution_hz, hold_low_us)` 静态内联助手，32767 tick 饱和，uint64 中间值防溢出。
- 遗留 singleton pulse-capture API 保留兼容。

**🐛 BUG / ❌ MISSING**:
- ❌ **DMA 不可配置**：计划的 `pal_rmt_channel_config_t.dma_enabled` 字段缺失（`pal_rmt.h:56-61`），ESP32 `rmt_tx_channel_config_t`（esp32:128）从不设置 `with_dma`。长 WS2812 灯条 DMA TX（Stage 0 明示目标）无法开启。
- ❌ **无 wasm RMT TU**：`targets/wasm/` 下无 `pal_wasm_rmt.c`。任何引用 RMT 的 wasm 构建链接失败。按 ADR-0012 至少需要一个返回 `WINK_ERR_UNSUPPORTED` 的桩 TU。
- 🐛 **`pal_rmt_rx_stop` ESP32 端是空 no-op**（`pal_hal_rmt_esp32.c:304-309`）：仅 NULL 检查后返回 OK，从不 `rmt_disable/rmt_rx_stop`，"停止"后接收继续。
- 🐛 **持锁内大栈分配 + 阻塞调用**：`pal_rmt_tx_send` 在持有 `s_rmt_lock`（自旋锁/临界区）时栈分配 `rmt_symbol_word_t sym_words[256]`（1024 字节，行 239）并调用 `rmt_transmit`（行 252）。
- 🐛 **ISR 工作量过大**：`esp32_rmt_rx_done_cb`（行 64）在 ISR 上下文循环转换最多 256 个 symbol 并直接调用用户回调（可能非平凡），违反文档化的 <10µs ISR 契约。
- 🐛 两个 RMT ISR 使用裸 `IRAM_ATTR`（行 48、64）而非 `PAL_ISR`。
- 通道 `channel_id` 配置字段缺失（自动池分配），与计划 API 分歧，但属合理设计选择。
- `pal_rmt_pulse_capture_init:335` 调用 `xSemaphoreCreateBinary()`，init 路径有运行时 malloc。

### 1.6 T0.5 PAL PCNT —— ⚠️ PARTIAL / 🐛 BUG

**到位**:
- X1/X2/X4 模式（`PAL_PCNT_MODE_1X/2X/4X`）、静态池无 malloc、64 位累加器字段（`volatile int64_t accum_count`，esp32:33）。
- watch point 高/低限软件累加（行 169-170）、glitch filter 配置入口。

**🐛 BUG（64 位累加非原子，双核撕裂读）**:
- ISR 行 48/51：`u->accum_count += u->high_limit/low_limit`，普通读写；读取行 246 在自旋锁下普通读。Xtensa 是 32 位核，**无法原子访问 int64_t**，双核下存在 64 位撕裂读。计划明确要求 `__atomic_fetch_add(&accum, ..., __ATOMIC_RELAXED)` 与 `__atomic_load_n(&accum, __ATOMIC_ACQUIRE)`，未实现。
- ISR 内 `pcnt_unit_clear_count` 与累加非原子复合，阈值命中到 ISR 进入之间的脉冲会丢；且加的是阈值常量而非中断时刻硬件实际计数。

**🐛 BUG（E-001 硅片缺陷未缓解）**:
- 计划要求 `glitch_filter_ns` 默认 ≥1000ns。配置默认 0（关闭），`pal_pcnt_init` 与 `set_glitch_filter` 均未强制 1000ns 下限。头文件注释（行 13-14）声称有钳制但代码未做。
- 更糟：单测 `test_pal_pcnt.c:58` 显式使用 `filter_ns = 500`，把违规路径固化。

**🐛 BUG（未检查返回值）**:
- 4X 模式下 `pcnt_new_channel`（esp32:152）返回值未检查，失败时 `chan_b` 被未初始化使用。

**🐛 BUG（wasm 旧 API，无法编译）**:
`targets/wasm/pal_wasm_pcnt.c` 来自旧 API：
- 行 22 用 `pal_pcnt_handle_t`，头文件是 `pal_pcnt_unit_handle_t`（`pal_pcnt.h:46`）。
- 行 23、26 引用 `cfg->unit`，`pal_pcnt_config_t` 无此字段。
- 定义 `pal_pcnt_start/stop/clear_count`（行 39/46/60），头文件只有 `clear`，无 `start/stop`。

**API 分歧**: 实际为 `init/deinit/get_count/clear/set_glitch_filter` 且 init 内立即 `pcnt_unit_start`（行 179）；计划要求 `acquire/release/start/pause/clear_count`，无 pause。命名也从 `PAL_PCNT_COUNT_SINGLE/QUAD_X1/X2/X4` 改为 `PAL_PCNT_MODE_1X/2X/4X`。

### 1.7 T0.6 PAL UART 异步化 —— ❌ MISSING（核心特性缺失）

- 旧同步 API（`init/deinit/read/write`）保留，三个 target 均实现。
- wasm 复用既有 push 符号（`pal_wasm_push_uart_rx_byte` 等）。
- ❌ **计划的 headline 特性全部缺失**：
  - 无 `pal_uart_open/close` handle 模型，无 `pal_uart_handle_t`、`pal_uart_config_t`（含 `rx_ring_buffer_bytes/rx_fifo_threshold/idle_to_us`）。
  - 无 `pal_uart_handle_read/write`。
  - **无 `PAL_UART_EVT_RX_IDLE`（总线静默帧分隔）**——这是 GPS NMEA / Modbus 帧分隔的全部理由。
  - 无可编程 `RX_FIFO_HIGH` 阈值。
- 实际设计是更简单的端口级事件回调：`pal_uart_set_event_callback(port, cb, arg)`，事件枚举为 `RX_DATA/RX_FIFO_OVF/BUFFER_FULL/BREAK/PARITY_ERR/FRAME_ERR/TX_DONE`，另加 `pal_uart_write_async`。
- 🐛 **RX 数据竞争**：ESP32 事件任务每次 `UART_DATA` 用栈上 `dtmp[128]` 调 `uart_read_bytes(port, dtmp, 128, 0)`（行 46/56），而 `pal_uart_read`（行 247）也直接 `uart_read_bytes`。注册回调后字节被事件任务偷走，`pal_uart_read` 看到空缓冲，数据在两条路径间静默丢失。
- 🐛 事件任务每次最多排空 128 字节（RX ring 1024），剩余字节要等下一字节触发新事件。
- 🐛 `pal_uart_init` 用 `xTaskCreate`（行 184）动态建任务；`pal_uart_deinit` 调 `vTaskDelete`（行 203），但任务永久阻塞在 `xQueueReceive(portMAX_DELAY)` 上无退出信号，强杀时队列可能已被 `uart_driver_delete` 释放。
- 同端口互斥：`pal_uart_init` claim `PAL_RESOURCE_UART_PORT`（esp32:113），双 init 返回 BUSY；但因为 open 模型不存在，新旧互斥要求无意义。
- 次要类型不匹配：`pal_wasm_ch2_uart.c:94` 定义 `tx_pin/rx_pin` 为 `uint8_t`，头文件声明为 `wink_pin_t`（int16_t）。

### 1.8 T0.7 CMake 与版本门禁 —— ⚠️ PARTIAL

- `targets/esp32/CMakeLists.txt:31-41`：IDF ≥5.4 时 REQUIRES 含 `esp_driver_gpio/rmt/ledc/i2c/spi/pcnt/gptimer/mcpwm`。
- ❌ **`esp_driver_gdma` 未列入 REQUIRES**（计划明确列出）。
- ❌ 无编译期 `#error "ESP-IDF >= 5.4 required"`。IDF <5.4 时静默回退 `set(WINK_ESP_DRIVER_COMPONENTS driver)`（行 43），新驱动头引入时产生晦涩编译错误而非计划要求的诊断。
- 树中唯一 IDF 相关 `#error` 在 `pal_hal_i2c_esp32.c:25`，针对无关的 v7 I2C 问题。

### 1.9 T0.8 ADR-0043 lint 规则 —— ❌ MISSING

规则文件位于 `D:\...\wink-ai\packages\wink-tools\tools\lint\rules\`：

- `layering.yaml`：定义了 BAL/DAL/app/runtime/wasm 层，但**无 `pal_public` 层**，未登记任何新 PAL 头（`pal_spi/pcnt/rmt/spinlock/uart/hwtimer/mcpwm/atomic`）。Stage 0 新增 PAL 头完全不被分层规则覆盖。
- `api.yaml`：无回调类型命名规则（`_cb_t/_callback_t/_isr_t` 后缀）、无 ISR 上下文 Doxygen 校验、无公共结构体位域正则。
- `dal.yaml`：基本为空，`include_rules/api_rules/path_rules/user_surface_rules` 全是 `[]`。T4.1 计划的 DAL 规则（init/request/deinit 存在性、禁 `#ifdef SIMULATION/ESP_PLATFORM`、禁 `esp_*/arduino_*` 调用）均未编码。
- `WASM-DAL-ISOLATION` 规则（layering.yaml:130-146）只查 `SIMULATION|WASM|__EMSCRIPTEN__`，**不查 `ESP_PLATFORM`/`ESP_IDF`**，与 Stage 4 要求相反，直接放行了 4.6 节的违规。
- `DAL-HDR-NO-HAL` 规则（行 58-75）禁 DAL 公共头含 `pal_hal.h`，但有两个永久路径豁免（`dal_dc_motor.h`、`dal_encoder.h`）；`dal_ws2812.h:14` 违规 `#include "hal/pal_hal.h"` 却未被列入（latent lint 失败或匹配器 bug）。

运行 `wink lint arch --pack layering --pack api` 对 Stage 0 新增内容**真空通过**，"零错误"验收门禁无意义。

---

## 2. Stage 1 —— 时序安全、微临界区、HX711

### 2.1 T1.1 HX711 临界区 —— ⚠️ PARTIAL

**✅ DONE**:
- `dal/src/sensor/dal_load_cell.c:179-199`：`pal_os_critical_enter()/exit(crit_key)` 正确包裹 24 位移位 + gain 脉冲（计划方案 A，跨核 portMUX）。
- 符号扩展与浮点重量计算正确移出临界区（行 202-208）。
- 字段名正确：`dev->config.zero_offset`（行 208）、`calibration_factor` 作除数、`dev->last_weight_g`。无 `tare_offset_raw/last_grams/last_sample_ms`。
- `read_weight_g` 阻塞路径用 64 位差值超时（行 246-249），有 `WINK_STRICT_NONBLOCKING` 守卫（行 237）。
- 三态 API：`request_read`（153）、`is_data_ready`（134，承担 poll 角色但命名与 `request/poll/get_cached` 惯例不同）、`get_cached_raw/get_cached_weight_g`（213/225）。
- ESP32 上 `pal_os_critical_enter` 走全局 `taskENTER_CRITICAL`（跨核自旋锁 + 本地关中断），满足方案 A，但 bit-bang 期间拉长双核中断延迟。

**❌ MISSING（无举证即视为未验证）**:
- 全文无 `xthal_get_ccount()` 或任何计时测量，无测量注释。计划明确"必须用 ccount，纸面估算不接受"，要求总时长 <100µs / 单 SCK 高电平 <50µs。
- 当前全局临界区包裹是否破 HX711 60µs 掉电红线，**无证据**。无超标则升级 RMT/SPI 的退路实现。

**🐛 BUG（`tare()` 假时钟 + 截断）**:
- 行 280：`uint32_t elapsed_us = 0;`（32 位）。
- 行 290：`elapsed_us += 10000;` 手工累加而非读真实时钟。`pal_os_sleep_ms(10)` 若超时，超时会计错。
- 行 282：`dev->config.timeout_us * 2` 对大 timeout 值有 uint32 溢出风险。

### 2.2 T1.2 微临界区策略 —— ❌ MISSING

- `docs/design/02-wink-micro-os/micro-critical-section-policy.md` 不存在。
- `.claude/rules/c-code.md` 已重定向为 Skill 文档索引，所链接文件均无"临界区三禁（禁 log / 禁 malloc / 禁 delay_ms）"表。
- 全仓 grep "micro-critical-section" 仅命中计划文档本身。

### 2.3 T1.3 64 位时钟贯穿 —— ⚠️ PARTIAL

**✅ DONE**:
- load_cell 主路径用 `uint64_t start_us` 减法（246-249）。
- DAL 内无 `uint32_t x = pal_os_get_us()` 命中，无裸 `esp_timer_get_time()`。
- `test_pal_time_safety.c` 验证 32 位边界减法。

**🐛 BUG（漏网截断，49.7 天回绕）**:
- `dal/src/comm/dal_gps.c:38`：`dev->last_position.timestamp_ms = (uint32_t)pal_os_get_ms();` 显式截断 64 位时钟，71 分钟（`uint32_t` 毫秒）溢出。
- `dal/src/output/dal_relay.c:51`：`dev->pulse_start_ms = (uint32_t)pal_os_get_ms();`
- `dal/src/output/dal_relay.c:252-253`：`uint32_t now = (uint32_t)pal_os_get_ms(); uint32_t elapsed = now - dev->pulse_start_ms;` 32 位算术算脉冲宽度。
- `dal/src/input/dal_keypad.c:232`：`uint32_t now = pal_os_get_ms();` 隐式截断；行 235 按键去抖使用。

### 2.4 T1.4 ISR 禁 log 静态强制 —— ❌ MISSING

- 无 `scripts/check_isr_no_log.sh` 或任何 CI 扫描脚本。
- 无 `__attribute__((error(...)))` / `__attribute__((deprecated))` 包裹 ISR 翻译单元的 `pal_log*`。
- 无链接期符号改名（`pal_log__forbidden_in_isr`）技巧。
- `PAL_ISR`（`pal_irq.h:85`）只展开为 `PAL_IRAM_TEXT`，对同头文件 ISR 契约注释块（行 63-68）承诺的"禁 log / 禁 malloc / 禁阻塞"零编译期强制。

### 2.5 WINK_STRICT_NONBLOCKING —— ✅ DONE（局限）

- `dal_load_cell.c:237` 阻塞 `read_weight_g` 有守卫；`pal_adc.h:97` 与 `pal_adc_esp32.c:401` 阻塞 oneshot 同理。
- `test_pal_nonblocking_strict.c` 定义 `WINK_STRICT_NONBLOCKING 1` 并包含全部 DAL/PAL 头，确保严格模式编译通过。
- ⚠️ 测试只用 `sizeof(&function)` 验证符号存在，不验证 DAL 调用路径中无 `pal_os_busy_wait_us(>5)` 或 `pal_delay_ms`。

---

## 3. Stage 2 —— 快慢环、pal_hwtimer、MCPWM、无锁管道

### 3.1 T2.1 pal_atomic —— ✅ DONE（次要瑕疵）

- `pal/include/osal/pal_atomic.h`：GCC/Clang（含 ESP32 Xtensa）用 `__atomic` 内建；MSVC `Interlocked*` 兜底；C11 `<stdatomic.h>` 最终兜底。比计划提议的单独 `pal_atomic_esp32.h` 更干净。
- 内存序宏：`PAL_ACQ/REL/ACQ_REL/RLX/SEQ_CST`。
- 操作：LOAD/STORE/ADD/SUB/XCHG/CAS/THREAD_FENCE/SIGNAL_FENCE。
- `test_pal_atomic.c:31-48` 覆盖 64 位与 32 位边界回绕。
- 次要瑕疵：stdatomic 兜底路径 `atomic_load_explicit((ptr), ord)`（行 73）缺 `(_Atomic __typeof__(*(ptr))*)` 强转，严格 C11 编译器可能有类型双关警告（实际 target 均走 GCC/Clang 或 MSVC，影响小）；该兜底路径未定义 `PAL_ATOMIC_CAS`（69-81 仅 GCC/MSVC 定义）。

### 3.2 T2.2 pal_hwtimer ESP32 —— 🐛 BUG（多项关键配置被忽略）

**✅ DONE**:
- 头文件 API 完整：`init/start/stop/change_period/deinit/fire_soft`；config 含 `timer_id/period_us/oneshot/auto_start/core_affinity/isr_priority/uses_fpu/callback/callback_arg`。
- wasm `pal_wasm_hwtimer.c` 维护 `next_fire_us`，周期定时器 `+= period_us` 无漂移累加；host `pal_hal_hwtimer_host.c` 的 `fire_soft` 可调用回调、支持 oneshot、提供测试桩。
- 单测 `test_pal_hwtimer.c` 到位。
- ESP32 端 claim `PAL_RESOURCE_HWTIMER`（行 59），静态槽无 malloc。

**🐛 BUG（红线 4 破线：核亲和未实现）**:
- `core_affinity` 存入 `slot->cfg`（行 100）但**全文无 `xTaskCreatePinnedToCore` / `esp_intr_set_affinity` / `xPortGetCoreID`**。ADR-0047/ADR-0007 要求快环默认钉 Core1，完全未兑现。

**🐛 BUG（E-006：FPU 未保存恢复）**:
- `uses_fpu` 存入但从不行动，无 `frsave/frrestor` 蹦床。若回调用浮点寄存器，Xtensa FPU 上下文被破坏。计划明确要求 `uses_fpu=true` 时约 100 cycle 的保存/恢复。

**🐛 BUG（IRAM 安全）**:
- 行 28：`static esp32_hwtimer_slot_t s_timers[PAL_HWTIMERS_MAX];` 在默认 DRAM，**无 `PAL_IRAM_DATA`**。ISR（行 31，裸 `IRAM_ATTR` 而非 `PAL_ISR`）读 `slot->cfg.callback/callback_arg/oneshot`、写 `slot->is_running`。flash 擦除 / cache 禁用期间访问会崩，违反 IRAM-safe 快环红线。

**🐛 BUG（fire_soft 测试路径缺失）**:
- 行 201-204：`pal_hwtimer_fire_soft` 在 ESP32 **无条件返回 `WINK_ERR_NOT_SUPPORTED`**，连 `WINK_BUILD_TESTS` 下也无软步进路径。ESP32 端单测无法驱动 hwtimer。

**🐛 BUG（返回值未检查）**:
- 行 95、97、104：`gptimer_register_event_callbacks`、`gptimer_enable`、`gptimer_start` 返回值全部丢弃，失败被静默忽略。

**❌ MISSING**:
- 无 TWDT（任务看门狗）对快/慢环任务的排除或喂狗策略（计划评审项 M3）。

### 3.3 T2.3 pal_mcpwm ESP32 —— 🐛 BUG（安全关键功能形同虚设）

**外观到位**: 头文件五类 handle（timer/oper/cmp/fault/capture）、config 结构、API 声明齐全；ESP32 实现 380 行，静态数组各 6（2 单元×3），用 `pal_resource_mcpwm_id` claim 资源。

**🐛 BUG（死区半残，桥臂直通风险）**:
- `pal_hal_mcpwm_esp32.c:180-186`：只给 generator A 配了半边死区（`posedge_path = BYPASS, negedge_path = DELAY`），**delay 值未设（=0）**，generator B 完全没有死区。
- 配置的 `deadtime_red_ticks` 与 `deadtime_fed_ticks` 字段被整体忽略。
- 计划要求互补对两个 generator 都配 RED + FED。当前死区为 0，若用于真实互补桥臂，**存在上下管直通炸管风险**。

**🐛 BUG（硬件异步刹车完全不工作）**:
- 行 265 调 `mcpwm_new_gpio_fault` 但**返回值未检查**。
- **从未调用 `mcpwm_operator_connect_fault()`** 把故障连到 operator。
- `async_brake`、`safe_level_a`、`safe_level_b` 存储后从不应用（无 `mcpwm_generator_set_force_level`）。
- `on_brake_isr` 回调从未注册、从不调用。
- `pal_mcpwm_fault_clear`（行 345-348）是空 no-op，直接返回 WINK_OK。
- 后果：电机控制中最安全关键的硬件异步刹车**不存在**。故障引脚触发时不会进入安全电平。

**🐛 BUG（捕获通道从未创建）**:
- `pal_mcpwm_new_capture`（行 277-309）claim GPIO + 置 `in_use`，但**从不调用 `mcpwm_new_capture_channel()`**，不注册捕获回调。`cap_chan` 字段保持 NULL，`on_capture_isr` 真机上永不触发。

**🐛 BUG（相位锁定 / 软件同步是空操作）**:
- `pal_mcpwm_timer_enable_phase_lock`（335-339）忽略两个参数直接返回 WINK_OK。
- `pal_mcpwm_trigger_software_sync`（341-343）什么都不做返回 WINK_OK。
- 无 `mcpwm_timer_set_phase_on_sync`、无 `mcpwm_sync_enable`；GPIO 同步源除了 resource claim 外无任何配置。

**🐛 BUG（资源泄漏）**:
- `pal_mcpwm_del_timer`（350-361）只 disable/delete timer 本身，不删除关联的 operator/comparator/generator/fault/capture，不释放其 GPIO 引脚。

**🐛 BUG（IDF 返回值普遍丢弃）**:
- `mcpwm_new_generator`（169、176）、`mcpwm_generator_set_dead_time`（185）、`mcpwm_new_gpio_fault`（265）返回值全部静默丢弃。

**❌ MISSING**:
- config 的 `counter_top` 字段被忽略；周期仅由 `pwm_freq_hz` 在行 95 计算。
- 静态数组 `s_timers/s_opers/s_cmps/s_faults/s_caps`（58-62）无 `PAL_IRAM_DATA`，部分从 ISR（duty 更新、fault 回调）访问。

**非 ESP32 target**（行 369-377）：`new_fault/sync_gpio_config/timer_enable_phase_lock/trigger_software_sync/fault_clear` 诚实返回 `WINK_ERR_NOT_SUPPORTED`，符合 ADR-0012。

### 3.4 T2.4 ADC Continuous / PWM-ADC TRGO —— ⚠️ PARTIAL / 🐛 BUG

**✅ DONE（E-004 合规）**:
- `pal/include/hal/pal_adc.h:119-155`：触发源与边沿枚举、continuous config、start/stop 声明齐全。
- `targets/esp32/pal_hal_adc_esp32.c:477-482`：ESP32 classic 对 `PAL_ADC_TRIG_SOURCE_MCPWM` 诚实返回 `WINK_ERR_UNSUPPORTED`（E-004：classic 无 MCPWM→ADC TRGO）。

**🐛 BUG（双缓冲契约未兑现）**:
- `dma_buf_a`、`dma_buf_b` 仅在非空检查中出现（行 473），**从未传给 IDF `adc_continuous` 驱动**。驱动使用自己的内部缓冲，双缓冲契约不成立。

**🐛 BUG（回调永不注册）**:
- config 的 `on_half_full`、`on_full` 函数指针**从不 `adc_continuous_register_event_callbacks`**，真机上永不被调用。

**🐛 BUG（通道/模式/采样率/边沿全被忽略）**:
- `cfg->channels` 与 `cfg->channel_count` 从不用来配置 ADC pattern 表（`adc_continuous_config_t::pattern_table`）。
- 行 494：`.sample_freq_hz = 20000` 硬编码，忽略 `cfg->sampling_period_pwm` 及关联 PWM timer 频率。
- `cfg->edge`（PEAK/VALLEY/BOTH）从不使用。

**🐛 BUG**:
- 行 498-499：`adc_continuous_config` 与 `adc_continuous_start` 返回值未检查。

### 3.5 T2.5 无锁 FOC 管道 —— ✅ DONE（全 roadmap 完成度最高）

- `pal/include/osal/pal_lockfree_pipeline.h`（106 行）：
  - `q15_t = int16_t`、`q31_t = int32_t`，所有信号值定点。
  - 双向双 buffer + 原子索引字节；`foc_publish_cmd/consume_cmd/publish_status/consume_status` 全 `static inline`，用 `PAL_ATOMIC_LOAD/STORE` + ACQ/REL 序。
  - SPSC 假设文档化；无浮点、无 memw、无 struct 原子。
  - `seq_id` 字段支持序列追踪。
- `test_pal_lockfree_pipeline.c`：10000 次迭代压力测试验证无撕裂读。
- 次要（非 bug）：slot 数组（行 47-50）带 `volatile`，在索引字节已有 ACQ/REL 前提下多余且略悲观；为安全保留可接受。

### 3.6 T2.6 ISR 两级层级 —— ⚠️ PARTIAL

- ✅ `pal_irq.h:48-53` 三级优先级模型：`PAL_IRQ_PRIO_LOW=1/NORMAL=2/HIGH=3`。
- ❌ 无 tier-3 nFAULT 独立 256 字节 IRAM 栈。
- ❌ hwtimer ISR 与（不工作的）MCPWM fault ISR 未按"周期控制 vs nFAULT 保护"分级，未配差异化栈与允许操作规则。
- ❌ 无编译期/运行机制阻止 tier-3 fault ISR 调用除 `portYIELD_FROM_ISR` 外的 FreeRTOS API。

---

## 4. Stage 3 —— Wasm 仿真时序桥（UniSim 3.0）

### 4.1 T3.1 Wasm SPI 通道 —— 🐛 BUG（无法编译）

见 1.4 节。`pal_wasm_ch2_spi.c` 与头文件不匹配，无法编译。另有 per-device 完成回调槽并发问题。

**红线（Axis E 重入）**：✅ 守住——grep 确认 `targets/wasm/` 内无同步回调，所有用户回调只在 `pal_wasm_drain_completions` 内触发。但如 4.2 所述，drain 从未被调用，"守住"的代价是回调根本不发生。

### 4.2 T3.2 CH4 pull-model 完成队列 —— 🐛 CRITICAL（drain 从未接线）

**实现本身正确**:
- `targets/wasm/pal_wasm_completion.c`（76 行）：32 项静态数组，满则返回 `WINK_ERR_RESOURCE_EXHAUSTED`，支持 result 传播，reset/query 助手齐全；线性扫描按 deadline 排序（32 项可接受）。
- 单测 `test_pal_wasm_completion.c` 覆盖排序与 32 项溢出拒绝。

**🐛 CRITICAL（drain 零调用点）**:
- `pal_wasm_drain_completions()`（行 49）定义正确，**但全代码库中只被单测调用**。
- `osal/wasm/pal_osal_wasm.c:48-52` 的 `pal_wasm_advance_virtual_clock`（唯一虚拟时钟 gate）只调了 `pal_wasm_drain_due_waveform_edges(s_virtual_us)`：
  ```c
  EMSCRIPTEN_KEEPALIVE
  void pal_wasm_advance_virtual_clock(uint64_t us) {
      WASM_FAULT_GUARD_VOID();
      wink_vclock_advance_internal(us);
      pal_wasm_drain_due_waveform_edges(s_virtual_us);
  }
  ```
  **既不调 `pal_wasm_drain_completions()`，也不在 HEADLESS idle-jump（`pal_osal_wasm.c:551`）处调用。**
- 后果：真实 wasm 运行中所有 `pal_spi_transfer_dma` 回调**永不触发**，OLED 刷屏 / SPI 协议测试全部挂起。计划"OLED 1024B @40MHz 不阻塞主循环、callback 在下一 Phase 0 触发"验收不成立。
- 这是两行代码的接线遗漏，但使整个 Stage 3 异步基础设施在真实运行中不可用。

**命名差异（非 bug，以代码枚举为准）**：计划写 `WINK_ERR_NO_RESOURCES`，实际 `wink_status.h:90` 是 `WINK_ERR_RESOURCE_EXHAUSTED = -10`，语义等价。

**次要**：单测用 host 时钟（`pal_os_busy_wait_us`）而非虚拟时钟，未验证应从 `pal_wasm_advance_virtual_clock` 触发的 drain 钩子。

### 4.3 T3.3 hwtimer 软步进 —— 🐛 CRITICAL（drain 从未接线）

- `targets/wasm/pal_wasm_hwtimer.c`（97 行）：槽表、`init/start/stop/change_period/deinit/fire_soft`、`next_fire_us` 从 `pal_os_get_us()` 计算（64 位），周期重载、oneshot 禁用逻辑正确；`pal_wasm_hwtimer_drain()`（行 90）扫描到期 timer 并回调。
- 🐛 **`pal_wasm_hwtimer_drain()` 零外部调用点**：未接入 `wink_vclock_advance_internal`，未接入 HEADLESS idle-jump，也未被 completions drain 调用。验收"20kHz × 1s → 20000 次回调无漂移"无法达成。
- 🐛 `change_period`（行 64）立即 `next_fire_us = now + new_period`，与计划"下一周期生效"（应保留当前周期边界、下次触发时应用新周期）不符。
- ❌ 缺与 ESP32 一致的 `_Static_assert` ABI 尺寸校验（计划 T3.3 明确要求）。

### 4.4 T3.4 MCPWM / PCNT wasm —— ⚠️ PARTIAL

- ✅ `pal_wasm_mcpwm.c`：timer/oper/cmp/fault/cap 语义状态数组，`set_duty_ticks`、`fault_clear`、`js_pal_mcpwm_get_duty_ticks` 导出（行 169）。
- ✅ `pal_wasm_pcnt.c`：64 位累加（`int64_t count`，行 16）、`pal_wasm_push_pcnt_edge`（行 74）、`get_count/clear_count`。
- ⚠️ wasm PCNT TU 仍是旧 API（见 1.6），需配合头文件改名。
- 🐛 `pal_mcpwm_new_capture` 存储 cfg 但 `on_capture_isr` 回调无任何边缘注入路径调用（JS→C 的捕获摄入未接线）。
- 🐛 fault：`s_faults[i].tripped` 存在且 `fault_clear` 复位，但无公共 `new_fault` 置 tripped、无 brake 事件调度、无 `pal_wasm_schedule_complete_us(...0...)` 调用。

**关于 `js_pal_*` 边缘符号的澄清**：`wasm_bridge.h:363-364` 声明的 `js_pal_pcnt_edge` / `js_pal_mcpwm_capture_edge` 是 **C→JS 的 import**（`extern`，由 `wink_sim_js.js:186-194` 提供，`wink_sim_stub.js:82-83` 列出），不是 JS→C 的缺失导出。链接正常，非 bug。早期子代理曾误报此项，经核查阅 JS 实现后排除。

### 4.5 ABI hash 与 bridge 组织 —— ✅ DONE（有文档漂移）

- ✅ `pal_wasm_degradation.c:80`：`PAL_WASM_ABI_HASH 0x50333037u`，按 Stage 3 从 `0x50333036` bump。
- ✅ 新符号 `js_pal_pcnt_edge`、`js_pal_mcpwm_capture_edge`、`js_pal_mcpwm_get_duty_ticks` 在 `wasm_bridge.h:362-365` 归入新 CH5（Soft Edge / Motor Semantics）注释块；Axis A–F 头组织保留。
- ✅ `wink_sim_stub.js:82-84` 与 `wink_sim_js.js:186-199` 加了 stub/包装。
- ⚠️ `wasm_bridge.h:94` 关于 SPI 的注释仍写 "Phase 4 T5 — minimal stub"，而 C PAL 现已全面接线，注释陈旧。
- ⚠️ `js_pal_spi_transfer` 实际签名为 7 参（`port, device_id, tx, len, rx, mode, sck_hz`，`wasm_bridge.h:97`），计划参考片段写 5 参且声称"签名不变不 bump"。若签名确已拓宽，应走 v2 符号 + bump；若声明一直是 7 参，则计划片段陈旧。需与 ABI hash 纪律核对。

### 4.6 T3.5 UniSim 3.0 文档漂移 —— ⚠️ PARTIAL

- `docs/design/04-wasm-simulation-3.0/` 为 Active 入口（四层：overview/mechanisms/axes/assurance）。
- ⚠️ `01-overview/01-architecture.md:117` 仍引用不存在的文件：`pal_hal_wasm.c`、`pal_irq_wasm.c`、`pal_wasm_physical.c`。真实文件是 `pal_wasm_ch1_gpio.c`、`pal_wasm_ch1b_pwm.c`、`pal_wasm_ch2_bus.c`、`pal_wasm_ch2_spi.c`、`pal_wasm_ch2_uart.c`、`pal_wasm_irq.c`。
- ⚠️ `02-mechanisms/08-channel-routing.md:102` 把 SPI 标为 "Landed"（C PAL 对接 `js_pal_spi_transfer` + completion 拉模型），但因 drain 未接线（4.2），标 Landed 过早。
- ⚠️ `02-mechanisms/05-memory-and-faults.md:35` 正确记录 ADR-0045 三标志尚未在构建落地（见 4.7）。
- ❌ `02-mechanisms/09-timer-and-pwm-semantics.md` 未见 T3.3 要求追加的 hwtimer 软步进小节。

### 4.7 ADR-0045 固定堆 —— 🐛 未落地

- `wink-micro-os/CMakeLists.txt:300-302`：
  ```cmake
  "-sALLOW_MEMORY_GROWTH=1"
  "-sINITIAL_MEMORY=8MB"
  "-sMAXIMUM_MEMORY=32MB"
  ```
  与 ADR-0045 要求的 `ALLOW_MEMORY_GROWTH=0` 固定上限相反。
- 文档自己标了"尚未落地"，但 Stage 3 计划要求至少开 issue / 加 CMake 任务，均未做。

### 4.8 T3.6 ADR-0043 lint allowlist —— ❌ MISSING

同 1.9 节。PAL public 头 allowlist 未登记，`dal.yaml` 为空，`WASM-DAL-ISOLATION` 不查 `ESP_PLATFORM`。

---

## 5. Stage 4 —— DAL 外设铺开与 CI 门禁

### 5.1 DAL 覆盖盘点（计划 30 应用类型）

SSOT（`docs/implementation-plans/wokwi-dal-type-coverage-type/00.1-category-type-variant-wokwi-ssot.md` §2）枚举 30 应用类型。计划 §2.1 定为"30 应用 DAL 类型 + 3 Provider + audio stub"。

**已落地（14 应用类型）**:

| # | 类型 | 路径 |
|---|---|---|
| 1 | button | `dal/{src,include}/input/dal_button.*` |
| 2 | analog_knob | `dal/{src,include}/input/dal_analog_knob.*` |
| 3 | keypad | `dal/{src,include}/input/dal_keypad.*` |
| 5 | led | `dal/{src,include}/output/dal_led.*` |
| 6 | buzzer | `dal/{src,include}/output/dal_buzzer.*` |
| 7 | relay | `dal/{src,include}/output/dal_relay.*` |
| 9 | dc_motor | `dal/{src,include}/actuator/dal_dc_motor.*` |
| 10 | rc_servo | `dal/{src,include}/actuator/dal_rc_servo.*` |
| 12 | encoder | `dal/{src,include}/sensor/dal_encoder.*` |
| 13 | ultrasonic | `dal/{src,include}/sensor/dal_ultrasonic.*` |
| 18b | load_cell | `dal/{src,include}/sensor/dal_load_cell.*` |
| 19 | mono_oled | `dal/{src,include}/display/dal_mono_oled.*` |
| 24 | gps | `dal/{src,include}/comm/dal_gps.*` |
| 25 | eeprom | `dal/{src,include}/storage/dal_eeprom.*` |

**audio 占位**（不计入 30）：`dal/{src,include}/output/dal_audio.*`，诚实 `WINK_ERR_UNSUPPORTED` stub。

**`dal_ws2812` 存在但无法链接**（见 5.3）；SSOT 将 WS2812 像素归为 `led_matrix`（行 22）的变体而非独立类型，存在分类待对齐。

**❌ 缺失（约 16 应用类型）**:

| # | 类型 | 期望变体 |
|---|---|---|
| 4 | ir_receiver | nec_standard |
| 5b | rgb_led | common_anode / common_cathode |
| 8 | led_bar | gpio_direct / shift_reg_74hc595 |
| 11 | stepper | step_dir / four_wire |
| 14 | analog_sensor | ntc / photoresistor / gas_mq2_ao / flame_ao / sound_ao / heart_rate_ao |
| 15 | digital_sensor | threshold_do |
| 16 | temp_humidity | dht22_single_wire / sht3x_i2c |
| 17 | motion | pir_standard |
| 18 | imu | mpu6050_i2c |
| 20 | lcd_char | i2c_pcf8574 / parallel_4bit |
| 21 | tft | ili9341_spi |
| 22 | led_matrix | ws2812_strip / ws2812_matrix / ws2812_ring / max7219_spi |
| 23 | seg_display | direct_gpio_1d / direct_gpio_n_digit / tm1637_two_wire |
| 26 | sdcard | spi_sdcard |
| 27 | rtc | ds1307_i2c |

3 个 Provider（28-30 io_expander / multiplexer / i2c_mux）按计划 §2.2 延后到独立基础设施轨道，预期 MISSING。

`codegen/drivers/` 只登记 14 个 YAML（analog_knob, button, buzzer, dc_motor, eeprom, encoder, gps, keypad, led, load_cell, mono_oled, rc_servo, relay, ultrasonic），**ws2812/audio 未注册**，可能根本没进代码生成驱动的构建。

### 5.2 dal_gps —— ⚠️ 骨架 / 🐛 BUG

- 59 行真实代码，但**非计划要求的 NMEA 驱动**：
  - 行 14：用**同步** `pal_uart_init(port, -1, -1, baud)`（引脚 -1,-1），行 36 同步 `pal_uart_read` 轮询。计划 Batch C 明确要求走 T0.6 事件/idle 回调，而 T0.6 本身也未实现。
  - 无 NMEA RMC/GGA 解析，`last_position` 经纬高/速度/星数永远为 0。
  - 行 34-40：`poll` 读入 32 字节栈缓冲后仅 bump 时间戳，字节被丢弃。
  - `rx_buffer_size` 配置字段声明（头文件行 38）但忽略，无实际 ring buffer。
- 🐛 行 38：`(uint32_t)pal_os_get_ms()` 64→32 位截断（见 2.3）。
- 测试 `test_dal_gps.c` 仅断言 init/deinit/null-args，无 NMEA 解析用例（因无解析可测）。
- 注：未直接 `pal_resource_claim(PAL_RESOURCE_UART_PORT)`，依赖 `pal_uart_init` 内部 claim（`test_dal_gps.c:39` 断言该 claim 存在）。

### 5.3 dal_ws2812 —— 🐛 CRITICAL（链接失败）

- `dal/src/output/dal_ws2812.c:57`：
  ```c
  return pal_rmt_ws2812_write(dev->config.pin, grb_buffer, (size_t)(active_count * 3));
  ```
  grep 全树，**`pal_rmt_ws2812_write` 无任何声明、无任何定义**：
  - `pal/include/hal/pal_rmt.h` 无此符号；
  - `targets/esp32/pal_hal_rmt_esp32.c`、`targets/host/pal_hal_rmt_host.c` 均无定义；
  - wasm 侧只有名字不同的 `pal_ws2812_write`（`targets/wasm/pal_wasm_ch4_buffer.c:18`），且计划明确要求"WS2812 走 pal_rmt TX，不要直调 pal_ws2812_write"。
- 后果：该驱动在**所有三个 target 上都无法通过链接**，不可能被构建或运行过。
- 结构上正确的部分：GRB 字节重排、`pal_resource_claim(GPIO_PIN, pin, "dal_ws2812")`、无位域、无 `#ifdef`、调 PAL 而非直调 JS。
- 🐛 本地 GRB 缓冲上限 64 灯（栈上 192 字节，行 11/48），超出静默截断（行 49）。
- 🐛 头文件 `dal_ws2812.h:14` 违规 `#include "hal/pal_hal.h"`（DAL 公共头禁 HAL，违反 layering 规则 `DAL-HDR-NO-HAL`）。
- 🐛 `dal_ws2812_config_t` 无 `variant` 字段，违反 SSOT Boundary D（单变体类型也应预留 variant 枚举 + `_Static_assert`）。
- ❌ 无 `test/unit/dal/test_dal_ws2812.c`；`test_dal_ws2812_sim.c` 测的是 PAL `pal_ws2812_write` 而非 DAL，名不副实。

### 5.4 dal_eeprom —— ⚠️ PARTIAL

- ✅ 真实 I2C 实现：`pal_i2c_transfer`（行 46/68）；`pal_resource_claim(PAL_RESOURCE_I2C_ADDR, pal_resource_i2c_id(...), owner)`（行 19）；request/poll/get_read_result 三态（`DAL_EEPROM_IDLE/BUSY/READY/ERROR`）；边界检查 `addr+len <= capacity_bytes`、128 字节内部缓冲；`_blocking` 包装在 `WINK_STRICT_NONBLOCKING` 下；deinit 释放资源。
- 🐛 **并发 bug**：`s_eeprom_rx_buf[128]`（行 10）是单一**静态**缓冲，非 per-instance。两个 EEPROM 实例并发 `request_read` 会互相污染结果。应改为 per-instance 或 claim I2C 总线互斥。
- 🐛 **写周期未建模**：`poll`（行 75-79）永远返回 WINK_OK，不建模 EEPROM 写周期（`page_size`/`write_time_ms` 配置存储了但未用，真实 EEPROM 写需 ~5ms 才 ACK）。状态机在 `pal_i2c_transfer` 返回后直接进 READY。
- ❌ 无 SPI EEPROM 变体（头文件注释"预留 spi_eeprom"）。
- ✅ 无 `#ifdef SIMULATION/ESP_PLATFORM`，干净。

### 5.5 dal_audio —— ✅ DONE（诚实占位）

- 5 个公共函数全部返回 `WINK_ERR_UNSUPPORTED`，无假静音、无 `#ifdef`、仅头文件依赖，符合计划 §audio 占位交付与 ADR-0012。
- 次要：无 `_Static_assert` ABI 尺寸、无测试。

### 5.6 dal_encoder —— ⚠️ 未迁移

- 仍是 GPIO 软件 ISR 计数（`dal_encoder.c:47` `PAL_DEFINE_ISR(dal_encoder_gpio_isr...)`），只支持 `DAL_ENCODER_VARIANT_X1_RISING`，其余返回 `WINK_ERR_UNSUPPORTED`（行 73）。
- Stage 0 已做 pal_pcnt 硬件加速（T0.5），但 encoder 未切过去，T0.5 收益未被消费，与计划意图矛盾。

### 5.7 DAL 分层红线 —— 🐛 BUG

- ✅ ADR-0003 要求 DAL 源码零 `#ifdef SIMULATION/ESP_PLATFORM`。grep 确认无 SIMULATION。
- 🐛 **`dal/src/sensor/dal_ultrasonic.c:247` 有 `#if defined(ESP_PLATFORM)`**，包裹 Xtensa `__asm__ __volatile__("memw" ::: "memory")` 屏障：
  ```c
  dev->last_status = WINK_OK;
  #if defined(ESP_PLATFORM)
      __asm__ __volatile__("memw" ::: "memory");
  #endif
      dev->state = DAL_ULTRASONIC_READY;
  ```
  违反 ADR-0003 与 Stage 4 明确要求。该内存屏障应下沉到 PAL（如 `pal_atomic.h` 的 `pal_memory_barrier()`）按 target 实现。lint 规则未拦住，因为 `WASM-DAL-ISOLATION` 正则只列 `SIMULATION|WASM|__EMSCRIPTEN__`（见 1.9）。
- ✅ DAL 内无 `esp_*` / `arduino_*` 调用（唯一 `esp_` 命中是 `dal_rc_servo.c:199` 的注释）。
- ✅ 公共 DAL 结构体无位域、无 `#pragma pack`（grep `: [0-9]+;` in `dal/include` 零命中）。

### 5.8 三态 request/poll/get_cached —— ⚠️ PARTIAL

- ✅ load_cell、ultrasonic、eeprom 到位。
- ⚠️ gps 只有 poll；ws2812 只有同步 write（长灯条 >1ms 应三态化，但它连链接都过不了）。
- ⚠️ 64 位减法超时：load_cell 正确（`pal_os_get_us() - start_us < timeout_us`，无符号回绕安全）；relay/keypad/gps 用 32 位毫秒或无超时（见 2.3）；eeprom 不用 `pal_os_get_us()` 建模写周期。

### 5.9 pal_resource_claim 上限强制 —— ✅ DONE

- `targets/common/src/pal_resource.c:69` 检查 `id >= pal_resource_max(type)` 返回 `WINK_ERR_INVALID_ARG`。
- DAL 驱动透传错误而非截断 id。
- gps 未直接 claim（依赖 `pal_uart_init` 内部 claim，已由测试断言）。

### 5.10 测试 —— ⚠️ PARTIAL

- ✅ PAL 单测丰富：`test_pal_atomic/hwtimer/mcpwm/adc_continuous/lockfree_pipeline/spi/rmt/pcnt/resource/uart/time_safety/nonblocking_strict/log_hardening/wasm_completion` 均在 `test/unit/pal/`。
- ✅ DAL 单测 23 个在 `test/unit/dal/`。
- ❌ **无任何 wasm 侧 SPI/PCNT/MCPWM/hwtimer/completion 的 `test/wasm/` 测试**（现有 `test/wasm/` 集早于 Stage 3）。
- ❌ 计划 §4 第 8 项要求的 **DAL 完成队列溢出重试测试**（填满 32 槽 → DAL request 返回 BUSY → 下个 10ms tick 重试 → 成功）不存在。当前没有任何 DAL 消费 `pal_wasm_schedule_complete_us`（mono_oled 仍 bitbang SPI，未切 `pal_spi_transfer_dma`），端到端溢出/重试路径未测未接线。
- ❌ 无 `test_dal_audio.c`；DAL 层无 `test_dal_ws2812.c`；`test_dal_gps.c` 无 NMEA 解析用例；`test_dal_eeprom.c` 不测多实例并发、页写时序、越界地址溢出。
- ⚠️ `test_dal_ws2812_sim.c` 测的是 **PAL** `pal_ws2812_write` 而非 DAL，mislabeled。

### 5.11 mono_oled 切 pal_spi —— ❌ MISSING

- `dal/src/display/dal_mono_oled.c:64-90` 仍是 `spi_bitbang_write`（GPIO 软翻 MOSI/CLK/CS），所有 SSD1306 SPI 路径用它（行 194/291/294/303/307/332）。
- Stage 3 T3.1 落地 wasm SPI PAL 本就是为这步铺路，未消费。因此 wasm SPI 完成队列当前**零 DAL 消费者**，这也是 4.2 节 drain 未接线长期未被发现的原因之一。

### 5.12 CI 三层门禁 —— ❌ MISSING

`.github/workflows/` 下**只有 `clang-tidy.yml`**（push/PR 到 main/develop 时装 clang-tidy-17、跑全 C 文件、跑 `python ../wink-tools/wink.py lint --strict`、跑 wink-tools pytest）。

对照计划 §5 PR/Nightly/Release 三层：

**PR 门禁（<10min）缺失**:
- ❌ 三 target 构建矩阵（IDF 5.4 与 6.0、host、wasm）——无 `idf.py`/emscripten 安装，无 cmake/CTest。
- ❌ ABI hash 不变性校验（diff `wasm_bridge.h` vs `PAL_WASM_ABI_HASH`）。
- ❌ `_Static_assert` ABI 尺寸门禁（头里写了但 CI 从不编译）。
- ❌ ISR 禁 log 扫描脚本。
- ❌ 堆增长门禁（断言 `-sALLOW_MEMORY_GROWTH=0`）——当前 CMake 是 `=1`，门禁若加上会直接红。
- ⚠️ lint 跑 `--strict`，但如 1.9 所述，YAML 规则对新内容为空/陈旧。
- ❌ Host Unity 测试不在 CI 执行（无 ctest 调用）。

**Nightly 门禁（全缺）**:
- ❌ 无定时 workflow、无 ESP32 DevKitC runner、无 30min 并发应力、无 24h WDT、无虚拟时钟确定性 replay。

**Release 门禁（全缺）**:
- ❌ 无 24h 应力、无 71.58min（2³²µs）边界测试（`test_clock_overflow.c` 只推到 `0x8000000000000001` 验证 warning latch，不测 DAL 超时回绕）、无故障注入 L1/L2/L3 矩阵、无 ABI 兼容矩阵。
- ❌ 任何地方都无 IDF 5.4/6.0 矩阵。

---

## 6. 八条红线体检

| # | 红线 | 状态 | 证据 |
|---|---|---|---|
| 1 | 快/慢环 WCET <2ms | ⚠️ 无举证 | HX711 bit-bang 在全局 `taskENTER_CRITICAL` 内，无 ccount 测量；SPI 持锁内 `portMAX_DELAY` 阻塞直接破线（1.4）。 |
| 2 | 临界区 <100µs | 🐛 破线 | `pal_hal_spi_esp32.c:256` 持自旋锁无限期阻塞；RMT `tx_send` 持锁栈分配 1KB + `rmt_transmit`（1.5）。 |
| 3 | 快环 IRAM-safe | 🐛 破线 | `s_timers` 无 `PAL_IRAM_DATA`，ISR 访问 DRAM（3.2）；多处裸 `IRAM_ATTR`（1.2）。 |
| 4 | 快环钉 Core1 | 🐛 破线 | hwtimer `core_affinity` 字段被忽略，无亲和设置（3.2）。 |
| 5 | DAL 零 `#ifdef` | 🐛 破线 | `dal_ultrasonic.c:247` `#if defined(ESP_PLATFORM)`（5.7）。 |
| 6 | wasm js_pal_* 内禁同步回调 | ✅ 虚假满足 | grep 零命中，但 drain 从未调用，回调根本不触发（4.2）。 |
| 7 | 负数错误码 / 不伪造成功 | ✅ 守住 | 整体一致；audio stub 是正面范例；ws2812/gps 属"未完成"而非"伪造成功"。 |
| 8 | 无位域 / `#pragma pack` | ✅ 守住 | PAL/DAL 公共头 grep 干净；`pal_rmt_symbol_t` 用独立字段 + `_pad`。 |

---

## 7. 修复优先级

### P0 —— 阻断构建/运行/安全，必须先修

1. **接线 wasm drain（解锁 Stage 3，约 2 行）**: 在 `osal/wasm/pal_osal_wasm.c:48-52` 的 `pal_wasm_advance_virtual_clock` 末尾补调 `pal_wasm_drain_completions()` 与 `pal_wasm_hwtimer_drain()`；HEADLESS idle-jump（`:551`）同样补。修后所有 wasm async 完成与 hwtimer 软步进才真正运行。
2. **修 wasm ch2_spi 头文件不匹配**（1.4）：`spi_bus` 字段名、`pal_spi_add_device` 加 `uint8_t bus` 首参；顺手修 per-device 回调槽并发（改从完成池取独立 adapter）。
3. **实现或删除 `pal_rmt_ws2812_write` 调用**（5.3）：正确做法是用 `pal_rmt_acquire_channel` + `pal_rmt_tx_send` 把 GRB 编成 RMT symbol；补 `codegen/drivers/ws2812.yaml`；`dal_ws2812.h` 去掉 `pal_hal.h` 依赖。
4. **修 wasm PCNT TU 旧 API**（1.6）：`pal_pcnt_unit_handle_t`、config 字段、`clear` 等，否则 wasm target 无法链接；补缺失的 wasm RMT TU（可先 `WINK_ERR_UNSUPPORTED` 桩保证链接，ADR-0012）。
5. **MCPWM 死区 + 硬件刹车（安全最高优先）**（3.3）：在用于任何电机功率驱动前，必须正确配置双 generator RED+FED（取 `deadtime_red_ticks/fed_ticks`）、`mcpwm_operator_connect_fault` + `mcpwm_generator_set_force_level`（应用 `async_brake/safe_level_a/b`）、注册 `on_brake_isr`、实现真实 `fault_clear`。当前代码接真实桥臂有直通炸管风险。

### P1 —— 正确性 / 安全

6. **hwtimer ESP32**: 实现 `core_affinity`（`esp_intr_set_affinity` 或 pinned task）、`uses_fpu` 的 FPU 保存/恢复（E-006）、`s_timers` 加 `PAL_IRAM_DATA`、`fire_soft` 在 `WINK_BUILD_TESTS` 下走软步进、检查所有 IDF 返回值、ISR 改用 `PAL_ISR`。
7. **MCPWM 其余功能**: `mcpwm_new_capture_channel` + 捕获回调、`mcpwm_timer_set_phase_on_sync` + 软件同步真实实现、`del_timer` 回收 operator/comparator/generator/fault/capture 与 GPIO、所有 IDF 返回值检查、静态数组 `PAL_IRAM_DATA`。
8. **ADC continuous**: 把 `dma_buf_a/b` 传入 IDF、注册 `on_half_full/on_full` 回调、配 pattern 表、采样率取 `sampling_period_pwm`、应用 `edge` 配置。
9. **PCNT**: 64 位累加改 `__atomic_fetch_add`/`__atomic_load_n`；glitch filter 强制 ≥1000ns 下限并修正单测；4X `pcnt_new_channel` 检查返回值；ISR 读中断时刻硬件计数而非加阈值常量。
10. **DAL 分层/时间戳**: `dal_ultrasonic.c:247` 的 `memw` 下沉为 `pal_memory_barrier()`；修 gps/relay/keypad 三处 `uint32_t` 时间戳截断为 64 位；`dal_eeprom.c` 静态 RX 缓冲改 per-instance 并建模写周期；HX711 `tare()` 假时钟改读真实时钟。
11. **SPI/RMT 临界区**: SPI 持锁内禁止 `portMAX_DELAY` 阻塞；映射 `cs_setup_ns/cs_hold_ns`；RMT `tx_send` 不在持锁期间栈分配 1KB + 阻塞调用；RMT RX ISR 工作量限制（推迟到 task）。
12. **HX711 ccount 举证**: 加 `xthal_get_ccount()` 实测总时长 / SCK 高电平时长，注释记录；若超 100µs/50µs 升级到 RMT/SPI。

### P2 —— 功能 / 门禁 / 文档

13. **T0.6 UART idle/FIFO-high**: 实现 `PAL_UART_EVT_RX_IDLE` 与可编程 FIFO 阈值（GPS NMEA / Modbus 真正依赖）；修 ESP32 事件任务与 `pal_uart_read` 的数据竞争。
14. **ADR-0043 lint 规则落地**: 加 `pal_public` allowlist（登记新 PAL 头）、DAL 规则（init/request/deinit 存在性、禁 `esp_*/arduino_*`）、`WASM-DAL-ISOLATION` 补查 `ESP_PLATFORM/ESP_IDF`、api.yaml 回调命名 + ISR Doxygen + 位域正则；修 `dal_ws2812.h` 违规。
15. **CI 三层门禁**: PR 加三 target 构建矩阵（IDF 5.4/6.0、host、wasm）+ host ctest + ABI hash 门禁 + ISR 禁 log 扫描 + 堆增长门禁；IDF <5.4 硬 `#error`；CMake 加 `esp_driver_gdma`；ADR-0045 固定堆（`ALLOW_MEMORY_GROWTH=0`）。Nightly/Release 按计划补真机应力、24h WDT、2³²µs 边界、故障注入矩阵。
16. **缺失 DAL**: 按 Batch 推进约 16 类；mono_oled 切 `pal_spi_transfer_dma`（解锁完成队列端到端测试）；encoder 切 pal_pcnt。
17. **文档/策略**: 写微临界区策略文档（三禁表）；补 UniSim 3.0 文档漂移（架构文件名、SPI Landed 状态、hwtimer 软步进小节）；补 wasm TU 的 `_Static_assert` ABI 尺寸校验；`js_pal_spi_transfer` ABI 签名核对。
18. **ISR 两级栈**: tier-3 nFAULT 独立 IRAM 栈与允许操作强制。

---

## 8. 正面评价

- **`pal_spinlock.h` / `wink_compiler.h` / `pal_resource.h` / `pal_atomic.h` / `pal_lockfree_pipeline.h`** 质量高，与计划高度吻合，是后续工作的坚实底座。
- **错误码纪律一致**：负数 `wink_status_t`、`WINK_WARN_UNUSED_RESULT` 普遍应用；`WINK_ERR_UNSUPPORTED` 诚实返回（audio stub、非 ESP32 MCPWM 桩、ESP32 classic MCPWM TRGO）符合 ADR-0012。
- **静态池 / 无 malloc 纪律**在 SPI、PCNT 热路径遵守到位。
- **Host 测试桩基础设施扎实**：SPI 注入环而非 naive loopback，支持协议级测试。
- **ADR-0002 位域禁令**在所有 PAL/DAL 公共头严格遵守，`pal_rmt_symbol_t` 的独立字段 + `_pad` 是范例。
- **DAL `#ifdef SIMULATION` 隔离**整体干净（仅一处 ESP_PLATFORM 违规），bypass 基本下沉到 PAL/target。
- **无锁管道**是全 roadmap 的标杆实现：定点、SPSC、ACQ/REL、有撕裂读压力测试。

---

## 9. 结论

这批提交的**骨架工程（Stage 0 基础设施 + lockfree pipeline）达到可合入质量**，但：

- **Stage 2 的 hwtimer/mcpwm/adc continuous 不能视为完成**——它们是"API 长得对、安全关键路径未接通"的状态，其中 MCPWM 死区/刹车有真实硬件损坏风险。
- **Stage 3 因 drain 未接线而整体不可用**——这是最小、最易修、也最该先修的遗漏。
- **Stage 4 的 DAL 铺开严重不足**（14/30，ws2812 无法链接，gps 是骨架），CI 门禁几乎为零。

建议按 P0 五项先修复（MCPWM 安全项与 wasm drain 接线优先），在三 target 真正能编译链接 + wasm 回调真正触发之后，再继续铺 DAL 类型与补 CI 门禁。按项目规则，所有复杂变更应先产出 implementation plan 并经确认后再动代码。

---

## 附：评审涉及的关键文件索引

**PAL 头**
- `wink-micro-os/pal/include/pal_spinlock.h`
- `wink-micro-os/pal/include/wink_compiler.h`
- `wink-micro-os/pal/include/pal_resource.h`
- `wink-micro-os/pal/include/pal_irq.h`
- `wink-micro-os/pal/include/hal/pal_spi.h`
- `wink-micro-os/pal/include/hal/pal_rmt.h`
- `wink-micro-os/pal/include/hal/pal_pcnt.h`
- `wink-micro-os/pal/include/hal/pal_uart.h`
- `wink-micro-os/pal/include/hal/pal_hwtimer.h`
- `wink-micro-os/pal/include/hal/pal_mcpwm.h`
- `wink-micro-os/pal/include/hal/pal_adc.h`
- `wink-micro-os/pal/include/osal/pal_atomic.h`
- `wink-micro-os/pal/include/osal/pal_lockfree_pipeline.h`

**Target 实现**
- `wink-micro-os/targets/esp32/pal_hal_spi_esp32.c`
- `wink-micro-os/targets/esp32/pal_hal_rmt_esp32.c`
- `wink-micro-os/targets/esp32/pal_hal_pcnt_esp32.c`
- `wink-micro-os/targets/esp32/pal_hal_uart_esp32.c`
- `wink-micro-os/targets/esp32/pal_hal_hwtimer_esp32.c`
- `wink-micro-os/targets/esp32/pal_hal_mcpwm_esp32.c`
- `wink-micro-os/targets/esp32/pal_hal_adc_esp32.c`
- `wink-micro-os/targets/esp32/pal_resource_esp32.c`
- `wink-micro-os/targets/esp32/CMakeLists.txt`
- `wink-micro-os/targets/host/pal_hal_spi_host.c`
- `wink-micro-os/targets/wasm/pal_wasm_ch2_spi.c`
- `wink-micro-os/targets/wasm/pal_wasm_pcnt.c`
- `wink-micro-os/targets/wasm/pal_wasm_mcpwm.c`
- `wink-micro-os/targets/wasm/pal_wasm_hwtimer.c`
- `wink-micro-os/targets/wasm/pal_wasm_completion.c`
- `wink-micro-os/targets/wasm/pal_wasm_degradation.c`
- `wink-micro-os/targets/wasm/wasm_bridge.h`
- `wink-micro-os/osal/wasm/pal_osal_wasm.c`

**DAL**
- `wink-micro-os/dal/src/sensor/dal_load_cell.c`
- `wink-micro-os/dal/src/sensor/dal_ultrasonic.c`
- `wink-micro-os/dal/src/sensor/dal_encoder.c`
- `wink-micro-os/dal/src/comm/dal_gps.c`
- `wink-micro-os/dal/src/storage/dal_eeprom.c`
- `wink-micro-os/dal/src/output/dal_ws2812.c`
- `wink-micro-os/dal/src/output/dal_audio.c`
- `wink-micro-os/dal/src/output/dal_relay.c`
- `wink-micro-os/dal/src/input/dal_keypad.c`
- `wink-micro-os/dal/src/display/dal_mono_oled.c`
- `wink-micro-os/dal/include/output/dal_ws2812.h`

**构建 / CI / lint**
- `wink-micro-os/CMakeLists.txt`
- `.github/workflows/clang-tidy.yml`
- `wink-ai/packages/wink-tools/tools/lint/rules/layering.yaml`
- `wink-ai/packages/wink-tools/tools/lint/rules/api.yaml`
- `wink-ai/packages/wink-tools/tools/lint/rules/dal.yaml`
