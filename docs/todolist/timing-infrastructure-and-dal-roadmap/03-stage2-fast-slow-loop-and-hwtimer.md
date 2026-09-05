# 阶段 2 计划：快慢环调度隔离、ADR-0047 pal_hwtimer、MCPWM 互补死区与无锁管道

| 元数据项 | 说明 |
| :--- | :--- |
| **阶段编号** | STAGE-2-FAST-SLOW-LOOP |
| **所属模块** | `wink-micro-os/pal/include/hal/`（`pal_hwtimer.h`、`pal_mcpwm.h`、`pal_atomic.h`）、`wink-micro-os/targets/{esp32,host,wasm}/`、BAL 控制环 |
| **解决时序类别** | Class 7（多轴相位对齐与死区互补）/ Class 8（20 kHz 硬实时闭环互锁） |
| **依赖 ADR** | [ADR-0007 协作循环](../../decisions/core/0007-cooperative-loop-execution-model.md)、[ADR-0017 阻塞硬隔离](../../decisions/core/0017-blocking-api-hard-isolation.md)、[ADR-0047 FOC ISR 分层与 pal_hwtimer](../../decisions/core/0047-foc-isr-layering-and-pal-hwtimer.md) |
| **前置阶段** | [Stage 0](01-stage0-pal-hardware-acceleration-engines.md)、[Stage 1](02-stage1-timing-safety-and-critical-section.md) |
| **基线** | 树内**无任何** `pal_hwtimer` / `pal_mcpwm` 符号；ADR-0047 C2 未完成。可移植原子仅存在 ESP32 私有的 `targets/esp32/pal_atomic_esp32.h`（u32 inc/dec/load） |
| **状态** | **Ready for Implementation** |

---

## 1. 阶段目标

1. 落地 ADR-0047 `pal_hwtimer`：微秒级硬件定时器快环，支持绑核、IRAM-safe 回调、周期/oneshot。
2. 新增 `pal_mcpwm`：互补通道对、死区 generator、外部 brake/fault 输入、捕获；与 `pal_pwm_router`（LEDC 低速）划清资源边界。
3. 建立 PWM–ADC TRGO 硬件级联（电流采样对齐 PWM 顶点/谷点，零软件抖动）。
4. 快慢环无锁通信：可移植 `pal_atomic.h` + `__atomic_load/store_n` acquire/release 双缓冲；数值类型按 ADR-0047 优先 Q15/Q31 定点，若用 float 必须显式处理 Xtensa FPU 上下文。
5. 两类 ISR 分级：周期控制 ISR（高优先级、IRAM、低抖动）vs nFAULT 保护 ISR（最高优先级、最短路径、独立栈）。

---

## 2. 任务清单

### T2.1 可移植 `pal_atomic.h`（先行）

**现状**：ESP32 私有 `pal_atomic_esp32.h` 提供 u32 load/add/xchg；host/wasm 无对应头。Stage 2 无锁管道、Stage 0 PCNT 64 位累加、Stage 3 软中断都依赖。

**交付**：新建 `wink-micro-os/pal/include/osal/pal_atomic.h`

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

#if defined(ESP_PLATFORM)
  /* 转发到已有实现；扩展 64 位与 acquire/release 包装 */
  #include "pal_atomic_esp32.h"
  #define PAL_ATOMIC_LOAD(ptr, ord)      __atomic_load_n((ptr), ord)
  #define PAL_ATOMIC_STORE(ptr, v, ord)  __atomic_store_n((ptr), v, ord)
  #define PAL_ATOMIC_ADD(ptr, v, ord)    __atomic_fetch_add((ptr), v, ord)
  #define PAL_ATOMIC_XCHG(ptr, v, ord)   __atomic_exchange_n((ptr), v, ord)
