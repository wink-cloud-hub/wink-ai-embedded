# 阶段 0 计划：PAL 硬件加速引擎（SPI-DMA / RMT 多通道+TX / PCNT / MCPWM / UART-DMA / IRAM-data）

| 元数据项 | 说明 |
| :--- | :--- |
| **阶段编号** | STAGE-0-PAL-HW-ENGINES |
| **所属模块** | `wink-micro-os/pal/include/hal/` + `wink-micro-os/targets/{esp32,host,wasm}/` + `wink-micro-os/osal/` |
| **解决时序类别** | Class 1（ns 脉冲 WS2812）/ Class 4（SPI-DMA 高速总线）/ Class 5（边沿正交计数） |
| **依赖 ADR** | [ADR-0002 双 Target 编译](../../decisions/unisim/0002-dual-target-compilation.md)、[ADR-0012 合约诚实](../../decisions/core/0012-contract-honesty-over-silent-degradation.md)、[ADR-0034 DAL 渐进式配置](../../decisions/core/0034-dal-progressive-config-disclosure.md)、[ADR-0043 YAML 分层 lint](../../decisions/tools/0043-yaml-driven-layer-lint.md) |
| **基线** | 本阶段"已存在"结论以 [00-master-plan.md §5](00-master-plan.md#5-stage--1-现状审计基线v2-新增) 为准 |
| **状态** | **Ready for Implementation** |

---

## 1. 阶段目标与解决痛点

- 消灭软件 Bit-Bang 热点：`dal_mono_oled.c:64-92` SPI bitbang 1024 字节刷屏约 17k 次 GPIO 调用（30~50 ms）；`dal_encoder.c:47-59` 软件边沿计数在高频丢步。
- 为 WS2812（Class 1，350 ns 脉宽）提供 RMT TX 引擎；为正交编码器（Class 5）提供 PCNT 硬件 64 位计数；为高速 SPI 屏/Flash 提供 DMA 异步传输。
- 把 ESP-IDF 新驱动（`esp_driver_spi/pcnt/gptimer/mcpwm/gdma`）纳入 REQUIRES 与版本门控；统一 IRAM/DMA 内存属性抽象。
- 扩展 `pal_resource` 仲裁范围，避免多驱动竞争同一硬件单元。

---

## 2. 任务清单

### T0.0 工具链阻塞项（Blocker，先于一切代码动工）

**前置条件（Master §3 引用）**：

1. **`wink lint` 路径核实**：ADR-0043 引用 `tools/lint/rules/`，UniSim 文档引用 `wink-tools/tools/lint/rules/`，仓内 `Glob **/lint/rules/*.yaml` 零命中。Stage 0 启动前必须：
   - 与 wink-tools owner 确认真实路径；若不存在，由 wink-tools owner 在 Stage 0 第 0 周创建空规则集并在 CI 跑通；
   - 路径确认后更新 ADR-0043 与 UniSim 文档，消除文档间漂移；
   - T0.8 lint allowlist 更新改为指向真实路径。
2. **四 target 编译矩阵**：IDF 5.4 / IDF 6.0 / host / wasm 四路 CI 在 Stage 0 首个 PR 前可用。
3. **`_Static_assert` 跨 target 验证**：加一个已知尺寸结构体（如 `_Static_assert(sizeof(pal_rmt_symbol_t)==8, "")`）在四路编译中均触发，确认工具链行为一致。

**验收**：上述三项全部在 CI 中可见、可跑、可挂；任一未关闭则 Stage 0 不进入 T0.1。

### T0.1 `pal_spinlock_t` + IRAM / DMA 内存属性宏（先行）

#### T0.1a 跨 target 自旋锁抽象

**现状**：仓内 5 个 `.c`（`pal_resource_esp32.c`、`pal_hal_gpio_esp32.c`、`pal_hal_i2c_esp32.c`、`pal_hal_adc_esp32.c`、`pal_osal_freertos_esp32.c`）直接 `portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED`；host/wasm 无对应抽象。Stage 1 T1.1 方案 B 依赖 `pal_os_spinlock_t`，Stage 2 无锁管道也需要。

**交付**：`pal/include/pal_spinlock.h`（放根目录，不放 `hal/` 也不放 `osal/`——spinlock 跨 task/ISR 边界，不驱动外设故不属 `hal/`；ISR 内可用故不属依赖调度器的 `osal/`；与 `pal_irq.h` 同属跨层横切基础设施）

```c
#pragma once
#include "wink_compiler.h"

#if defined(ESP_PLATFORM)
  #include "freertos/FreeRTOS.h"
  #include "soc/cpu.h"
  typedef portMUX_TYPE pal_spinlock_t;
  #define PAL_SPINLOCK_INITIALIZER   portMUX_INITIALIZER_UNLOCKED
  static inline void pal_spinlock_init(pal_spinlock_t *l) { vPortCPUInitializeMutex(l); }
  static inline void pal_spinlock_lock(pal_spinlock_t *l) {
    /* Debug guard：pal_spinlock_lock 严禁在 ISR 上下文调用；ISR 必须使用
     * pal_spinlock_lock_isr，否则 FreeRTOS 临界区嵌套计数不匹配导致 assert/崩溃。
     * configASSERT 在 Debug build 下立即定位误用点；Release build 零开销。 */
    configASSERT(!xPortInIsrContext());
    taskENTER_CRITICAL(l);
  }
  static inline void pal_spinlock_unlock(pal_spinlock_t *l) {
    configASSERT(!xPortInIsrContext());
    taskEXIT_CRITICAL(l);
  }
  /* ISR 側使用 FromISR 变体（必须且只能在 ISR 上下文调用） */
  static inline void pal_spinlock_lock_isr(pal_spinlock_t *l)   { taskENTER_CRITICAL_ISR(l); }
  static inline void pal_spinlock_unlock_isr(pal_spinlock_t *l) { taskEXIT_CRITICAL_ISR(l); }
#elif defined(__wasm__) || defined(__unix__) || defined(__APPLE__)
  typedef struct { char _dummy; } pal_spinlock_t;
  #define PAL_SPINLOCK_INITIALIZER {0}
  static inline void pal_spinlock_init(pal_spinlock_t *l) { (void)l; }
  static inline void pal_spinlock_lock(pal_spinlock_t *l)   { (void)l; __atomic_signal_fence(__ATOMIC_ACQUIRE); }
  static inline void pal_spinlock_unlock(pal_spinlock_t *l) { (void)l; __atomic_signal_fence(__ATOMIC_RELEASE); }
  static inline void pal_spinlock_lock_isr(pal_spinlock_t *l)   { pal_spinlock_lock(l); }
  static inline void pal_spinlock_unlock_isr(pal_spinlock_t *l) { pal_spinlock_unlock(l); }
#else
  #error "Define pal_spinlock for this target"
#endif
```

**要点**：
- 单线程 host/wasm 退化为编译屏障，但保留与真机一致的 API 形态；
- 既有 5 个 `.c` 中 `portMUX_TYPE` 全部替换为 `pal_spinlock_t`（本任务内完成，不留 TODO）；
- 与 `pal_os_critical_enter/exit`（FreeRTOS 全局 critical）区分：`pal_spinlock_t` 是**细粒度 per-instance**，适合 DAL 设备实例锁；`pal_os_critical_enter` 是全局短临界。

**实现形态决策（header-only，不可更改）**：

`pal_spinlock_lock/unlock` 在 MCPWM/PCNT ISR 热路径中调用，函数调用的压/弹栈开销在 10 kHz 中断频率下不可接受，**必须在调用方编译单元内联展开**，因此实现只能在头文件中以 `static inline` 提供。由此衍生两条不可变约束：

1. **不可拆成 per-target `.c` TU**：`pal_spinlock_esp32.c` / `pal_spinlock_host.c` 等实现文件无法 inline，方案放弃。
2. **不可拆成 per-target `.h`**（即不建 `pal_spinlock_esp32.h` / `pal_spinlock_host.h` 再靠 CMake 路径分发）：调用方只能写一个 include 路径；CMake include_path 分发路径配错就是神秘的"file not found"，且把平台分发逻辑从代码转移到构建系统，复杂性没有减少只是移位了。

`pal_spinlock.h` 内的 `#if defined(ESP_PLATFORM)` 分支**不违反 ADR-0003**：ADR-0003 禁止的是 `dal/src/` 里出现 `#ifdef SIMULATION/WASM`，目的是防止仿真逻辑渗入业务 DAL；PAL 层有平台分支是其本职，两者语义不同。

**与 `pal_atomic_esp32.h` 的关系**：`pal_atomic_esp32.h` 是 target-private arch 原语（`targets/esp32/` 下），被 `pal_hal_gpio_esp32.c`、`pal_irq_esp32.c` 等 TU 直接使用。`pal_spinlock.h` 的 ESP32 分支直接使用 FreeRTOS `portMUX_TYPE`（`#include "freertos/FreeRTOS.h"` 包裹在 `#if defined(ESP_PLATFORM)` 块内，host/wasm build 完全跳过），两者**平行不交叉**——公共头不引用 target-private 头，分层规则不受影响。

**验收**：
- `grep -rn "portMUX_TYPE\|portMUX_INITIALIZER" targets/ osal/ pal/` 仅在 `pal_spinlock.h` 命中；
- host TSan 下两线程并发抗锁无 data race（host 实现若需真实互斥可在 debug 编译换 `pthread_mutex_t`，release 仍为信号屏障以匹配 wasm）；
- **Debug build ISR 误用拦截**：在 ESP32 真机 Debug build 中，在任意 `PAL_ISR` 修饰的回调内调用 `pal_spinlock_lock`（非 `_isr` 变体），`configASSERT(!xPortInIsrContext())` 立即触发，日志打印调用任务/ISR 名；PR 中附真机 assert 日志截图作为证据；
- 三 target 编译通过。

#### T0.1b IRAM / DMA 内存属性宏

**现状**：`wink_compiler.h` 仅有 `WINK_WEAK`；DMA 描述符、ISR 常量、IRAM 数据散落 `DRAM_ATTR`/`IRAM_ATTR`，wasm/host target 无抽象。

**交付**：在 `wink-micro-os/pal/include/osal/wink_compiler.h` 增加

```c
#if defined(ESP_PLATFORM)
  #include "esp_attr.h"
  #define PAL_IRAM_TEXT     IRAM_ATTR        /* ISR 函数 */
  #define PAL_IRAM_DATA     IRAM_DATA_ATTR   /* ISR 读写数据（非 DMA） */
  #define PAL_IRAM_RODATA   IRAM_DATA_ATTR   /* ISR 只读常量表 */
  #define PAL_DMA_ATTR      WORD_ALIGNED_ATTR DRAM_ATTR /* DMA 描述符 */
  #define PAL_DMA_BUF_ATTR  WORD_ALIGNED_ATTR DRAM_ATTR /* DMA 数据缓冲 */
#else
  #define PAL_IRAM_TEXT
  #define PAL_IRAM_DATA
  #define PAL_IRAM_RODATA
  #define PAL_DMA_ATTR
  #define PAL_DMA_BUF_ATTR
#endif
```

- 现有 `PAL_ISR` 宏（`pal_irq.h:83-85`）内部改为使用 `PAL_IRAM_TEXT`，保持源码兼容。
- 验收：`grep -rn "IRAM_ATTR\|DRAM_ATTR" targets/esp32/` 除 `wink_compiler.h` 自身外全部通过新宏。

### T0.2 `pal_resource` 仲裁扩展（API 命名校正 + max 边界）

**现状**：
- 实际 API 是 `pal_resource_claim / release / is_claimed / reset`（**非** `take/release`）。v2 文档用 `take` 错了；本版统一改回 `claim/release`。
- 枚举仅覆盖 GPIO_PIN/PWM_CHANNEL/I2C_PORT/I2C_ADDR/UART_PORT/ADC_CHANNEL；无 max 边界校验。

**交付**：

1. 扩展枚举（保留现有 1~6，追加）：
   ```c
   PAL_RESOURCE_SPI_BUS,          /* ESP32 classic max=2 (SPI2/SPI3) */
   PAL_RESOURCE_SPI_CS,
   PAL_RESOURCE_PCNT_UNIT,        /* ESP32 classic max=8 */
   PAL_RESOURCE_PCNT_CHAN,
   PAL_RESOURCE_RMT_CHAN,         /* ESP32 classic max=8 */
   PAL_RESOURCE_HWTIMER,          /* max=PAL_HWTIMERS_MAX=4 */
   PAL_RESOURCE_MCPWM_UNIT,       /* ESP32 classic max=2 */
   PAL_RESOURCE_MCPWM_TIMER,      /* per-unit 0..2 */
   PAL_RESOURCE_MCPWM_OPERATOR,
   PAL_RESOURCE_MCPWM_COMPARATOR,
   PAL_RESOURCE_MCPWM_SYNC_GPIO,  /* Stage 2 T2.3 全局 sync 源仲裁 */
   PAL_RESOURCE_GDMA_CHAN
   ```

2. 新增 target-specific 上界 API：
   ```c
   /* 返回该 type 在当前 target 的最大实例数（不含）。
    * ESP32 classic: SPI_BUS=2, PCNT_UNIT=8, MCPWM_UNIT=2, HWTIMER=4, RMT_CHAN=8, UART=3。
    * host/wasm：与 ESP32 classic 返回相同上限（而非「足够大」的 SIM_MAX）。
    *   理由：pal_resource_claim 越界边界测试（如第三路 SPI bus 返回 INVALID_ARG）
    *   必须在三 target 同等有效；若 host 返回大山，该边界用例永远不触发，跨平台测试失效。
    *   若某 host 测试场景确需超出硬件上限的资源数，通过
    *   CMake option -DPAL_RESOURCE_HOST_MAX_OVERRIDE=n 覆盖（默认禁用）；
    *   覆盖时 CI 中必须有注释说明为何偏离真机约束。 */
   uint32_t pal_resource_max(pal_resource_type_t type);
   ```

3. `pal_resource_claim` 内部强制 `if (id >= pal_resource_max(type)) return WINK_ERR_INVALID_ARG;`，Debug 构建 `assert`。

4. 每 target 编译期 `_Static_assert` 常量与硬件手册一致：
   ```c
   #if defined(CONFIG_IDF_TARGET_ESP32)
   _Static_assert(PAL_SPI_BUS_MAX == 2, "ESP32 classic has 2 DMA-capable SPI hosts");
   _Static_assert(PAL_PCNT_UNIT_MAX == 8, "");
   _Static_assert(PAL_MCPWM_UNIT_MAX == 2, "");
   #endif
   ```

5. MCPWM 多实例 ID 编码可用 `(unit << 8) | sub_id`，参考既有 `pal_resource_i2c_id`。
6. Host/Wasm `PAL_RESOURCE_MAX_CLAIMS` 从 32 提到 >= 64。

**验收**：
- [ ] `pal_resource_claim(PAL_RESOURCE_SPI_BUS, 2, "x")` 在 ESP32 classic **和 host/wasm** 均返回 `WINK_ERR_INVALID_ARG`（三 target 边界测试等效）；
- [ ] 重复 claim 同 id 返回 `WINK_ERR_BUSY`；
- [ ] host/wasm 与 esp32 同型错误码；
- [ ] `pal_resource_max(PAL_RESOURCE_SPI_BUS)` 在 host 返回 2（与 ESP32 classic 一致），单测断言验证；
- [ ] 故意把 ESP32 classic max 改 mismatch，`_Static_assert` 触发编译失败。

### T0.3 `pal_spi`：主机 + DMA 异步

**API（`pal/include/hal/pal_spi.h`）**：

```c
typedef struct {
    uint8_t    spi_bus;     /* 0 = FSPI/HSPI, 按 target 映射 */
    wink_pin_t sclk;
    wink_pin_t mosi;
    wink_pin_t miso;
    uint32_t   clock_hz;    /* <= 40 MHz ESP32, <= 80 MHz S3 */
    uint8_t    mode;        /* 0..3 CPOL/CPHA */
    bool       dma_enabled;
} pal_spi_bus_config_t;

typedef struct {
    wink_pin_t cs_pin;
    uint32_t   clock_hz;
    uint8_t    mode;
    bool       cs_active_high;
    uint16_t   cs_setup_ns;
    uint16_t   cs_hold_ns;
} pal_spi_device_config_t;

typedef struct pal_spi_device_s *pal_spi_device_handle_t;

/* 异步完成回调。
 * ESP-IDF: 在 ISR 上下文触发（spi_transaction_event_t）。
 * host: 直接在调用线程触发。
 * wasm: 软中断上下文（Stage 3 拉模型）。
 * 回调内严禁阻塞 / log / malloc；可使用 FromISR 后缀的 FreeRTOS API。 */
typedef void (*pal_spi_dma_callback_t)(void *arg, wink_status_t result);

wink_status_t pal_spi_init_bus(const pal_spi_bus_config_t *cfg);
wink_status_t pal_spi_add_device(uint8_t bus, const pal_spi_device_config_t *cfg,
                                 pal_spi_device_handle_t *out);

wink_status_t pal_spi_transfer_dma(pal_spi_device_handle_t dev,
                                   const uint8_t *tx_buf, uint8_t *rx_buf,
                                   size_t len,
                                   pal_spi_dma_callback_t cb, void *cb_arg);

/* 同步轮询传输；仅允许在 WINK_BLOCKING 路径或测试中使用，严禁 ISR 调用 */
wink_status_t pal_spi_transfer_polling(pal_spi_device_handle_t dev,
                                       const uint8_t *tx, uint8_t *rx, size_t len);
```

**约束**：
- DMA 缓冲必须 `PAL_DMA_BUF_ATTR` + 4 字节对齐（内部 RAM，ESP32 classic 不可放 PSRAM）。
- ESP32 classic 支持 DMA 的 SPI 主机只有 SPI2/SPI3（即 `spi_bus=0,1`），由 `pal_resource_max(PAL_RESOURCE_SPI_BUS)=2` 静态封顶；`init_bus` 第三路返回 `WINK_ERR_INVALID_ARG`（红线 7）。
- 所有句柄/描述符在 `init_bus`/`add_device` 阶段从静态池分配，运行期不调 `malloc`（红线 6）。池中 `PAL_SPI_DEV_MAX_PER_BUS` 编译期可配，默认 4。
- ESP-IDF 走 `spi_device_queue_trans` + 完成 ISR；**host 不做简单的 "TX 即 RX" loopback**——见下方 host 注入钩子；wasm 转发已声明的 `js_pal_spi_transfer`（`wasm_bridge.h:97`），完成走 Stage 3 拉模型。

**Host target 测试钩子（必须）**：
```c
/* test/unit/host/pal_spi_stub.h — only in host/debug builds */
void stub_spi_inject_rx(uint8_t bus, const uint8_t *bytes, size_t len);
void stub_spi_get_last_tx(uint8_t bus, uint8_t *out, size_t *out_len);
void stub_spi_force_failure(uint8_t bus, wink_status_t err);
```

- `pal_spi_transfer_dma` 在 host 上不把 TX 直接回环到 RX；RX 来自 `stub_spi_inject_rx` 队列；TX 拷贝到 last_tx 环形历史供断言；
- 这对 `dal_sdcard` / `dal_tft` / SPI 传感器协议测试是必须的（评审 S4）。

**验收**：
- ESP32 1024 B @ 40 MHz DMA 逻辑分析仪实测 < 250 µs；
- 连续 10000 次传输无描述符泄漏、`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` 不下降（红线 6）；
- 重复 `init_bus` 返回 `WINK_ERR_BUSY`；第三路 bus 返回 `WINK_ERR_INVALID_ARG`；
- host 单测：注入 SDCard R1/R3 响应序列，DAL 能正确解析。

### T0.4 `pal_rmt`：多通道句柄 + TX 发射

**现状**：`pal_rmt.h` 单例、RX-only、`with_dma=false`（`pal_hal_rmt_esp32.c:72`）。WS2812 无法发射。

**API（重写 `pal_rmt.h`）**：

```c
typedef struct pal_rmt_channel_s *pal_rmt_channel_handle_t;

typedef enum { PAL_RMT_DIR_RX, PAL_RMT_DIR_TX } pal_rmt_dir_t;

typedef struct {
    uint8_t        channel_id;     /* 0..7; pal_resource 仲裁 */
    wink_pin_t     gpio;
    pal_rmt_dir_t  direction;
    uint32_t       resolution_hz;  /* 典型 10 MHz = 100ns/tick */
    size_t         mem_block_symbols;
    bool           dma_enabled;    /* WS2812 长灯带 TX 必须 DMA */
} pal_rmt_channel_config_t;

/* RMT 符号：两电平持续时间。
 * ADR-0002 禁止跨平台使用 C 位域与 #pragma pack。
 * 不使用 uint32_t 位域；电平状态用独立字节，duration 用 uint16_t。 */
typedef struct {
    uint16_t duration0_ticks;  /* 0..32767 */
    uint16_t duration1_ticks;
    uint8_t  level0;           /* 0 or 1 */
    uint8_t  level1;
    uint8_t  _pad[2];
} pal_rmt_symbol_t;

wink_status_t pal_rmt_acquire_channel(const pal_rmt_channel_config_t *cfg,
                                      pal_rmt_channel_handle_t *out);
wink_status_t pal_rmt_release_channel(pal_rmt_channel_handle_t ch);

/* TX：symbols 必须 PAL_DMA_BUF_ATTR；异步；cb 上下文 = ISR */
wink_status_t pal_rmt_tx_send(pal_rmt_channel_handle_t ch,
                              const pal_rmt_symbol_t *symbols, size_t count,
                              void (*cb)(void *arg, wink_status_t), void *arg);

/* RX：注册脉冲宽度回调（ISR 上下文） */
wink_status_t pal_rmt_rx_set_callback(pal_rmt_channel_handle_t ch,
                                      void (*on_symbol)(void *arg, const pal_rmt_symbol_t *s),
                                      void *arg);
wink_status_t pal_rmt_rx_start(pal_rmt_channel_handle_t ch);
wink_status_t pal_rmt_rx_stop(pal_rmt_channel_handle_t ch);
```

> 旧文档示例的位域结构体
> ```c
> typedef struct { uint32_t level0_duration_ns:15; uint32_t level0_state:1; ... } pal_rmt_symbol_t;
> ```
> 违反 ADR-0002（跨平台位域），**禁止提交**。

**Reset / 帧尾契约（WS2812 等时序协议）**：
- RMT 硬件在 symbol 流末尾不会自动产生 WS2812 所需的 ">50 µs 低电平复位"；驱动必须在 `pal_rmt_tx_send` 的 symbols 末尾显式追加一个 end-symbol，例如 `{.duration0_ticks = reset_ticks, .level0 = 0, .duration1_ticks = 0, .level1 = 0}`，其中 `reset_ticks = resolution_hz * 50µs / 1e6`（10 MHz 分辨率下 = 500 ticks）。
- API 提供 helper：`pal_rmt_symbol_t pal_rmt_make_reset_symbol(uint32_t resolution_hz, uint32_t hold_low_us);` 由 WS2812 DAL 调用，避免每个协议重新算。
- 验收：逻辑分析仪抓 144 LED 帧末段，低电平持续 $\ge 55$ µs（留 5 µs 裕量）。

**验收**：
- WS2812 30/60/144 LED 整帧 TX：$T_{0H}=350\pm20$ ns、$T_{1H}=700\pm20$ ns、Reset > 50 µs（示波器/逻辑分析仪）；
- 多通道并发（1× TX WS2812 + 1× RX DHT22）无撕裂；
- DMA 模式 144 LED 帧 < 5 ms 且 CPU 占用 < 5%；
- 句柄/符号缓冲在 `acquire_channel` 阶段从静态池分配，运行期无 `malloc`（红线 6）。

### T0.5 `pal_pcnt`：硬件正交 + 64 位累加

**现状**：编码器走 GPIO ISR 软件计数（`dal_encoder.c:47-59`），50 kHz 必然丢步；X2/X4 返回 UNSUPPORTED。

**API（`pal/include/hal/pal_pcnt.h`）**：

```c
typedef struct pal_pcnt_unit_s *pal_pcnt_unit_handle_t;

typedef enum {
    PAL_PCNT_COUNT_SINGLE,
    PAL_PCNT_COUNT_QUAD_X1,
    PAL_PCNT_COUNT_QUAD_X2,
    PAL_PCNT_COUNT_QUAD_X4,
} pal_pcnt_quadrature_t;

typedef struct {
    uint8_t               unit_id;   /* 0..7 */
    wink_pin_t            pin_a;
    wink_pin_t            pin_b;
    pal_pcnt_quadrature_t mode;
    int16_t               watch_threshold; /* ± 值，触发累加 ISR，建议 30000 */
    uint16_t              glitch_filter_ns;/* 典型 1000 */
} pal_pcnt_config_t;

wink_status_t pal_pcnt_acquire(const pal_pcnt_config_t *cfg,
                               pal_pcnt_unit_handle_t *out);
wink_status_t pal_pcnt_release(pal_pcnt_unit_handle_t u);
wink_status_t pal_pcnt_start(pal_pcnt_unit_handle_t u);
wink_status_t pal_pcnt_pause(pal_pcnt_unit_handle_t u);
wink_status_t pal_pcnt_clear_count(pal_pcnt_unit_handle_t u);

/* 读取 64 位累计计数。
 * 硬件仅 16 位；driver 层在 watch_threshold 命中时由 PAL_ISR
 * __atomic_fetch_add(&accum, (int64_t)hw, __ATOMIC_RELAXED) 累加。
 * get_count：base = __atomic_load_n(&accum, ACQUIRE); return base + hw_read(); */
wink_status_t pal_pcnt_get_count(pal_pcnt_unit_handle_t u, int64_t *out_count);
```

**实现要点**：
- ESP32 PCNT 单元 16 位有符号；注册 `pcnt_event_callbacks_t` 监听 `on_reach` + `on_cross_zero`，在 `PAL_ISR` 中累加。
- 读侧使用 `__atomic_load_n(..., __ATOMIC_ACQUIRE)`；累加侧用 RELAXED（单调且无并发写）。
- **硬件勘误 E-001（master §10）**：ESP32 classic PCNT 在 `glitch_filter_ns < 1000` 时存在 LEAKAGE 计数（silicon bug），高速输入下偶发 ±1 偏差。`pal_pcnt_config_t::glitch_filter_ns` 默认值必须 $\ge 1000$；若应用要求更低滤波，需在文档中显式标注已知偏差。
- Host/Wasm：plugin 注入边沿事件序列，driver 维护同型 64 位累加（Stage 3 软步进）。

**验收**：
- 50 kHz 正交方波，**连续 1 分钟（300 万边沿）零丢步**（基于 64 位 `get_count`）；
- 阈值 ISR 抖动 < 5 µs；
- X4 相对 X1 同物理旋转计数恰好 4×。

### T0.6 `pal_uart`：事件队列 + 空闲中断 + DMA 环形缓冲

**现状**：`uart_driver_install(..., rx_buffer_size=0, ...)`（`pal_hal_uart_esp32.c:42`），无事件队列、无 idle ISR；GPS NMEA / Modbus 分帧只能轮询。

> **命名约定（PLAN-PRE-STAGE0-PAL-NAMING v2 已敲定）**：ESP32 target 下 HAL 层 TU 一律 `pal_hal_<module>_esp32.c`，服务层 TU 无前缀。本任务**不新增 `pal_uart_ex_esp32.c`、不新增 `pal_uart_ex.h`**——同硬件同端口的新旧 API 同居一个头、一个 TU，共享 static driver state。

**调用方分析（全仓 grep 结论）**：
- `targets/esp32/CMakeLists.txt:49,125`：编译进 esp32 target（唯一真实链接者）
- `pal_wasm_ch2_uart.c`：wasm target 的同名 API 实现，与 esp32 文件互斥编译，已有完整 ring buffer + 软 IRQ，语义已对齐
- `test/unit/dal/test_dal_uart_rx_sim.c`：host 单测，链接 wasm 实现，**不链接** esp32 文件，零影响
- `dal/src/`：**零调用**—— `dal_gps` 是全 stub，旧 `pal_uart_*` API 在 DAL 层当前为死代码

**处置策略（原地强化，不新建文件、不新建并行 API 层）**：
1. **`pal/include/hal/pal_uart.h` 原地扩写**：保留旧同步 API 声明，在同一头文件下方追加异步事件 API 声明（见下方代码块）。CMakeLists 不增不删。
2. **`pal_hal_uart_esp32.c` 原地强化**：
   - 将 `uart_driver_install(port, 256*2, 0, 0, NULL, 0)` 改为参数化版本（`rx_ring_buffer_bytes` 可配、`event_queue` 启用）；
   - 在同一 TU 内新增 `pal_uart_open/close` 实现，注册 idle ISR、FIFO 阈值回调、event queue task；
   - 新旧两组 API 共享同一组 static driver state（端口句柄表、ring buffer、ISR handler），避免跨 TU 内部函数或双重 init；
   - 文件名、CMakeLists 引用均不变。
3. **`pal_wasm_ch2_uart.c` 同步扩写**：在已有 ring buffer + 软 IRQ 基础上补 `pal_uart_open/close` 语义，保持三 target 同头同契约。
4. **旧 `pal_uart_init/read/write/deinit` 接口保留不动**：wasm sim 和现有测试依赖该签名。DAL GPS 在 Stage 4 实现时直接使用 `pal_uart.h` 的新异步 API，不依赖旧同步接口。未来旧 API 真无调用方时再用 `WINK_DEPRECATED` 标记并独立 PR 删除。

**同端口互斥约束**：同一物理 UART port 不可被新旧 API 同时占用。实现侧通过以下任一方式强制（择一，评审定）：
- 优先方案：在 `pal_resource` 表中对 UART port 做 claim，`pal_uart_init` 与 `pal_uart_open` 都先 claim，冲突返回 `WINK_ERR_BUSY`；
- 兜底方案：`pal_uart_open` 内部检查该 port 是否已被 `pal_uart_init` 占用，是则返回 `WINK_ERR_INVALID_STATE`，反之亦然。

**API（直接追加到 `pal/include/hal/pal_uart.h`，不新建头文件）**：

```c
/* ── 旧同步 API（保留，签名不变） ── */
wink_status_t pal_uart_init(uint8_t port, uint8_t tx_pin, uint8_t rx_pin, uint32_t baud_rate);
void           pal_uart_deinit(uint8_t port);
wink_status_t pal_uart_read(uint8_t port, uint8_t *buf, uint32_t len, uint32_t *out_read);
wink_status_t pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len);

/* ── Stage 0 新增：异步事件 API ── */
typedef struct pal_uart_bus_s *pal_uart_handle_t;

typedef enum {
    PAL_UART_EVT_RX_IDLE,      /* 总线静默 >= idle_to_us */
    PAL_UART_EVT_RX_FIFO_HIGH, /* FIFO 达到阈值 */
    PAL_UART_EVT_BREAK,
    PAL_UART_EVT_FRAMING_ERR,
} pal_uart_event_type_t;

typedef struct {
    pal_uart_event_type_t type;
    size_t                bytes_available;
} pal_uart_event_t;

/* 任务上下文（来自 uart event queue task），非 ISR */
typedef void (*pal_uart_event_cb_t)(void *arg, const pal_uart_event_t *ev);

typedef struct {
    uint8_t              port;
    uint32_t             baud_rate;
    wink_pin_t           tx_pin;
    wink_pin_t           rx_pin;
    size_t               rx_ring_buffer_bytes;
    size_t               rx_fifo_threshold;
    uint32_t             idle_to_us;        /* NMEA 1000, Modbus 3500 */
    pal_uart_event_cb_t  event_cb;
    void                *event_cb_arg;
} pal_uart_config_t;

wink_status_t pal_uart_open(const pal_uart_config_t *cfg, pal_uart_handle_t *out_h);
void           pal_uart_close(pal_uart_handle_t h);

/* 以下两个读写函数在句柄模型下使用；与旧 port 模型的同名函数靠参数类型重载（C 无重载，见注） */
wink_status_t pal_uart_handle_read(pal_uart_handle_t h, uint8_t *buf, size_t len,
                                   uint32_t timeout_us, size_t *out_read);
wink_status_t pal_uart_handle_write(pal_uart_handle_t h, const uint8_t *buf, size_t len);
```

> **命名注记**：旧 `pal_uart_read(port, ...)` 与新 `pal_uart_read(handle, ...)` 在 C 下签名冲突，不能重载。新句柄版读写函数命名为 `pal_uart_handle_read/write`（或 `pal_uart_read_evt` / `pal_uart_read_h`，评审定）。`pal_uart_open/close` 不冲突，保留。Stage 4 DAL GPS 只用句柄版 API。

> Wasm 侧三个推流符号（`pal_wasm_push_uart_rx_byte/error`、`pal_wasm_get_uart_rx_available`）**已存在**于 `wasm_bridge.h:332` + `targets/wasm/pal_wasm_ch2_uart.c:42`。本任务只是让真机侧与 wasm 侧语义对齐；不要重复定义。

**验收**：
- 9600/115200/921600 连续 1 h 收发零丢字节；
- 1000 条 NMEA 注入，idle 事件恰好触发 1000 次；
- Wi-Fi + WS2812 并发压力下无 framing error 漏报；
- 同一 port 上新旧 API 同时调用时，后调用方返回 `WINK_ERR_BUSY` 或 `WINK_ERR_INVALID_STATE`，不允许双重 init。

### T0.7 ESP-IDF CMake 与版本门控

**现状**：`wink-micro-os/targets/esp32/CMakeLists.txt` REQUIRES 缺 `esp_driver_spi/pcnt/gptimer/mcpwm/gdma`。

**交付**：
- REQUIRES 增补：`esp_driver_spi`、`esp_driver_pcnt`、`esp_driver_gptimer`、`esp_driver_mcpwm`、`esp_driver_gdma`、`esp_driver_rmt`（若未显式列出）。
- 版本门控 `if(IDF_VERSION_MAJOR EQUAL 5 AND IDF_VERSION_MINOR GREATER_EQUAL 4)`；< 5.4 编译期 `#error "ESP-IDF >= 5.4 required"`。
- CI 增加 IDF 5.4 与 IDF 6.0 双版本构建矩阵。

### T0.8 ADR-0043 lint 规则更新

**规则文件真实路径（已确认，T0.0 阻塞项关闭）**：
- 绝对路径：`D:\MyWorkSpace_program\lowcode-nocode\ai-app\wink-ai\packages\wink-tools\tools\lint\rules\`
- 已有规则文件：`layering.yaml`、`api.yaml`、`dal.yaml`、`i18n.yaml`、`user_surface.yaml`

**运行命令**（从 `wink-ai-embedded` 根目录执行）：
```bash
python ..\wink-ai\packages\wink-tools\wink.py lint --pack layering --pack api
```
CI 中使用 monorepo 绝对路径或 `WINK_TOOLS_PATH` 环境变量引用，与 CI 工具链约定一致。

**交付**：
- `layering.yaml` 新增/更新 PAL 头文件层注册：`pal_spi.h`、`pal_pcnt.h`、`pal_rmt.h`（重写）；`pal_uart.h` 已登记但需补充新异步 API 符号归属；Stage 2 的 `pal_hwtimer.h`、`pal_mcpwm.h`、`pal_atomic.h` 一并登记。（不再有 `pal_uart_ex.h`，按 PLAN-PRE-STAGE0-PAL-NAMING v2 该头已取消。）
- `api.yaml` 规则补充：回调类型名以 `_cb_t`/`_callback_t`/`_isr_t` 结尾；ISR 上下文回调 Doxygen 必须含 `ISR context`。
- `python wink.py lint --pack layering --pack api` 零错误。

---

## 3. 三 Target 覆盖矩阵

| 模块 | ESP32 | Host | Wasm |
|---|---|---|---|
| `pal_spinlock_t` | `portMUX_TYPE` + `taskENTER_CRITICAL` | 编译屏障（debug 可换 `pthread_mutex`） | 编译屏障（单线程） |
| IRAM/DMA 宏 | 真实段属性 | 空宏 | 空宏 |
| `pal_resource` + max | 真实仲裁 + target-specific 上限 | 内存表 + 大上限 | 内存表 + 大上限 |
| `pal_spi` | `spi_device_queue_trans` + GDMA；静态句柄池 | `stub_spi_inject_rx` 响应注入 + TX 历史 | 转发 `js_pal_spi_transfer`，完成走 Stage 3 拉模型 |
| `pal_rmt` TX | RMT TX + DMA；驱动追加 reset symbol | 符号时间戳注入 plugin | ADR-0012：未实现 variant 返回 UNSUPPORTED；不另立 `js_pal_rmt_*` |
| `pal_rmt` RX | RMT RX + ISR cb | 脉冲宽度注入桩 | CH1 pin-event 复用现有机制 |
| `pal_pcnt` | PCNT + threshold ISR 64 位；`glitch_filter >= 1000ns`（E-001） | 边沿序列 64 位累加 | plugin 软步进，同 host |
| `pal_uart` event | event queue + idle ISR | ring buffer + 软 idle | 复用 `pal_wasm_push_uart_rx_byte` |

---

## 4. 验收门槛（进入 Stage 1 前置）

- [ ] T0.0 三项 blocker 在 CI 中可见可跑（lint 路径 / 四 target 矩阵 / `_Static_assert`）；
- [ ] 公共头：新增 4 个（`pal_spinlock.h`、`pal_spi.h`、`pal_pcnt.h`、`pal_resource.h` 扩展）+ 重写 1 个（`pal_rmt.h`）+ 原地扩写 1 个（`pal_uart.h` 加异步事件 API，不新增 `pal_uart_ex.h`），3 target 实现合并；
- [ ] `grep -rn "portMUX_TYPE\|portMUX_INITIALIZER" targets/ osal/ pal/` 仅在 `pal_spinlock.h` 命中；
- [ ] `grep -rnE ":[[:space:]]+[0-9]+[[:space:]]*;" pal/ targets/` 在位域上下文中零命中（ADR-0002）；
- [ ] `python ..\wink-ai\packages\wink-tools\wink.py lint --pack layering --pack api` 零错误；
- [ ] 所有新模块 host Unity 单测通过，含 host SPI RX 注入用例；
- [ ] ESP32 真机量化：SPI 40 MHz 1024 B < 250 µs；WS2812 30 LED 脉宽 + reset > 50 µs；PCNT 50 kHz × 1 min 零丢步；UART 115200 × 1 h 零丢字节；
- [ ] 运行期 heap 水位零增长：所有新 PAL 模块 init 后 10000 次操作 `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` 不下降（红线 6）；
- [ ] `pal_resource_max` 边界单测：越界 id 返回 `WINK_ERR_INVALID_ARG`；
- [ ] `pal/include/` 公共头无任何 `#ifdef SIMULATION`（ADR-0003）；
- [ ] Stage 0 集成冒烟（master §9）：WS2812 + SPI OLED + PCNT encoder + UART GPS 四路并发 5 min 无崩溃。
