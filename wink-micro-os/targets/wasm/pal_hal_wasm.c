/**
 * @file pal_hal_wasm.c
 * @brief Wasm 仿真端 PAL HAL 适配（GPIO / PWM / I2C / pulse_in / debug_printf）。
 *        仅 HAL；IRQ 见 pal_irq_wasm.c；OSAL 见 pal_osal_wasm.c；entry 见 wasm_entry.c；
 *        JS 契约见 wasm_bridge.h。
 *
 * 物理退化中间件（ADR-0009 Wave 2 Task 3）：
 *   pal_gpio_read  → 边界检查 + 抖动状态机（per-pin ctx，bounce_us=0 时旁路）
 *   pal_i2c_transfer → PRNG 驱动确定性丢包（drop_permil=0 时旁路）
 *   故障配置全部位于 pal_wasm_physical.c，通过 pal_wasm_get_* 内部 helper 读取；
 *   零退化时只多一次内存读，热路径开销可忽略。
 */
#include "pal_hal.h"
#include "hal/pal_i2c.h"
#include "pal_pwm_router.h"
#include "pal_osal.h"
#include "pal_resource.h"  /* pal_resource_is_claimed / PAL_RESOURCE_GPIO_PIN — 与 host/esp32 同源保真 */
#include "wasm_bridge.h"
#include "pal_wasm_internal.h"
#include "wink_sim_physical.h"
#include "devices/wasm_sim_registry.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

/* P2-4: wasm64 迁移门控 —— pal_wasm_i2c_transfer 对 wbufPtr/rbufPtr 做
 * (uint32_t)(uintptr_t) 截断，js_pal_gpio_write/read、js_pal_pwm_set_duty 等
 * ABI 契约也依赖 32-bit 指针；开启 wasm64 编译时此 _Static_assert 会立刻红。
 * 迁移时需同步：(1) wasm_bridge.h ABI 契约 #5 更新为 64-bit 指针 ABI；
 *              (2) JS 侧 writeU32LE → writeU64LE，BigInt 化；
 *              (3) 所有 (uint32_t)(uintptr_t) 截断改为全宽度。 */
_Static_assert(sizeof(void*) == 4,
    "wasm64 migration required: see wasm_bridge.h ABI 契约 #5 "
    "and review every (uint32_t)(uintptr_t) cast in pal_hal_wasm.c / createUnisimImports.ts");

static pal_gpio_mode_t s_gpio_mode[WASM_SIM_MAX_PINS];
static bool            s_gpio_mode_known[WASM_SIM_MAX_PINS];

#define PIN_EVENT_QUEUE_SIZE 8

typedef struct {
    uint64_t virtual_time_us;
    uint8_t level;
} wasm_pin_event_t;

static wasm_pin_event_t s_pin_events[WASM_SIM_MAX_PINS][PIN_EVENT_QUEUE_SIZE];
static uint8_t s_pin_event_count[WASM_SIM_MAX_PINS] = {0};

void wasm_sim_pin_events_reset(void) {
    memset(s_pin_event_count, 0, sizeof(s_pin_event_count));
    memset(s_pin_events, 0, sizeof(s_pin_events));
}

static bool pal_gpio_mode_idle_level(pal_gpio_mode_t mode)
{
    switch (mode) {
        case PAL_GPIO_INPUT_PULLUP:
            return true;
        case PAL_GPIO_INPUT_PULLDOWN:
            return false;
        default:
            return false;
    }
}

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode) {
    if (pin >= 0 && pin < WASM_SIM_MAX_PINS) {
        s_gpio_mode[(uint8_t)pin] = mode;
        s_gpio_mode_known[(uint8_t)pin] = true;
        /* P1 electrical SSOT: INPUT* must drop MCU SUPPLY driver so pull/plugin
         * can take the bus (open-drain / 1-wire style handoff). */
        if (mode == PAL_GPIO_INPUT
            || mode == PAL_GPIO_INPUT_PULLUP
            || mode == PAL_GPIO_INPUT_PULLDOWN) {
            js_pal_gpio_release_mcu((uint16_t)pin);
        }
    }
    return WINK_OK;
}

void pal_gpio_reset_pin(wink_pin_t pin) {
    if (pin >= 0 && pin < WASM_SIM_MAX_PINS) {
        s_gpio_mode_known[(uint8_t)pin] = false;
    }
}

