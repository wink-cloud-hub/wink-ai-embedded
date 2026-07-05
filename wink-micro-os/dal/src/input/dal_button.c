#include "dal_button.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "pal_osal.h"
#include "pal_irq.h"
#include <string.h> /* memcpy */

static bool button_raw_pressed(bool raw_level, bool active_low) {
    /* active_low: 按下=LOW(raw=false) → pressed=true */
    return raw_level != active_low;
}

/* ── ISR counter thunk (file-scope, PAL_DEFINE_ISR typed wrapper) ──
 * Each button with isr_counter_enabled uses the SAME thunk; the dev pointer
 * is passed via the ISR arg (pal_gpio_enable_interrupt stores it per-pin).
 * ISR contract: <10µs, no blocking API, only FromISR RTOS calls (none needed
 * here — we just increment a volatile counter). */
PAL_DEFINE_ISR(dal_button_edge_counter_isr, dal_button_t, dev) {
    /* Volatile ++ is NOT atomic on all architectures (Xtensa l32i/addi/s32i is
     * interruptible at the instruction level? No — a single instruction is
     * atomic w.r.t. interrupts on Xtensa, but between load and store an ISR
     * at higher prio could preempt.  However, all GPIO ISRs are registered
     * at PAL_IRQ_PRIO_NORMAL (pal_gpio_enable_interrupt default) and this ISR
     * itself cannot nest at equal priority on FreeRTOS (configMAX_SYSCALL_INTERRUPT_PRIORITY
     * masking plus same-priority ISRs don't nest on Xtensa).  A plain ++ is
     * safe for counting transitions on the same pin — re-entrant edge on the
     * SAME pin while already in this ISR cannot happen.
     *
     * On host/wasm the ISR runs single-threaded, no re-entrancy possible. */
    dev->edge_count++;
}

wink_status_t dal_button_init(dal_button_t *dev, const dal_button_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }

    /* Track A（M1）：GPIO 引脚冲突治理。 */
    wink_status_t rs = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner);
    if (wink_status_is_error(rs)) { return rs; }

    pal_gpio_mode_t mode = cfg->active_low ? PAL_GPIO_INPUT_PULLUP : PAL_GPIO_INPUT_PULLDOWN;
    wink_status_t status = pal_gpio_init(cfg->pin, mode);
    if (wink_status_is_error(status)) {
        WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner));
        return status;
    }
    /* 深拷贝配置到实例（支持 ADR-0008 Flash 动态覆写） */
    memcpy(&dev->config, cfg, sizeof(dal_button_config_t));
    dev->stable_pressed   = false;
    dev->last_reported    = false;
    dev->initialized      = true;
    dev->debounce_counter = 0;

    /* Wave 3: 初始化新增字段 */
    dev->event_cb            = NULL;
    dev->event_cb_ctx        = NULL;
    dev->long_press_fired    = false;
    dev->prev_pressed_for_event = false;
    dev->long_press_ms       = DAL_BUTTON_DEFAULT_LONG_PRESS_MS;
    dev->press_start_ms      = 0;
    dev->isr_counter_enabled = false;
    dev->edge_count          = 0;
    return WINK_OK;
}