#else
  #include <stdatomic.h>
  #define PAL_ATOMIC_LOAD(ptr, ord)      atomic_load_explicit((_Atomic __typeof__(*(ptr))*)(ptr), ord)
  #define PAL_ATOMIC_STORE(ptr, v, ord)  atomic_store_explicit((_Atomic __typeof__(*(ptr))*)(ptr), v, ord)
  #define PAL_ATOMIC_ADD(ptr, v, ord)    atomic_fetch_add_explicit((_Atomic __typeof__(*(ptr))*)(ptr), v, ord)
  #define PAL_ATOMIC_XCHG(ptr, v, ord)   atomic_exchange_explicit((_Atomic __typeof__(*(ptr))*)(ptr), v, ord)
#endif

#define PAL_ACQ      __ATOMIC_ACQUIRE
#define PAL_REL      __ATOMIC_RELEASE
#define PAL_ACQ_REL  __ATOMIC_ACQ_REL
#define PAL_RLX      __ATOMIC_RELAXED
```

- ESP32 侧扩展 64 位变体（`uint64_t` 用 `__atomic_load_n` / `__atomic_store_n`；Xtensa 工具链内建）。
- **不**引入 C11 `_Atomic` 到 ESP32 公共路径（保持 GCC `__atomic` 风格一致）；host/wasm 用 `<stdatomic.h>`。
- 验收：`test/unit/pal/test_pal_atomic.c` 三 target 跑通 acquire/release 配对、64 位 wrap-around、ISR 上下文 add。

### T2.2 `pal_hwtimer.h`（ADR-0047 C2）

**API**：

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "pal_osal.h"   /* pal_os_core_id_t */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAL_HWTIMERS_MAX
#define PAL_HWTIMERS_MAX 4
#endif

/**
 * @brief 硬件定时器快环回调。
 *
 * ADR-0047 ABI：
 *  - 必须 PAL_ISR 链入 IRAM；
 *  - 严禁访问 SPI Flash / 阻塞 / log / malloc；
 *  - 数据与常量 PAL_IRAM_DATA / PAL_IRAM_RODATA；
 *  - 数值类型 Q15/Q31 优先；若用 float，注册时 uses_fpu=true，
 *    target 负责保存/恢复 Xtensa FPU 上下文（额外 ~100 cycles/ISR）。
 *
 * FreeRTOS API 必须 FromISR 后缀。
 */
typedef void (*pal_hwtimer_isr_t)(void *arg);

typedef struct {
    uint8_t            timer_id;      /* 0..PAL_HWTIMERS_MAX-1; pal_resource 仲裁 */
    uint32_t           period_us;
    bool               oneshot;
    bool               auto_start;
    pal_os_core_id_t   core_affinity; /* ADR-0007：快环默认 Core 1 */
    uint8_t            isr_priority;  /* pal_irq.h 3-tier */
    bool               uses_fpu;
    pal_hwtimer_isr_t  callback;
    void              *callback_arg;
} pal_hwtimer_cfg_t;

wink_status_t pal_hwtimer_init(const pal_hwtimer_cfg_t *cfg);
wink_status_t pal_hwtimer_start(uint8_t timer_id);
wink_status_t pal_hwtimer_stop(uint8_t timer_id);
wink_status_t pal_hwtimer_change_period(uint8_t timer_id, uint32_t new_period_us);
void           pal_hwtimer_deinit(uint8_t timer_id);

/* 仿真软步进；真机返回 WINK_ERR_UNSUPPORTED。
 * wasm 由 pal_wasm_advance_virtual_clock 在每个推进步长结束时调用。 */
wink_status_t pal_hwtimer_fire_soft(uint8_t timer_id);

#ifdef __cplusplus
}
#endif
```

