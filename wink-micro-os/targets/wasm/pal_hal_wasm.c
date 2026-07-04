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
#include "pal_pwm_router.h"
#include "pal_osal.h"
#include "pal_resource.h"  /* pal_resource_is_claimed / PAL_RESOURCE_GPIO_PIN — 与 host/esp32 同源保真 */
#include "pal_debug.h"
#include "wasm_bridge.h"
#include "pal_wasm_internal.h"
#include "wink_sim_physical.h"
#include <stdarg.h>
#include <stdio.h>

/* P2-4: wasm64 迁移门控 —— pal_wasm_i2c_transfer 对 wbufPtr/rbufPtr 做
 * (uint32_t)(uintptr_t) 截断，js_pal_gpio_write/read、js_pal_pwm_set_duty 等
 * ABI 契约也依赖 32-bit 指针；开启 wasm64 编译时此 _Static_assert 会立刻红。
 * 迁移时需同步：(1) wasm_bridge.h ABI 契约 #5 更新为 64-bit 指针 ABI；
 *              (2) JS 侧 writeU32LE → writeU64LE，BigInt 化；
 *              (3) 所有 (uint32_t)(uintptr_t) 截断改为全宽度。 */
_Static_assert(sizeof(void*) == 4,
    "wasm64 migration required: see wasm_bridge.h ABI 契约 #5 "
    "and review every (uint32_t)(uintptr_t) cast in pal_hal_wasm.c / createUnisimImports.ts");

wink_status_t pal_gpio_init(wink_pin_t pin, pal_gpio_mode_t mode) {
    (void)pin; (void)mode;            /* 仿真下无需硬件配置 */
    return WINK_OK;
}

wink_status_t pal_gpio_write(wink_pin_t pin, bool level) {
    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }
    js_pal_gpio_write((uint32_t)pin, level);
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

    /* Step 1: 从 JS 侧获取理想电平（UniSim 宏观物理状态）。 */
    bool ideal = js_pal_gpio_read(pin);

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
    uint8_t timer_num = 0;
    /* wasm 无资源表/硬件，但 router 提供通道/频率校验与槽位记账，保持与 host/esp32 一致。*/
    return pal_pwm_router_acquire(channel, frequency_hz, &timer_num);
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    js_pal_pwm_set_duty(channel, duty_cycle_percent);
    return WINK_OK;
}

void pal_pwm_deinit(uint8_t channel) {
    pal_pwm_router_release(channel);   /* no-op if uninitialized */
}

wink_status_t pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                      const uint8_t *write_buf, uint32_t write_len,
                      uint8_t *read_buf, uint32_t read_len) {
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

    /* Step 2: 正常传输（无退化路径）。 */
    return js_pal_i2c_transfer(port, dev_addr, write_buf, write_len, read_buf, read_len)
           ? WINK_OK : WINK_ERR_IO;
}

wink_status_t pal_gpio_pulse_in(wink_pin_t pin, bool level, uint32_t timeout_us, uint32_t *pulse_us) {
    if (pulse_us == NULL) { return WINK_ERR_INVALID_ARG; }
    if (pin < 0 || pin >= WASM_SIM_MAX_PINS) { return WINK_ERR_INVALID_ARG; }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) { return WINK_ERR_INVALID_STATE; }
    (void)level; (void)timeout_us;
    *pulse_us = js_sim_measure_echo_pulse_us((uint32_t)pin);
    return WINK_OK;
}

/* ─────────────────────────────────────────────────────────
 * Debug Output（PAL 统一接口）
 * ───────────────────────────────────────────────────────── */

void pal_debug_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
