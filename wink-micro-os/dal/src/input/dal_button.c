#include "dal_button.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "pal_osal.h"
#include "pal_irq.h"
#include <string.h> /* memcpy */

/* ── File-scope: BAL IRQ notify hook (single process-global slot) ──
 * Set by BAL's IRQ backend (dal_button_set_irq_hook) so the shared GPIO ISR
 * can wake the BAL daemon without DAL depending on BAL.  DAL never invokes
 * this hook unless the offending button has event_backend == BACKEND_IRQ,
 * so a stale hook pointer left after the last IRQ-using button was torn
 * down is harmless.  Access is a plain load in the ISR: hook install/
 * uninstall happens on the task path and the ISR just does an ISR-safe
 * function-pointer read; ADR-0018 §atomic-pointer-load. */
static volatile dal_button_irq_notify_hook_t s_irq_hook = NULL;
static void *s_irq_hook_ctx = NULL;

static bool button_raw_pressed(bool raw_level, bool active_low) {
    /* active_low: 按下=LOW(raw=false) → pressed=true */
    return raw_level != active_low;
}

static bool button_pull_valid(dal_button_pull_t pull)
{
    return pull <= DAL_BUTTON_PULL_NONE;
}

static pal_gpio_mode_t button_gpio_mode(const dal_button_config_t *cfg)
{
    if (cfg->pull == DAL_BUTTON_PULL_AUTO) {
        return cfg->active_low ? PAL_GPIO_INPUT_PULLUP : PAL_GPIO_INPUT_PULLDOWN;
    }
    if (cfg->pull == DAL_BUTTON_PULL_UP) {
        return PAL_GPIO_INPUT_PULLUP;
    }
    if (cfg->pull == DAL_BUTTON_PULL_DOWN) {
        return PAL_GPIO_INPUT_PULLDOWN;
    }
    return PAL_GPIO_INPUT; /* NONE */
}

/* ── Unified GPIO ISR thunk (file-scope, PAL_DEFINE_ISR typed wrapper) ──
 * Registered exactly once per pin when either isr_counter_enabled or
 * event_backend == BACKEND_IRQ becomes true (refcount managed by
 * dal_button_enable_gpio_isr / dal_button_disable_gpio_isr).
 *
 * ISR contract (ADR-0018 §ISR body ≤ 10 simple ops):
 *   1. if isr_counter_enabled: ++edge_count (volatile, atomic on all supported ISAs)
 *   2. if event_backend == BACKEND_IRQ:
 *        - set irq_pending = true (volatile)
 *        - call BAL hook if installed (ISR-safe sem give — no LOG/malloc/timer)
 *
 * NO log, NO malloc, NO event_post, NO timer_start. All heavy work happens
 * in the BAL daemon task after debounce timer fires. */
PAL_DEFINE_ISR(dal_button_gpio_isr, dal_button_t, dev) {
    /* Counter path: same reasoning as the retired dal_button_edge_counter_isr
     * — same-priority GPIO ISRs on the same pin cannot preempt each other. */
    if (dev->isr_counter_enabled) {
        dev->edge_count++;
    }
    /* Event-IRQ path: flag + wake daemon. Hook is a plain volatile pointer;
     * BAL guarantees it is either NULL (no daemon armed) or points to an
     * ISR-safe give function that survives for the lifetime of the daemon. */
    if (dev->event_backend == DAL_BUTTON_BACKEND_IRQ) {
        dev->irq_pending = true;
        dal_button_irq_notify_hook_t hook = s_irq_hook;
        if (hook != NULL) {
            hook(s_irq_hook_ctx);
        }
    }
}