**ESP32 实现（IDF 5.4+ `gptimer`）**：
- 基于 `gptimer_new_timer` + `gptimer_register_event_callbacks`（Stage 0 T0.7 已纳入 `esp_driver_gptimer`，IDF ≥ 5.4 硬门控，勘误 E-002）；
- 回调 `PAL_ISR`；
- **绑核契约（ADR-0047 / ADR-0007 Core 1 默认）**：`gptimer_new_timer_config_t` 在 IDF 5.4 中没有直接的 `core_id` 字段；ISR 亲和性必须通过以下机制之一显式控制：
  1. **首选**：在目标核创建一个 pined one-shot task（`xTaskCreatePinnedToCore(..., core_affinity, ...)`），在该 task 内调用 `gptimer_enable/start`。IDF 5.4 `gptimer_enable` 在调用任务所在核分配中断，ISR 即绑定到该核；start 后 task 可自删。
  2. 在 `gptimer_register_event_callbacks` 后通过 `esp_intr_get_affinity` 查询，若与 `core_affinity` 不符，用 `esp_intr_set_affinity(intr_handle, 1 << core_affinity)` 显式迁移（IDF ≥ 5.1 支持）。
- 实现 PR 必须附带 `xPortGetCoreID()` 在 ISR 内读取的真机日志/测试结果作为证据，不能只靠代码审查。
- `uses_fpu=true` 时 trampoline 前后用 Xtensa `frsave`/`frrestor` 宏保存/恢复 CP0~CP15 寄存器（额外 ~100 cycles/ISR）；code review 确认。
- **TWDT 交互（评审 M3）**：20 kHz ISR 本身不喂狗，但被 ISR 长时间抢占的慢环任务可能触发 Task WDT。策略：
  - 若 FOC 慢环跑在独立 pined task，从 TWDT 监控中 `esp_task_wdt_delete(NULL)` 排除，并在注释中说明"快环健康度由 Stage 2 集成测试周期计数自检"；
  - 或在慢环 task 中定期 `esp_task_wdt_reset()`，周期 < TWDT timeout（默认 5 s）；
  - 严禁在 ISR 中调 `esp_task_wdt_reset`（TWDT API 非 FromISR）。

**`pal_hwtimer_fire_soft` 构建范围**：
- 真机非测试 build：实现为 `return WINK_ERR_UNSUPPORTED;`（不进 IRAM，不占 Flash 之外的内存）；
- Host + Wasm + 真机 `WINK_BUILD_TESTS` 编译：提供真实软步进路径，供单元测试注入虚拟边沿；
- 头文件 Doxygen 明确该函数仅用于测试/仿真。

**Host/Wasm 实现**：
- Host：维护 `next_fire_virtual_us`，由测试主循环 `pal_hwtimer_drain(virtual_now_us)` 手动驱动；
- Wasm：由 `wink_vclock_advance_internal`（`pal_osal_wasm.c:40`）扫描到期 timer 并调用 `pal_hwtimer_fire_soft`；对齐 ADR-0047 "仿真软步进"。

**验收**：
- ESP32 20 kHz（50 µs）× 1,000,000 次中断：相邻间隔抖动 < 500 ns（`xthal_get_ccount` 测），单次 ISR < 15 µs；
- 后台线程循环擦 NVS/Flash（cache 禁用）时不崩不丢周期；
- ISR 内 `xPortGetCoreID()` 与 `core_affinity` 一致；
- Host/Wasm 确定性：相同种子两次回放快环序列完全一致。

### T2.3 `pal_mcpwm.h`：互补死区 + brake + 捕获

**与 `pal_pwm_router` 的边界**：

| 能力 | 走谁 |
|---|---|
| 单色 LED / 蜂鸣器 / 舵机（< 4 kHz，无死区/互补） | `pal_pwm_router`（LEDC） |
| H 桥半桥互补对、dead-time、外部 fault 刹车、多相相位对齐 | `pal_mcpwm`（MCPWM 单元） |

DAL（`dal_dc_motor`、`dal_stepper`、BAL FOC）按上表静态选择，无运行时虚分派。

**API（`pal/include/hal/pal_mcpwm.h`）**：