wink_status_t pal_gpio_set_direction(wink_pin_t pin, pal_gpio_mode_t mode) {
    /* Wasm simulation: pins are always read-write; direction change is a no-op. */
    (void)pin; (void)mode;
    return WINK_OK;
}

wink_status_t pal_gpio_write(wink_pin_t pin, bool level) {
    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }
    wasm_sim_gpio_write((uint8_t)pin, level);
    js_pal_gpio_write((uint32_t)pin, level);
    js_pal_gpio_on_write((uint8_t)pin, level ? 1 : 0);
    return WINK_OK;
}

wink_status_t pal_gpio_read(wink_pin_t pin, bool *out_level) {
    if (out_level == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *out_level = false; /* Defense-in-depth initialization */

    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) {
        return WINK_ERR_INVALID_ARG;
    }

    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }

    /* Step 1: Arbiter-only electrical SSOT (P3).
     * HIGH|CONFLICT → true; LOW → false; HiZ → pull idle / DISCONNECTED /
     * mode-unknown LOW (constraint 14). C input shadow is not consulted. */
    bool ideal;
    uint8_t st = js_pal_gpio_read_state((uint16_t)pin);
    if (st == JS_GPIO_STATE_HIGH || st == JS_GPIO_STATE_CONFLICT) {
        ideal = true;
    } else if (st == JS_GPIO_STATE_LOW) {
        ideal = false;
    } else {
        /* HiZ (or unknown encoding treated as floating) */
        if (!s_gpio_mode_known[(uint8_t)pin]) {
            /* Constraint 14: mode unknown + HiZ → LOW (not DISCONNECTED). */
            ideal = false;
        } else if (s_gpio_mode[(uint8_t)pin] == PAL_GPIO_INPUT) {
            return WINK_ERR_DISCONNECTED;
        } else {
            ideal = pal_gpio_mode_idle_level(s_gpio_mode[(uint8_t)pin]);
        }
    }

    /* Step 2: 退化中间件（仅当 bounce_us > 0 时生效）。
     * bounce_us=0 是默认零退化路径，热路径只多一次内存读 + 一次比较。 */
    uint32_t bounce_us = pal_wasm_get_bounce_us();
    if (bounce_us > 0u) {
        wink_phys_debounce_ctx_t *ctx = pal_wasm_get_debounce_ctx(pin);
        /* ctx 不可能为 NULL（pin 已过边界检查），但仍做防御式判断——
         * 假设 WASM_SIM_MAX_PINS 未来在两处不同步，至少不会崩。 */
        if (ctx != NULL) {
            /* Task 8 故障审计：在进入抖动窗口的瞬间（in_bounce false→true）
             * 记录一条审计事件。每次抖动 episode 只记录一次，避免把环形日
             * 志被采样周期内的多次同 pin 调用刷爆。CI 侧由 sequence 与
             * timestamp 区分独立的 bounce 触发。 */
            bool was_in_bounce = ctx->in_bounce;
            bool result = wink_phys_debounce_step(ctx, ideal, pal_os_get_us(), bounce_us);
            if (!was_in_bounce && ctx->in_bounce) {
                pal_wasm_log_fault(FAULT_TYPE_GPIO_BOUNCE, pin);
            }
            *out_level = result;
            return WINK_OK;
        }
    }

    /* 无退化 → 原样返回（兼容路径）。 */
    *out_level = ideal;
    return WINK_OK;
}

wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz) {
    pal_pwm_config_t cfg = { .freq_hz = frequency_hz };
    return pal_pwm_init_ex(channel, &cfg);
}

wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg) {
    if (cfg == NULL || cfg->freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->clock_requirement == PAL_PWM_CLOCK_STABLE_REQUIRED) {
        return WINK_ERR_UNSUPPORTED;
    }

    uint8_t bits = cfg->resolution_bits ? cfg->resolution_bits : 13u;
    if (bits == 0u || bits > 20u) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_pwm_timer_profile_t prof = {
        .freq_hz = cfg->freq_hz,
        .resolution_bits = bits,
        .clock_source = PAL_PWM_EFF_CLK_PLATFORM_AUTO,
    };
    uint8_t timer_num = 0;
    return pal_pwm_router_acquire(channel, &prof, &timer_num);
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    if (wasm_sim_pwm_channel_exists(channel)) {
        wasm_sim_pwm_set_duty(channel, duty_cycle_percent);
    }
    js_pal_pwm_set_duty(channel, duty_cycle_percent);
    return WINK_OK;
}

