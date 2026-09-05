# 3.2 平台抽象层 (PAL) API 设计规范 (OSAL & HAL)

<!-- i18n-meta
source: docs/zh/design/02-wink-micro-os/02-pal-platform-abstraction.md
translated: 2026-08-17
glossary-version: v1.0
translator: AI-assisted
sync-status: up-to-date
-->

The Platform Abstraction Layer (PAL, Platform Abstraction Layer) is the unified contract interface layer in the WinkMicroOS kernel that shields upper layers from physical chip differences and operating system variations.

---

## 1. PAL 架构分层设计思想

PAL consists of two major sub-layers:
1. **HAL (Hardware Abstraction Layer / Hardware Bus & Peripheral Interfaces)**: Uniformly abstracts initialization, read/write, and interrupt services for foundational communication buses including GPIO, PWM, I2C, SPI, and ADC.
2. **OSAL (OS Abstraction Layer / OS & Kernel Environment Abstraction)**: Uniformly abstracts kernel services such as high-precision system Ticks, microsecond/millisecond blocking delays, and thread synchronization mutexes.

```text
  ┌────────────────────────────────────────────────────────┐
  │                 器件抽象层 (DAL Drivers)                │
  └───────────────────────────┬────────────────────────────┘
                              │ 调用
                              ▼
  ┌────────────────────────────────────────────────────────┐
  │             平台抽象层 (PAL) [统一契约定义]             │
  │   - pal_hal.h (总线/外设)    - pal_osal.h (系统服务/同步)  │
  └───────────────────────────┬────────────────────────────┘
                              │
            ┌─────────────────┴─────────────────┐ (CMake 静态装配路由 - ADR-0041)
            ▼                                   ▼
  ┌──────────────────┐                ┌──────────────────┐
  │ 硬件外设适配层   │                │ 操作系统适配层   │
  │ (TARGET_PLATFORM)│                │ (WINK_OSAL_TYPE) │
  │  - targets/wasm  │                │  - osal/wasm     │
  │  - targets/esp32 │                │  - osal/freertos_│
  │  - targets/host  │                │    esp32         │
  │  - targets/stm32 │                │  - osal/host     │
  └──────────────────┘                └──────────────────┘
```

In order to achieve optimal execution efficiency, PAL in physical targets does **not** employ dynamic C++ virtual method tables (vtables) or C runtime function pointer registration polymorphism. Instead, it utilizes **CMake static conditional compilation bindings** orthogonal across `TARGET_PLATFORM` (HAL) and `WINK_OSAL_TYPE` (OSAL) per [ADR-0041](../../decisions/core/0041-hal-osal-directory-orthogonality.md), eliminating all runtime wrapper overhead.

---

## 1.1 核心架构决策摘要（ADR 回写）

> This section is the authoritative living backport for [ADR-0001](../../decisions/core/0001-error-code-sign-convention.md), [ADR-0002](../../decisions/unisim/0002-dual-target-compilation.md), [ADR-0004](../../decisions/core/0004-static-dispatch-vs-runtime-ops.md), [ADR-0012](../../decisions/core/0012-contract-honesty-over-silent-degradation.md), and [ADR-0025](../../decisions/core/0025-app-blocking-api-honesty-pragma-convention.md).