```c
typedef struct pal_mcpwm_timer_s    *pal_mcpwm_timer_handle_t;
typedef struct pal_mcpwm_operator_s *pal_mcpwm_oper_handle_t;
typedef struct pal_mcpwm_cmp_s     *pal_mcpwm_cmp_handle_t;
typedef struct pal_mcpwm_fault_s   *pal_mcpwm_fault_handle_t;
typedef struct pal_mcpwm_cap_s     *pal_mcpwm_cap_handle_t;

typedef struct {
    uint8_t  mcpwm_unit;        /* 0/1 on ESP32 classic */
    uint8_t  timer_id;          /* 0..2 */
    uint32_t pwm_freq_hz;       /* 典型 20 kHz for FOC */
    uint16_t counter_top;       /* 计数峰值 */
    pal_os_core_id_t core_affinity;
    bool     iram_safe;
} pal_mcpwm_timer_cfg_t;

typedef struct {
    pal_mcpwm_timer_handle_t timer;
    uint8_t     operator_id;   /* 0..2 */
    wink_pin_t  pin_pwm_a;
    wink_pin_t  pin_pwm_b;     /* 互补；不需要传 -1 */
    uint16_t    deadtime_red_ticks;
    uint16_t    deadtime_fed_ticks;
    bool        complementary_enable;
} pal_mcpwm_oper_cfg_t;

typedef struct {
    pal_mcpwm_oper_handle_t oper;
    uint32_t initial_duty_ticks;
} pal_mcpwm_cmp_cfg_t;

typedef struct {
    uint8_t     fault_id;      /* 0..2 */
    wink_pin_t  fault_pin;
    bool        active_level;
    bool        async_brake;   /* 硬件异步制动：不经 CPU */
    bool        safe_level_a;
    bool        safe_level_b;
    void      (*on_brake_isr)(void *arg);  /* 可选 ISR 通知；PAL_ISR */
    void       *on_brake_arg;
} pal_mcpwm_fault_cfg_t;

typedef struct {
    wink_pin_t  cap_pin;
    uint8_t     cap_channel;
    bool        pull_up;
    void      (*on_capture_isr)(void *arg, uint32_t ts_ns, bool rising);
    void       *on_capture_arg;
} pal_mcpwm_cap_cfg_t;

wink_status_t pal_mcpwm_new_timer(const pal_mcpwm_timer_cfg_t *cfg, pal_mcpwm_timer_handle_t *out);
wink_status_t pal_mcpwm_new_oper(const pal_mcpwm_oper_cfg_t *cfg, pal_mcpwm_oper_handle_t *out);
wink_status_t pal_mcpwm_new_cmp(const pal_mcpwm_cmp_cfg_t *cfg, pal_mcpwm_cmp_handle_t *out);
wink_status_t pal_mcpwm_new_fault(const pal_mcpwm_fault_cfg_t *cfg, pal_mcpwm_fault_handle_t *out);
wink_status_t pal_mcpwm_new_capture(const pal_mcpwm_cap_cfg_t *cfg, pal_mcpwm_cap_handle_t *out);

wink_status_t pal_mcpwm_timer_start(pal_mcpwm_timer_handle_t t);
wink_status_t pal_mcpwm_timer_stop(pal_mcpwm_timer_handle_t t);

/* ISR 上下文安全；IRAM 安全 */
wink_status_t pal_mcpwm_set_duty_ticks(pal_mcpwm_cmp_handle_t cmp, uint32_t duty_ticks);

/* 相位对齐：多 timer 同步到 GPIO 或软触发 sync 源。
 *
 * sync_gpio 是全局资源（ESP32 MCPWM 有 0..2 三个 sync input，GPIO 路由有限），
 * 首次配置调用 pal_resource_claim(PAL_RESOURCE_MCPWM_SYNC_GPIO, sync_gpio, owner)；
 * 重复 claim 返回 WINK_ERR_BUSY。多电机场景优先使用 pal_mcpwm_trigger_software_sync()，
 * GPIO sync 仅在需要外部硬件主时使用。 */
wink_status_t pal_mcpwm_sync_gpio_config(wink_pin_t sync_gpio, bool active_level);
wink_status_t pal_mcpwm_timer_enable_phase_lock(pal_mcpwm_timer_handle_t t, uint32_t phase_ticks);
wink_status_t pal_mcpwm_trigger_software_sync(void);

wink_status_t pal_mcpwm_fault_clear(pal_mcpwm_fault_handle_t f);
void           pal_mcpwm_del_timer(pal_mcpwm_timer_handle_t t);
```