void pal_pwm_deinit(uint8_t channel) {
    pal_pwm_router_release(channel);   /* no-op if uninitialized */
}

/* P1-P4 (2026-07-04)：pin_map getter。wasm 是纯虚拟外设 target（走 JS 桥，不映射
 * 物理 GPIO），无 pin_map 概念；返回 WINK_ERR_UNSUPPORTED。参数越界仍先返
 * INVALID_ARG（参数校验优先于能力可用性）。*/
wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin) {
    if (out_pin == NULL) { return WINK_ERR_INVALID_ARG; }
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl) {
    if (out_sda == NULL && out_scl == NULL) { return WINK_ERR_INVALID_ARG; }
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    return WINK_ERR_UNSUPPORTED;
}

static bool s_i2c_bus_inited[PAL_I2C_PORTS] = {false};

wink_status_t pal_i2c_bus_init(uint8_t port, uint8_t sda, uint8_t scl, uint32_t hz) {
    (void)sda; (void)scl; (void)hz;
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    s_i2c_bus_inited[port] = true;
    return WINK_OK;
}

void pal_i2c_bus_deinit(uint8_t port) {
    if (port < PAL_I2C_PORTS) {
        s_i2c_bus_inited[port] = false;
    }
}

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len) {
    if (port >= PAL_I2C_PORTS) { return WINK_ERR_INVALID_ARG; }
    if (!s_i2c_bus_inited[port]) {
        printf("WINK_WARN: I2C port %d transfer called before bus init, lazy initializing (deprecated path)\n", port);
        s_i2c_bus_inited[port] = true;
    }
    /* Step 1: 丢包判定（PRNG 确定性，§4.1 合规）。
     *
     * 设计说明：全局 PRNG 是有意的设计选择，保证"单种子复现全系统行为"。
     * 如果 I2C 丢包和 ADC 噪声独立 PRNG，那么改变 ADC 采样率不会影响
     * I2C 序列，但这也失去了"一个 seed = 整个系统的完整快照"的能力。
     * 当前选择：全局 PRNG，简化确定性复现（详见 pal_wasm_physical.c）。
     *
     * drop_permil=0 是零退化默认路径，热路径只多一次内存读 + 一次比较。 */
    uint16_t drop_permil = pal_wasm_get_i2c_drop_permil();
    if (drop_permil > 0u) {
        uint32_t prng_state = pal_wasm_get_prng_state();
        bool should_drop = wink_phys_bus_drop(drop_permil, &prng_state);
        pal_wasm_advance_prng_state(prng_state);  /* 回写推进后的状态 */
        if (should_drop) {
            /* Task 8 故障审计：丢包瞬间记录审计事件，pin_or_bus 字段
             * 复用为 I2C port，便于 CI 区分多总线场景。 */
            pal_wasm_log_fault(FAULT_TYPE_I2C_DROP, port);
            return WINK_ERR_IO;  /* 模拟总线故障，驱动超时退回机制触发 */
        }
    }

    // Phase 4 T5: C-side I2C Scheme-A short-circuit retired (SSD1306 → Unisim plugin).
    // Keep the exists/transfer hooks for future C virtual devices; currently always miss.
    if (wasm_sim_i2c_dev_exists(dev_addr)) {
        return wasm_sim_i2c_dev_transfer(port, dev_addr, write_buf, write_len, read_buf, read_len);
    }

    /* Step 2: 正常传输（无退化路径，Fallback 走 JS 侧）。 */
    return js_pal_i2c_transfer(port, dev_addr, write_buf, write_len, read_buf, read_len)
           ? WINK_OK : WINK_ERR_IO;
}