wink_status_t dal_button_init(dal_button_t *dev, const dal_button_config_t *cfg) {
    if (dev == NULL || cfg == NULL) { return WINK_ERR_INVALID_ARG; }
    if (cfg->owner == NULL) { return WINK_ERR_INVALID_ARG; }
    /* DAL-L-004: detect double-init FIRST so the error path doesn't
     * clobber the existing initialized=true marker.  This check must
     * come before the DAL-L-007 unconditional reset below. */
    if (dev->initialized) { return WINK_ERR_ALREADY_INITIALIZED; }
    if (!button_pull_valid(cfg->pull)) { return WINK_ERR_INVALID_ARG; }

    /* DAL-L-007: even on early-return paths (after the ALREADY check),
     * dev->initialized MUST stay false so a subsequent deinit is safe
     * (DAL-L-010 idempotent).  Explicit reset here means we don't depend
     * on the {0}-init assumption from the caller. */
    dev->initialized = false;

    /* DAL-L-008: chained resource acquisition with goto-cleanup rollback.
     * Each step inverts in REVERSE order on failure.  Without the cleanup
     * label, every new step would need its own bespoke inline release,
     * which is exactly the class of bug we already hit once (the original
     * 4 inline-release sequences diverged when the GPIO init step moved
     * to a different code path). */
    bool          pin_claimed = false;
    bool          pin_inited  = false;
    wink_status_t rc;

    /* Track A（M1）：GPIO 引脚冲突治理。 */
    rc = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner);
    if (wink_status_is_error(rc)) { return rc; }
    pin_claimed = true;

    pal_gpio_mode_t mode = button_gpio_mode(cfg);
    rc = pal_gpio_init(cfg->pin, mode);
    if (wink_status_is_error(rc)) { goto cleanup; }
    pin_inited = true;
    /* 深拷贝配置到实例（支持 ADR-0008 Flash 动态覆写） */
    memcpy(&dev->config, cfg, sizeof(dal_button_config_t));
    dev->stable_pressed   = false;
    dev->last_reported    = false;
    dev->initialized      = true;
    dev->debounce_counter = 0;
    dev->debounce_threshold = DAL_BUTTON_DEBOUNCE_THRESHOLD;

    /* Wave 3: 初始化新增字段 */
    dev->event_cb            = NULL;
    dev->event_cb_ctx        = NULL;
    dev->long_press_fired    = false;
    dev->prev_pressed_for_event = false;
    dev->long_press_ms       = DAL_BUTTON_DEFAULT_LONG_PRESS_MS;
    dev->press_start_ms      = 0;
    dev->last_status         = WINK_OK;
    dev->isr_counter_enabled = false;
    dev->edge_count          = 0;

    /* Wave 4 (S3): shared-ISR + BAL event-backend fan-out fields */
    dev->event_backend       = DAL_BUTTON_BACKEND_NONE;
    dev->gpio_isr_registered = false;
    dev->irq_pending         = false;
    return WINK_OK;

cleanup:
    /* Roll back in REVERSE order.  pal_gpio_reset_pin is the inverse of
     * pal_gpio_init; pal_resource_release is the inverse of pal_resource_claim.
     * pal_gpio_reset_pin returns void, so use plain (void) cast; the other
     * returns wink_status_t and uses WINK_IGNORE_UNUSED to silence the
     * warn_unused_result attribute.  We do NOT memset the whole handle here
     * because the caller may still be holding a reference to inspect the
     * failed-init state; the dedicated dal_button_deinit is the path that
     * clears everything (DAL-L-013). */
    if (pin_inited)  { (void)pal_gpio_reset_pin(cfg->pin); }
    if (pin_claimed) { WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, cfg->pin, cfg->owner)); }
    /* dev->initialized already false (set at function top per DAL-L-007). */
    return rc;
}