**实现要点**：
- 基于 ESP-IDF 5.4+ `mcpwm_*` 新驱动（Stage 0 T0.7 已加 REQUIRES）；
- ISR `PAL_ISR`，数据 `PAL_IRAM_DATA`，常量 `PAL_IRAM_RODATA`；
- async brake 走 MCPWM 硬件 fault，不经 CPU（ADR-0047 R-005）；软件回调为可选通知；
- Host/Wasm：维护 duty/freq 状态；capture 通道提供软边沿时间戳供 DAL 单测。

**验收**：
- 互补对 20 kHz 逻辑分析仪：dead time = 配置 ± 50 ns；brake 触发到输出安全电平 < 200 ns（硬件异步路径）；
- 多 timer phase lock 相位偏差 < 1 timer tick；
- 20 kHz 满载 + 反复 brake/clear 1 小时无异常中断。

### T2.4 PWM–ADC TRGO 硬件级联

**目标**：FOC 电流采样在 PWM 顶点/谷点由硬件触发 ADC continuous DMA，零 CPU 中断参与、零软件抖动。

**API（扩展 `pal_adc.h`）**：

```c
typedef enum {
    PAL_ADC_TRIG_SOURCE_SW,        /* 软件/定时器触发；ESP32 classic 唯一支持的连续模式 */
    PAL_ADC_TRIG_SOURCE_MCPWM,     /* MCPWM 事件硬件触发；ESP32-S2/S3 */
} pal_adc_trig_source_t;

typedef enum {
    PAL_ADC_TRIG_AT_PWM_PEAK,
    PAL_ADC_TRIG_AT_PWM_VALLEY,
    PAL_ADC_TRIG_AT_PWM_BOTH,
} pal_adc_trgo_edge_t;

typedef struct {
    pal_adc_trig_source_t source;
    pal_mcpwm_timer_handle_t pwm_timer;  /* source=MCPWM 时必填；SW 时可 NULL */
    uint8_t        adc_unit;
    const uint8_t *channels;
    uint8_t        channel_count;
    pal_adc_trgo_edge_t edge;
    uint16_t       sampling_period_pwm;  /* 每 N 个 PWM 周期采一次 */
    uint16_t      *dma_buf_a;           /* PAL_DMA_BUF_ATTR + 字对齐 */
    uint16_t      *dma_buf_b;           /* PAL_DMA_BUF_ATTR + 字对齐 */
    size_t         samples_per_buf;
    void         (*on_half_full)(void *arg, const uint16_t *buf, size_t n);
    void         (*on_full)(void *arg, const uint16_t *buf, size_t n);
    void          *cb_arg;
} pal_adc_continuous_cfg_t;

wink_status_t pal_adc_continuous_start(const pal_adc_continuous_cfg_t *cfg);
wink_status_t pal_adc_continuous_stop(uint8_t adc_unit);
```

**芯片兼容性（勘误 E-004）**：