wink_status_t pal_i2c_scan(uint8_t port, uint8_t start_addr, uint8_t end_addr,
                            uint8_t *out_found_bitmap, size_t bitmap_bytes) {
    if (out_found_bitmap == NULL || bitmap_bytes < 16) {
        return WINK_ERR_INVALID_ARG;
    }
    if (port >= PAL_I2C_PORTS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_i2c_bus_inited[port]) {
        printf("WINK_WARN: I2C port %d scan called before bus init, lazy initializing (deprecated path)\n", port);
        s_i2c_bus_inited[port] = true;
    }
    if (start_addr > end_addr || end_addr > 0x7F) {
        return WINK_ERR_INVALID_ARG;
    }
    /* Clamp to valid 7-bit range (0x03..0x77 per I2C spec). */
    uint8_t lo = start_addr < 0x03 ? 0x03 : start_addr;
    uint8_t hi = end_addr   > 0x77 ? 0x77 : end_addr;
    /* Zero bitmap then set bits for addresses the C-side registry reports present.
     * JS-side virtual devices would require a js_pal_i2c_probe() bridge; for
     * v1 we report only C-side simulated devices (matches current usage by
     * selftest in SIMULATION runs). */
    memset(out_found_bitmap, 0, 16);
    for (uint16_t addr = lo; addr <= hi; addr++) {
        if (wasm_sim_i2c_dev_exists((uint8_t)addr)) {
            uint8_t byte_idx = (uint8_t)(addr >> 3);
            uint8_t bit_idx  = (uint8_t)(addr & 0x7);
            out_found_bitmap[byte_idx] |= (uint8_t)(1u << bit_idx);
        }
    }
    return WINK_OK;
}

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL) { return WINK_ERR_INVALID_ARG; }
    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) { return WINK_ERR_INVALID_ARG; }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) { return WINK_ERR_INVALID_STATE; }
    *pulse_us = 0;
    (void)timeout_us;

    // 1. 优先从虚拟引脚未来事件队列中匹配高低电平跳变并进行同步时钟快进
    uint8_t count = s_pin_event_count[(uint8_t)pin];
    if (count > 0) {
        int start_idx = -1;
        int end_idx = -1;
        for (int i = 0; i < count; i++) {
            if (s_pin_events[(uint8_t)pin][i].level == (level ? 1 : 0)) {
                start_idx = i;
                for (int j = i + 1; j < count; j++) {
                    if (s_pin_events[(uint8_t)pin][j].level != (level ? 1 : 0)) {
                        end_idx = j;
                        break;
                    }
                }
                break;
            }
        }
        if (start_idx != -1 && end_idx != -1) {
            uint64_t t_start = s_pin_events[(uint8_t)pin][start_idx].virtual_time_us;
            uint64_t t_end = s_pin_events[(uint8_t)pin][end_idx].virtual_time_us;
            if (t_end > t_start) {
                uint64_t duration = t_end - t_start;
                uint64_t current_time = pal_os_get_us();
                if (t_end > current_time) {
                    pal_wasm_advance_virtual_clock(t_end - current_time);
                }
                *pulse_us = (uint32_t)duration;
                s_pin_event_count[(uint8_t)pin] = 0; // 消费后清空队列
                return WINK_OK;
            }
        }
    }

    // 2. 次优先调用 C 侧虚拟超声波模拟获取脉宽作为 Fallback
    uint32_t v = wasm_dev_ultrasonic_get_pulse_us((uint8_t)pin);
    if (v > 0) {
        *pulse_us = v;
        return WINK_OK;
    }

    return WINK_ERR_TIMEOUT;
}

EMSCRIPTEN_KEEPALIVE void pal_wasm_push_pin_event(uint8_t pin, uint64_t delay_us, uint8_t level) {
    WASM_FAULT_GUARD_VOID();
    if (pin >= WASM_SIM_MAX_PINS) {
        return;
    }
    uint8_t count = s_pin_event_count[pin];
    if (count >= PIN_EVENT_QUEUE_SIZE) {
        // 队列满时覆盖最老事件
        for (int i = 1; i < PIN_EVENT_QUEUE_SIZE; i++) {
            s_pin_events[pin][i - 1] = s_pin_events[pin][i];
        }
        count = PIN_EVENT_QUEUE_SIZE - 1;
    }
    s_pin_events[pin][count].virtual_time_us = pal_os_get_us() + delay_us;
    s_pin_events[pin][count].level = level;
    s_pin_event_count[pin] = count + 1;
}

wink_status_t pal_test_enable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    (void)pin_out; (void)pin_in;
    /* wasm 单线程协同调度模型下，无真实硬件环回能力——若返 WINK_OK，selftest 会
     * 软件翻转后等 ISR 触发，结果 ctx.fired==0 被误判为"PASS (注册成功)"。
     * 诚实返 UNSUPPORTED 让 selftest 跳过翻转段，note 明确写明需物理信号。*/
    return WINK_ERR_UNSUPPORTED;
}

wink_status_t pal_test_disable_hardware_loopback(wink_pin_t pin_out, wink_pin_t pin_in) {
    (void)pin_out; (void)pin_in;
    /* No loopback is ever enabled on wasm; keep disable idempotent and honest. */
    return WINK_ERR_UNSUPPORTED;
}