wink_status_t dal_button_poll(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    bool raw = false;
    wink_status_t s = pal_gpio_read(dev->config.pin, &raw);
    if (wink_status_is_error(s)) { return s; }

    bool now_pressed = button_raw_pressed(raw, dev->config.active_low);

    /* ── 去抖状态机（保持原有逻辑） ── */
    if (now_pressed == dev->stable_pressed) {
        dev->debounce_counter = 0;
    } else {
        dev->debounce_counter++;
        if (dev->debounce_counter >= DAL_BUTTON_DEBOUNCE_THRESHOLD) {
            dev->stable_pressed = now_pressed;
            dev->debounce_counter = 0;
        }
    }

    /* ── Wave 3: 事件派发（仅在 stable_pressed 翻转时触发边沿事件） ── */
    if (dev->event_cb != NULL && dev->stable_pressed != dev->prev_pressed_for_event) {
        dev->prev_pressed_for_event = dev->stable_pressed;
        if (dev->stable_pressed) {
            /* 按下：记录起始时间，重置 long_press 锁 */
            dev->press_start_ms   = pal_os_get_ms();
            dev->long_press_fired = false;
            dev->event_cb(DAL_BUTTON_EVT_PRESS, dev->event_cb_ctx);
        } else {
            /* 释放：重置长按锁 */
            dev->long_press_fired = false;
            dev->event_cb(DAL_BUTTON_EVT_RELEASE, dev->event_cb_ctx);
        }
    }

    /* ── Wave 3: 长按检测（poll 驱动，按住期间持续检查） ── */
    if (dev->event_cb != NULL && dev->stable_pressed && !dev->long_press_fired) {
        uint64_t held_ms = pal_os_get_ms() - dev->press_start_ms;
        if (held_ms >= dev->long_press_ms) {
            dev->long_press_fired = true;
            dev->event_cb(DAL_BUTTON_EVT_LONG_PRESS, dev->event_cb_ctx);
        }
    }

    return WINK_OK;
}

wink_status_t dal_button_is_pressed(const dal_button_t *dev, bool *out_pressed) {
    if (dev == NULL || out_pressed == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    *out_pressed = dev->stable_pressed;
    return WINK_OK;
}

wink_status_t dal_button_was_pressed(dal_button_t *dev, bool *out_was_pressed) {
    if (dev == NULL || out_was_pressed == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    bool event = (dev->stable_pressed && !dev->last_reported);
    dev->last_reported = dev->stable_pressed;
    *out_was_pressed = event;
    return WINK_OK;
}

/* ── Wave 3: Event callback API ─────────────────────────── */

wink_status_t dal_button_on_event(dal_button_t *dev, dal_button_event_cb cb, void *ctx) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    dev->event_cb     = cb;
    dev->event_cb_ctx = ctx;
    /* 重置边沿检测基线，避免注册瞬间的 stale 状态误触发 */
    dev->prev_pressed_for_event = dev->stable_pressed;
    dev->long_press_fired       = false;
    return WINK_OK;
}

wink_status_t dal_button_set_long_press_ms(dal_button_t *dev, uint32_t ms) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (ms == 0) { return WINK_ERR_INVALID_ARG; }
    dev->long_press_ms = ms;
    return WINK_OK;
}

/* ── Wave 3: ISR edge counter ───────────────────────────── */

wink_status_t dal_button_enable_isr_counter(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (dev->isr_counter_enabled) { return WINK_OK; } /* 幂等 */

    /* 注册 ANY_EDGE GPIO ISR，使用 file-scope thunk（PAL_DEFINE_ISR 生成
     * 的 dal_button_edge_counter_isr 解包装函数）。prio 默认 NORMAL。 */
    wink_status_t st = pal_gpio_enable_interrupt(
        dev->config.pin,
        PAL_GPIO_INTR_ANY_EDGE,
        dal_button_edge_counter_isr,
        dev);
    if (wink_status_is_error(st)) { return st; }

    dev->edge_count          = 0;
    dev->isr_counter_enabled = true;
    return WINK_OK;
}

wink_status_t dal_button_get_edge_count(const dal_button_t *dev, uint32_t *out_count) {
    if (dev == NULL || out_count == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    /* Volatile 读本身是 atomic 的（单 word 对齐读在 Xtensa/host/wasm 上是
     * 单指令），无需临界区——只是快照，允许 ±1 的瞬时竞争。 */
    *out_count = dev->edge_count;
    return WINK_OK;
}

wink_status_t dal_button_reset_edge_count(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    /* 关中断再清零，避免清零瞬间的 ISR 触发丢失（ISR 在临界区内被 PENDING，
     * 恢复后写回新计数 1）。 */
    PAL_CRITICAL_SECTION({
        dev->edge_count = 0;
    });
    return WINK_OK;
}