| Target | `MCPWM` 触发 | `SW/Timer` 触发 | 说明 |
|---|---|---|---|
| ESP32 classic | ❌ 不支持 | ✅ | ADC continuous 走 I2S0 DMA + 内置 timer；MCPWM 事件无法路由到 ADC DIG 触发源 |
| ESP32-S2 | ✅ | ✅ | `ADC_DIGI_TRIGGER_SOURCE_MCPWM0_TIMER0` 等 |
| ESP32-S3 | ✅ | ✅ | 同 S2，且支持更多触发源 |
| Host / Wasm | — | ✅ 软模型 | 按虚拟 PWM 周期注入 CH3 模拟量 |

- ESP32 classic 实现：`cfg->source` 必须为 `PAL_ADC_TRIG_SOURCE_SW`，否则返回 `WINK_ERR_UNSUPPORTED`（ADR-0012）；用 `adc_continuous_new_handle` + 内置 timer 触发，周期由 `sampling_period_pwm × PWM 周期` 换算；
- S2/S3 实现：`source=MCPWM` 时通过 GPIO Matrix 路由 MCPWM 事件到 ADC DMA；
- CMake 按 `IDF_TARGET` 条件编译选择触发源，`_Static_assert` 校验当前 target 支持的 source；
- **不允许**在 classic 上退化到"GPIO 通用 ISR 触发 ADC"——抖动不可接受，且违反红线 3。classic 若需 FOC 级电流采样，方案是 PWM 频率与 ADC timer 周期对齐（同一定时器时基），软件保证相位而非硬件 TRGO。

**DMA 缓冲对齐**：
- `dma_buf_a/b` 必须 `PAL_DMA_BUF_ATTR`（含 `WORD_ALIGNED_ATTR`，4 字节对齐）；ESP32 classic GDMA 要求对齐，未对齐触发 `ESP_ERR_INVALID_ARG`；
- DMA 描述符同样 `PAL_DMA_ATTR`；
- 内部 RAM only（红线 6：GDMA 不访问 PSRAM）。

- Wasm：ADC 模型已有 RC + 噪声；`source=MCPWM` 在 wasm 接受（软模型按虚拟 PWM 周期采样），确定性回放。

**验收**：
- 20 kHz PWM + 双相电流采样，PWM 谷点到 ADC 采样保持开关延迟 < 1 µs，抖动 < 50 ns；
- DMA 双缓冲 1 h 无 overflow（`adc_continuous_on_pool` conv_required overflow 计数为 0）。

### T2.5 快慢环无锁管道（Q15/Q31 优先）

**旧文档示例问题**：
- 用 `float`（8 字节，Xtensa 非原子读写）；
- 用 `memw`（ESP 专属，host/wasm 为空操作，无 acquire/release 语义）；
- 只写发布侧，未演示 ISR 读侧；
- 与 ADR-0047 "Q15/Q31 优先" 冲突。

**修正：Q15 定点双缓冲 + T2.1 `pal_atomic.h`**

```c
#include "pal_atomic.h"
#include <stdint.h>

typedef int16_t q15_t;

typedef struct {
    q15_t target_speed_q15;
    q15_t target_angle_q15;
} foc_slow_to_fast_cmd_t;

typedef struct {
    q15_t actual_current_q15;
    q15_t actual_velocity_q15;
    uint16_t fault_flags;
} foc_fast_to_slow_status_t;

typedef struct {
    foc_slow_to_fast_cmd_t    cmd_slot[2];
    uint8_t                   cmd_idx;   /* 0/1, 原子发布 */
    foc_fast_to_slow_status_t stat_slot[2];
    uint8_t                   stat_idx;
} foc_pipeline_t;

/* App 慢环发布命令（任务上下文） */
static inline void foc_publish_cmd(foc_pipeline_t *p, const foc_slow_to_fast_cmd_t *cmd) {
    uint8_t w = 1u - PAL_ATOMIC_LOAD(&p->cmd_idx, PAL_ACQ);
    p->cmd_slot[w] = *cmd;
    PAL_ATOMIC_STORE(&p->cmd_idx, w, PAL_REL);
}

/* ISR 快环读取最新命令 */
static inline foc_slow_to_fast_cmd_t foc_consume_cmd(const foc_pipeline_t *p) {
    uint8_t i = PAL_ATOMIC_LOAD(&p->cmd_idx, PAL_ACQ);
    return p->cmd_slot[i];
}

/* ISR 发布状态 */
static inline void foc_publish_status(foc_pipeline_t *p, const foc_fast_to_slow_status_t *st) {
    uint8_t w = 1u - PAL_ATOMIC_LOAD(&p->stat_idx, PAL_ACQ);
    p->stat_slot[w] = *st;
    PAL_ATOMIC_STORE(&p->stat_idx, w, PAL_REL);
}

/* App 慢环读取状态 */
static inline foc_fast_to_slow_status_t foc_consume_status(const foc_pipeline_t *p) {
    uint8_t i = PAL_ATOMIC_LOAD(&p->stat_idx, PAL_ACQ);
    return p->stat_slot[i];
}
```