| ADR 编号 | 决策主题 | 核心约定 |
|---|---|---|
| **ADR-0001** | Negative error code convention | ✅ All fallible functions return `wink_status_t`<br>✅ `0 = WINK_OK = Success`<br>✅ **Negative values = Error** (e.g. `-1 = WINK_ERR_INVALID_ARG`)<br>✅ Checking pattern: `status < 0` or `wink_status_is_error(status)`<br>📘 See [Error Model Spec §11](../07-platform-governance/02-error-fault-model.md#11-ai-codegen-错误码语义详表) |
| **ADR-0002** | Dual-target compilation | ✅ Single C codebase compiles simultaneously to Emscripten/Wasm32 and ESP-IDF/xtensa<br>✅ CMake statically routes implementation files per target platform<br>✅ Simulation code strictly isolated in `targets/*/` + `#if defined(SIMULATION)` |
| **ADR-0004** | Compile-time static dispatch | ✅ **Forbidden vtables**, forbidden runtime `ops` function pointer tables, forbidden `container_of`<br>✅ Named API + POD structs paradigm, static bindings at compile time<br>✅ DAL instances are pure data structs manipulated by named functions |
| **ADR-0006** | ESP-IDF v6.x I2C compatibility | ✅ ESP-IDF v5.x $\rightarrow$ v6.x I2C API breaking changes smoothed by PAL<br>✅ MVP fixed GPIO mappings (I2C0: 21/22, I2C1: 33/32) |
| **ADR-0012** | Contract honesty over silent degradation | ✅ PAL/HAL header commitments must strictly align with target implementations; unsupported targets **explicitly return `WINK_ERR_UNSUPPORTED`**<br>✅ Cross-target behavioral divergence documented explicitly in Doxygen headers<br>✅ Target capability evaluations mandatory for new APIs |
| **ADR-0025** | App blocking honesty pragma convention | ✅ **Minimal warning suppression**: Prohibits file-scope naked pragmas in App event callbacks or main loops.<br>✅ **Compile-time macros**: PAL/Runtime provides `WINK_INTERNAL_BLOCKING_REGION_BEGIN/END` and `WINK_INIT_BLOCKING_REGION_BEGIN/END`.<br>✅ **Wasm STRICT_NONBLOCKING=1**: Fail-fast compilation and link time interception of illegal blocking. |
| **ADR-0047** | FOC ISR layering & `pal_hwtimer` | ✅ Public contract `pal_hwtimer_*` + PWM-ADC hardware triggers<br>✅ Callback ABI: **IRAM-safe** (`IRAM_ATTR`); forbidden flash access / `pal_log` / malloc / blocking<br>✅ Separate registration for Periodic Control vs nFAULT Protection |

---

## 2. HAL 抽象总线接口规范 (`pal_hal.h`)

`pal_hal.h` defines foundational hardware controller interfaces.

### 2.1 完整 API 定义

```c
#ifndef PAL_HAL_H
#define PAL_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

/* --- 1. GPIO 数字输入输出 --- */

typedef enum {
    PAL_GPIO_INPUT,
    PAL_GPIO_INPUT_PULLUP,
    PAL_GPIO_INPUT_PULLDOWN,
    PAL_GPIO_OUTPUT_PUSH_PULL,
    PAL_GPIO_OUTPUT_OPEN_DRAIN
} pal_gpio_mode_t;

typedef enum {
    PAL_GPIO_INTR_DISABLE,
    PAL_GPIO_INTR_RISING_EDGE,
    PAL_GPIO_INTR_FALLING_EDGE,
    PAL_GPIO_INTR_ANY_EDGE
} pal_gpio_intr_t;

typedef void (*pal_gpio_isr_t)(void *arg);

WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_init(uint16_t pin, pal_gpio_mode_t mode);
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_write(uint16_t pin, bool level);
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_read(uint16_t pin, bool *out_level);
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_enable_interrupt(uint16_t pin, pal_gpio_intr_t intr_type, pal_gpio_isr_t callback, void *arg);
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_enable_interrupt_ex(uint16_t pin, pal_gpio_intr_t intr_type, pal_irq_prio_t prio, pal_gpio_isr_t callback, void *arg);
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_disable_interrupt(uint16_t pin);
WINK_WARN_UNUSED_RESULT wink_status_t pal_gpio_pulse_in(uint16_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us);

/* --- 2. PWM 控制器抽象 --- */

typedef enum {
    PAL_PWM_CLOCK_AUTO            = 0,
    PAL_PWM_CLOCK_STABLE_REQUIRED = 1,
} pal_pwm_clock_requirement_t;

typedef struct {
    uint32_t                    freq_hz;
    uint8_t                     resolution_bits;
    pal_pwm_clock_requirement_t clock_requirement;
} pal_pwm_config_t;

WINK_WARN_UNUSED_RESULT wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz);
WINK_WARN_UNUSED_RESULT wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg);
WINK_WARN_UNUSED_RESULT wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent);

/* --- 3. I2C 串行总线抽象 --- */

WINK_WARN_UNUSED_RESULT wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len);

#endif // PAL_HAL_H
```

### 2.2 `pal_hwtimer` / PWM–ADC sync 契约草案（ADR-0047）

> Status: Contract Draft per [ADR-0047](../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md).

#### 2.2.1 公共契约方向

| 项 | 约定 |
|---|---|
| API 形态 | Public `pal_hwtimer_*` named static APIs (ADR-0004), no runtime ops tables |
| 用途 | (a) Periodic fast-loop control ($\ge 10\text{kHz}$); (b) Hardware PWM-synchronized ADC triggers |
| Target 矩阵 | ESP32: MCPWM / LEDC+ADC; host/wasm: Deterministic virtual-time soft stepping; unsupported: `WINK_ERR_UNSUPPORTED` |
| 禁止 | Target-private hidden timers; exposing hardware timer symbols in BAL public headers |

#### 2.2.2 回调 ABI（R-008）— 施工红线

| # | 要求 | 说明 |
|---|---|---|
| 1 | **IRAM-safe (ESP-IDF)** | Callbacks placed in IRAM via `IRAM_ATTR` / `PAL_ISR` |
| 2 | **No flash access** | Forbidden instruction fetches or `.rodata` reads from flash during execution |
| 3 | **No `pal_log`** | Logging prohibited in ISR fast paths |
| 4 | **No malloc / free** | Zero heap allocations |
| 5 | **No blocking** | Forbidden `pal_delay_*`, mutex locks, busy-waits |
| 6 | **Bounded stack** | Strict stack budget |

#### 2.2.3 两类 ISR 注册入口与优先级（R-007）

| 类 | 角色 | 注册方向（草案） | 优先级 / 时延 |
|---|---|---|---|
| **周期控制 ISR** | Runs BAL pure math + DAL read/write | `pal_hwtimer_register_periodic` $\rightarrow$ DAL/target trampoline | Typically `PAL_IRQ_PRIO_NORMAL` or HIGH |
| **nFAULT 保护 ISR** | Async sub-microsecond shutdown | `pal_hwtimer_register_fault` or GPIO driver dedicated fault entry | Higher than periodic control ISR; fast hardware shutdown |

#### 2.2.4 PWM–ADC 硬件同步触发绑定（R-007 / R-008 配套）

| 方向 | 约定 |
|---|---|
| **真机** | ADC sampling triggered by PWM Timer TRGO / Underflow hardware events |
| **绑定责任** | DAL building blocks declare bindings via PAL during init |
| **仿真降级** | Host/wasm software stepping approximations |

```c
/* pal_hwtimer.h — CONTRACT DRAFT only; Wave C implements */
typedef void (*pal_hwtimer_isr_t)(void *arg);

wink_status_t pal_hwtimer_init(uint8_t timer_id, uint32_t rate_hz);
wink_status_t pal_hwtimer_register_periodic(uint8_t timer_id, pal_hwtimer_isr_t cb, void *arg);
wink_status_t pal_hwtimer_register_fault(uint8_t src_id, pal_hwtimer_isr_t cb, void *arg);
wink_status_t pal_hwtimer_bind_pwm_adc(uint8_t pwm_timer_id, uint8_t adc_unit);
wink_status_t pal_hwtimer_start(uint8_t timer_id);
wink_status_t pal_hwtimer_stop(uint8_t timer_id);
```

### 2.3 PAL ADC 子系统 (`pal_adc.h`) — ADR-0057

> Backport Record: Added 2026-08-05 per [ADR-0057](../../decisions/core/0057-pal-adc-subsystem-and-channel-3-analog-contract.md).

Target-neutral on-chip ADC ($\le 16$-bit) abstraction unlocking Channel 3 simulation for analog peripherals.

```c
typedef uint8_t pal_adc_channel_t;

typedef struct {
    wink_pin_t pin;
    uint16_t  full_scale_mv;
    uint8_t   resolution_bits;
} pal_adc_config_t;

wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg);
void          pal_adc_deinit(pal_adc_channel_t ch);
wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin);
wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch);
wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw);
wink_status_t pal_adc_read_mv(pal_adc_channel_t ch, uint16_t *out_mv);
wink_status_t pal_adc_full_scale_mv(pal_adc_channel_t ch, uint16_t *out_mv);
```

| target | 实现 |
|---|---|
| esp32 | ESP-IDF v6.0.1 `adc_oneshot` + `adc_cali` |
| wasm | `js_pal_adc_read_norm(pin)` + Gaussian noise & RC lowpass |
| host | `pal_host_adc_inject_raw/mv` injection |
| baremetal | `WINK_ERR_UNSUPPORTED` stubs |

---

## 3. OSAL 操作系统抽象规范 (`pal_osal.h`)

### 3.1 完整 API 定义

```c
#ifndef PAL_OSAL_H
#define PAL_OSAL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

/* --- 1. 系统时间与高精度延时 --- */
void pal_delay_ms(uint32_t ms);
void pal_delay_us(uint32_t us);
uint64_t pal_get_ms(void);
uint64_t pal_get_us(void);

/* --- 2. 线程同步互斥锁 (Mutex) --- */
typedef void* pal_mutex_t;
pal_mutex_t pal_mutex_create(void);
WINK_WARN_UNUSED_RESULT wink_status_t pal_mutex_lock(pal_mutex_t mutex, uint32_t timeout_ms);
WINK_WARN_UNUSED_RESULT wink_status_t pal_mutex_unlock(pal_mutex_t mutex);
void pal_mutex_destroy(pal_mutex_t mutex);

/* --- 3. 全局临界区 task/ISR 双入口 (ADR-0016) --- */
uint32_t pal_os_critical_enter(void);
void pal_os_critical_exit(uint32_t key);
uint32_t pal_os_critical_enter_isr(void);
void pal_os_critical_exit_isr(uint32_t key);

void pal_os_set_sim_isr_context(bool in_isr);
bool pal_os_in_sim_isr_context(void);

#endif // PAL_OSAL_H
```

### 3.2 临界区双入口契约（ADR-0016）

| 调用点上下文 | 应使用 | 后果 |
|---|---|---|
| task 上下文（`app_loop`、DAL 主流程） | `pal_os_critical_enter/exit` | 正常互斥 |
| ISR 回调（GPIO ISR wrapper、timer ISR、fault handler） | `pal_os_critical_enter_isr/exit_isr` | 正常互斥 |
| task 里调 `_isr` 版 | ❌ 禁止 | task/task 竞态未保护 |
| ISR 里调 task 版 | ❌ 禁止 | ESP32 assert / SMP deadlock |

| Target | task 版实现 | ISR 版实现 |
|---|---|---|
| ESP32 | `portENTER_CRITICAL(&s_global_mux)` | `portENTER_CRITICAL_ISR(&s_global_mux)` |
| host | no-op + `assert(!s_sim_in_isr)` | no-op + `assert(s_sim_in_isr)` |
| wasm | no-op + `assert(!s_sim_in_isr)` | no-op + `assert(s_sim_in_isr)` |
| baremetal | 关中断 | 关中断 |

### 3.3 PAL IRQ 公开面收窄（ADR-0018）

| 分类 | 收窄后 |
|---|---|
| Handler 原型 | `pal_isr_t (void *arg)` 一个 |
| 优先级枚举 | `PAL_IRQ_PRIO_LOW / NORMAL / HIGH` |
| 注册控制 | `pal_irq_enable / pal_irq_disable / pal_irq_set_pending / pal_irq_clear_pending` |
| 临界区 | `PAL_CRITICAL_SECTION(code)` |
| 属性宏 | `PAL_ISR`, `PAL_DEFINE_ISR(name, T, arg)` |

| Target | LOW | NORMAL | HIGH |
|---|---|---|---|
| ESP32 | `ESP_INTR_FLAG_LEVEL1` | `ESP_INTR_FLAG_LEVEL2` | `ESP_INTR_FLAG_LEVEL3` |
| host | 仿真调度顺序 | 仿真调度顺序 | 仿真调度顺序 |
| wasm | 仿真调度顺序 | 仿真调度顺序 | 仿真调度顺序 |
| baremetal | NVIC 低段 | NVIC 中段 | NVIC 高段 |

---

## 4. 各平台 (Targets) 移植绑定规范

### 4.1 ESP32 (基于 ESP-IDF & FreeRTOS)
- `pal_delay_ms` $\rightarrow$ `vTaskDelay`.
- `pal_get_us` $\rightarrow$ `esp_timer_get_time()`.
- `pal_gpio_init` $\rightarrow$ `gpio_config()`.
- `pal_i2c_transfer` $\rightarrow$ `driver/i2c_master.h` or `driver/i2c.h` ([ADR-0006](../../decisions/core/0006-esp-idf-v6-i2c-compatibility.md)).
- `pal_gpio_pulse_in` $\rightarrow$ Hardware RMT / Dual-edge GPIO ISR.

### 4.2 STM32 (基于 STM32 HAL & FreeRTOS)
- `pal_delay_us` $\rightarrow$ Hardware timer spinlocks (TIM6/TIM7).
- `pal_delay_ms` $\rightarrow$ `osDelay`.
- `pal_mutex` $\rightarrow$ `SemaphoreHandle_t`.

### 4.3 WebAssembly 仿真端 (基于 Emscripten JS 桥接)
- `pal_gpio_write` $\rightarrow$ `js_pal_gpio_write(pin, level)`.
- `pal_delay_ms` $\rightarrow$ Asyncify suspension:
  ```c
  extern void js_pal_delay_ms(uint32_t ms);
  void pal_delay_ms(uint32_t ms) {
      js_pal_delay_ms(ms);
  }
  ```

---

## 4.4 Target 内公共设施（`targets/common/`）

| 文件 | 用途 | 引入日期 | 相关文档 |
|---|---|---|---|
| `wink_sim_physical.{h,c}` | 物理退化算法库，wasm & host target 复用 | 2026-06-28 | [ADR-0009](../../decisions/unisim/0009-physical-behavior-simulation-fault-injection.md) |

### 设计约束
- Never exported to `WINK_CORE_INCLUDE_DIRS`.
- Static allocation without `malloc`.

---

## 4.1 资源占用治理 (`pal_resource.h`)

| 资源类型 | 粒度 | 说明 |
|---|---|---|
| `PAL_RESOURCE_GPIO_PIN` | 单引脚 | DAL 以 `cfg->owner` claim |
| `PAL_RESOURCE_PWM_CHANNEL` | 单通道 | DAL 以 `cfg->owner` claim |
| `PAL_RESOURCE_I2C_PORT` | 整端口 | 保留 |
| `PAL_RESOURCE_I2C_ADDR` | `(port, 7位地址)` | DAL 以 device-owner claim |
| `PAL_RESOURCE_UART_PORT` | 单端口 | DAL 以 `cfg->owner` claim |
| `PAL_RESOURCE_ADC_CHANNEL` | 单逻辑 ADC 通道 | 同时 claim 对应 `GPIO_PIN` |
| `PAL_RESOURCE_PWM_TIMER` | 单 LEDC timer 槽 | ESP32 profile-aware router |

---

## 4.2 非易失覆写存储与设备树逃生通道 (`pal_storage` + `wink_dev_config`) — ADR-0008

| target | 实现 | read 缺省语义 |
|---|---|---|
| host | 进程内内存单槽 | 空 $\rightarrow$ `WINK_ERR_EMPTY` |
| esp32 | NVS (`"wink"` / `"dtcfg"`) | key 不存在 $\rightarrow$ `WINK_ERR_EMPTY` |
| wasm | no-op stub | 恒返 `WINK_ERR_UNSUPPORTED` |

---

## 5. DAL ↔ PAL 契约与外设依赖矩阵

### 外设分组

#### 1. 简易数字与高精度时序类外设
- **依赖的 PAL 接口**: `pal_gpio` + `pal_delay`.
- **典型外设实例**: HC-SR04, DHT11, VS1838B.

#### 2. 模拟采集与动力执行类外设
- **依赖的 PAL 接口**: `pal_pwm` + `pal_adc`.
- **典型外设实例**: SG90, L298N DC Motors, Potentiometers.

#### 3. 同步串行总线协议类外设 (I2C/SPI)
- **依赖的 PAL 接口**: `pal_i2c` / `pal_spi`.
- **典型外设实例**: MPU6050, BMP280, SSD1306 OLED.

#### 4. 异步串行通信与智能模组类外设 (UART)
- **依赖的 PAL 接口**: `pal_uart`.
- **典型外设实例**: GPS, AT Modems.

#### 5. 系统支撑与非易失性安全类外设
- **依赖的 PAL 接口**: `pal_wdt` + `pal_flash`.
- **典型外设实例**: WDT, Flash Storage.

### 移植解耦设计要点
1. **按需分步移植**: Bring-up starts with GPIO & Delay.
2. **实例复用与引脚隔离**: DAL purely consumes logical pins.

---

## 6. WINK_BLOCKING 警告抑制与阻塞区域宏契约（ADR-0025）

### 6.1 编译期警告抑制宏规范
1. `WINK_INTERNAL_BLOCKING_REGION_BEGIN / END`: Internal BAL task loops.
2. `WINK_INIT_BLOCKING_REGION_BEGIN / END`: Diagnostic synchronous boot init.

### 6.2 跨编译器底层展开原理
- **GCC / Clang**:
  ```c
  #define WINK_INTERNAL_BLOCKING_REGION_BEGIN \
      _Pragma("GCC diagnostic push") \
      _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
  #define WINK_INTERNAL_BLOCKING_REGION_END  _Pragma("GCC diagnostic pop")
  ```
- **MSVC**:
  ```c
  #define WINK_INTERNAL_BLOCKING_REGION_BEGIN  __pragma(warning(push)) __pragma(warning(disable:4996))
  #define WINK_INTERNAL_BLOCKING_REGION_END    __pragma(warning(pop))
  ```