wink_status_t dal_button_poll(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    bool raw = false;
    wink_status_t s = pal_gpio_read(dev->config.pin, &raw);
    if (wink_status_is_error(s)) {
        /* DAL-B-025: record the error so callers can introspect it via
         * dal_button_get_status() without losing the state machine.  The
         * state machine itself is left untouched — the previous stable
         * state remains valid until the next successful poll restores
         * a fresh reading. */
        dev->last_status = s;
        return s;
    }
    dev->last_status = WINK_OK;

    bool now_pressed = button_raw_pressed(raw, dev->config.active_low);

    /* ── 去抖状态机（保持原有逻辑） ── */
    if (now_pressed == dev->stable_pressed) {
        dev->debounce_counter = 0;
    } else {
        dev->debounce_counter++;
        if (dev->debounce_counter >= dev->debounce_threshold) {
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

wink_status_t dal_button_get_status(const dal_button_t *dev, wink_status_t *out_status) {
    if (dev == NULL || out_status == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    /* Volatile read; no critical section needed (a single wink_status_t word
     * load is atomic on Xtensa/host/wasm; the read may race with a poll
     * write but the worst case is observing a stale value, not a torn one). */
    *out_status = dev->last_status;
    return WINK_OK;
}

wink_status_t dal_button_was_pressed(dal_button_t *dev, bool *out_was_pressed) {
    if (dev == NULL || out_was_pressed == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }

    /* DAL-V-010: read-clear of (stable_pressed, last_reported) MUST be atomic
     * with respect to a concurrent was_pressed caller on another core / in an
     * ISR-context poll.  Without a critical section, two callers can both
     * observe the rising edge and both write last_reported=true, causing a
     * single press to be reported twice.  Wrap read + write in a single
     * PAL_CRITICAL_SECTION so the two fields are observed as a consistent
     * snapshot by every caller (SMP-safe). */
    bool event = false;
    PAL_CRITICAL_SECTION({
        event = (dev->stable_pressed && !dev->last_reported);
        dev->last_reported = dev->stable_pressed;
        *out_was_pressed = event;
    });
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

wink_status_t dal_button_set_debounce_ms(dal_button_t *dev, uint32_t ms) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (ms == 0u) { return WINK_ERR_INVALID_ARG; }

    /* Convert ms → 连续一致采样数：floor(ms / 10 ms tick), min 1, clamp to
     * uint8_t max (255 samples ≈ 2.55 s @10 ms tick — anything beyond this
     * is well outside the "debounce" regime). */
    uint32_t samples = ms / 10u;
    if (samples < 1u) { samples = 1u; }
    if (samples > 255u) { samples = 255u; }
    dev->debounce_threshold = (uint8_t)samples;
    /* Reset the running counter so a stale in-progress transition doesn't
     * fire off the new (potentially smaller) threshold on the very next poll. */
    dev->debounce_counter = 0;
    return WINK_OK;
}

/* ── Wave 4 (S3): shared GPIO ISR — refcount enable / disable ─────
 * The unified ISR (dal_button_gpio_isr) is installed on first enable and
 * uninstalled on last disable. Both isr_counter_enabled and
 * event_backend == BACKEND_IRQ count as "someone needs the ISR".  Callers:
 *   - dal_button_enable_isr_counter → flips isr_counter_enabled ON, then
 *     calls dal_button_enable_gpio_isr.
 *   - BAL IRQ arm → sets event_backend, then calls dal_button_enable_gpio_isr.
 *   - deinit / BAL disarm / counter-disable → calls dal_button_disable_gpio_isr;
 *     the shared ISR stays installed until BOTH refs drop. */

wink_status_t dal_button_enable_gpio_isr(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (dev->gpio_isr_registered) { return WINK_OK; } /* 幂等 */

    wink_status_t st = pal_gpio_enable_interrupt(
        dev->config.pin,
        PAL_GPIO_INTR_ANY_EDGE,
        dal_button_gpio_isr,
        dev);
    if (wink_status_is_error(st)) { return st; }
    dev->gpio_isr_registered = true;
    return WINK_OK;
}

void dal_button_disable_gpio_isr(dal_button_t *dev) {
    if (dev == NULL || !dev->initialized) { return; }
    if (!dev->gpio_isr_registered) { return; }
    /* Only unregister the shared ISR when no consumer needs it anymore.
     * counter still on OR event backend still IRQ → keep it. */
    if (dev->isr_counter_enabled) { return; }
    if (dev->event_backend == DAL_BUTTON_BACKEND_IRQ) { return; }

    WINK_IGNORE_UNUSED(pal_gpio_disable_interrupt(dev->config.pin));
    /* Synchronize is only strictly needed before freeing ISR arg / resetting
     * pin (deinit path handles that separately).  For a plain refcount drop
     * we still synchronize so a subsequent set_event_backend(NONE) → re-
     * enable(IRQ) cannot race with an in-flight ISR that still holds the old
     * dev pointer.  Safe on all targets (host/wasm no-op). */
    WINK_IGNORE_UNUSED(pal_gpio_synchronize_interrupt(dev->config.pin));
    dev->gpio_isr_registered = false;
}

/* ── Wave 3: ISR edge counter ───────────────────────────── */

wink_status_t dal_button_enable_isr_counter(dal_button_t *dev) {
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    if (dev->isr_counter_enabled) { return WINK_OK; } /* 幂等 */

    /* Flip the counter refcount BEFORE registering the shared ISR so that
     * the very first edge the ISR observes is already accounted for. */
    dev->edge_count          = 0;
    dev->isr_counter_enabled = true;

    wink_status_t st = dal_button_enable_gpio_isr(dev);
    if (wink_status_is_error(st)) {
        /* Roll back the refcount flip. */
        dev->isr_counter_enabled = false;
        return st;
    }
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

/* ── Wave 4 (S3): BAL event-backend + IRQ pending consume ─── */

void dal_button_set_event_backend(dal_button_t *dev, uint8_t backend) {
    if (dev == NULL) { return; }
    /* Field is read from ISR context; update via critical section so an
     * in-flight ISR either sees the OLD or the NEW backend consistently and
     * never a torn value.  On the platforms we care about (Xtensa / host /
     * wasm) uint8_t writes are already atomic, but the critical section
     * keeps the semantics honest under -O2 reordering. */
    PAL_CRITICAL_SECTION({
        dev->event_backend = backend;
    });
}

wink_status_t dal_button_consume_irq_pending(dal_button_t *dev,
                                             bool *out_was_pending) {
    if (dev == NULL || out_was_pending == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!dev->initialized) { return WINK_ERR_NOT_INITIALIZED; }
    bool was_pending = false;
    PAL_CRITICAL_SECTION({
        was_pending = dev->irq_pending;
        dev->irq_pending = false;
    });
    *out_was_pending = was_pending;
    return WINK_OK;
}

void dal_button_set_irq_hook(dal_button_irq_notify_hook_t fn, void *ctx) {
    /* Task-context install; the ISR reads s_irq_hook as a plain volatile
     * pointer load.  No critical section needed on install because a NULL
     * hook is a valid transient value (ISR just skips) — the worst case is
     * one dropped ISR-notify at the instant of install, which the debounce
     * timer's stable-state read will still catch on the next edge. */
    s_irq_hook_ctx = ctx;
    s_irq_hook     = fn;
}

wink_status_t dal_button_deinit(dal_button_t *dev) {
    /* ADR-0024 §4 deinit — checked: 1(N/A: button safe-off is Hi-Z, no actuator)/
     *   2(pal_gpio_reset_pin)/3(disable_interrupt→synchronize before reset)/
     *   4(N/A: no DMA)/5(N/A)/6(N/A)/7(memset clears event_cb/counter/last_ts)/
     *   8(NULL+uninit idempotent)/9(<100µs, no waits)/10(signature unified) */
    if (dev == NULL) { return WINK_ERR_INVALID_ARG; }
    if (!dev->initialized) { return WINK_OK; }  /* idempotent no-op on un-init dev */

    /* Keep pin for resource release and GPIO reset (read before any memset). */
    uint16_t pin = dev->config.pin;
    const char *owner = dev->config.owner;

    /* 3. Drop BOTH refs and then let the shared ISR uninstall itself.
     *    Order matters: clear backend + counter first, THEN disable, so
     *    disable_gpio_isr sees no live consumers and actually unregisters. */
    dev->event_backend       = DAL_BUTTON_BACKEND_NONE;
    dev->isr_counter_enabled = false;
    if (dev->gpio_isr_registered) {
        WINK_IGNORE_UNUSED(pal_gpio_disable_interrupt(pin));
        WINK_IGNORE_UNUSED(pal_gpio_synchronize_interrupt(pin));
        dev->gpio_isr_registered = false;
    }

    /* 2. Reset GPIO: disables any leftover routing, reverts to Hi-Z INPUT,
     *    clears esp_gpio_reserve bitmap (ADR-0024 §4 #2). */
    pal_gpio_reset_pin(pin);

    /* Release software resource claim */
    WINK_IGNORE_UNUSED(pal_resource_release(PAL_RESOURCE_GPIO_PIN, pin, owner));

    /* 7. Clear the instance data completely to guarantee no residual state */
    memset(dev, 0, sizeof(dal_button_t));

    return WINK_OK;
}