- 索引字节在 Xtensa/x86/wasm32 天然原子；acquire/release 保证结构体拷贝可见性；
- 不使用 `memw` / `__atomic` on struct；
- **单写者假设（SPSC）**：本双缓冲方案假设发布侧为**单一写者**。该假设由 ADR-0007 协作循环架构保证——慢环为单一协作任务，不存在并发的第二个写者（CLI 调试等旁路须串行化到同一任务）。若未来架构引入多写者，**不能**直接对 `foc_publish_cmd` 加 `pal_spinlock`——那会使无锁管道退化为有锁结构，失去 IRAM 快环 ISR 安全性。正确路径：走 ADR 修订，升级为 MPSC 无锁队列（如 lock-free ring buffer）或 seqlock，不在本结构上打补丁；
- 若 BAL 必须 float：`pal_hwtimer_cfg_t::uses_fpu=true`，trampoline 保存/恢复 FPU 上下文，并在设计文档中明确接受 ~100 cycles/ISR 额外开销。**不得**在未声明的快环中使用 float。

**验收**：
- 多核 + 20 kHz ISR 压力 1 h，数据无撕裂（pattern 0xAA55/0x55AA 校验）；
- Host TSan 零数据竞争；
- Wasm 确定性回放两次字节一致。

### T2.6 ISR 两级分级（ADR-0047 R-005/R-009）

| 级别 | 用途 | 优先级 | 栈 | 允许操作 |
|---|---|---|---|---|
| **周期控制 ISR** | `pal_hwtimer` 20 kHz FOC、ADC DMA 半满/全满 | 中（2） | 系统 ISR 栈 | 定点数学、写比较寄存器、FromISR 队列 |
| **nFAULT 保护 ISR** | `pal_mcpwm` 硬件 brake、过流过压、紧急停机 | 最高（3） | 独立小栈（256 B IRAM） | 仅写故障寄存器；其余推迟任务 |

- 在 `pal_irq.h` 既有 3-tier 模型上为 3 级 ISR 声明独立 IRAM 栈；
- nFAULT ISR 不调用 FreeRTOS API（除 `portYIELD_FROM_ISR`），不记日志；
- 验收：外部拉低 nFAULT 到输出安全电平 < 1 µs；nFAULT ISR 执行时间 < 2 µs。

---

## 3. 验收门槛（进入 Stage 3 前置）

- [ ] `pal_atomic.h` 三 target 单测通过；
- [ ] `pal_hwtimer` 20 kHz 抖动 / Flash 并发 / 绑核 / 确定性四项达标；
- [ ] `pal_mcpwm` 死区 / 制动 / 相位锁定量化达标；
- [ ] PWM–ADC TRGO 延迟与抖动达标；
- [ ] 无锁管道 TSan 干净；快环无未声明 float；
- [ ] `python wink-tools/wink.py lint --pack layering --pack api` 零错误；
- [ ] ADR-0047 C2/C3 follow-up 勾选并回写 ADR。
