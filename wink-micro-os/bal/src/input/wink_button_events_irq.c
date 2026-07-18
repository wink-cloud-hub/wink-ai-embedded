/**
 * @file wink_button_events_irq.c
 * @brief BAL button GPIO-IRQ backend (S3, ADR-0031).
 *
 * Architecture (ADR-0031 §S3):
 *   GPIO ISR (DAL, edge) ──► s_daemon_sem ──► daemon task ──► debounce timer
 *                                                          └─► on expiry, sample
 *                                                              stable pin and
 *                                                              dispatch PRESS/
 *                                                              RELEASE via the
 *                                                              shared state
 *                                                              machine.
 *   PRESS ──► arm long-press one-shot for cfg->long_press_ms.
 *   RELEASE ──► cancel long-press one-shot.
 *
 * Non-ESP32 targets get a stub: irq_supported() returns false and arm
 * returns WINK_ERR_UNSUPPORTED. The stub also provides the two
 * long-press timer helpers that events.c calls unconditionally.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "bal.btn.irq"

#include "input/wink_button_events.h"
#include "wink_button_events_internal.h"
#include "wink_status.h"
#include "pal_log.h"

#include <stddef.h>

/* DAL pruning: real IRQ backend needs the button driver (ADR-0039). */
#if defined(ESP_PLATFORM) \
    && defined(WINK_USE_BUTTON) && (WINK_USE_BUTTON)

#include "pal_osal.h"
#include "pal_hal.h"       /* pal_gpio_read for stable-state sampling */
#include "wink_soft_timer.h"
#include "dal_button.h"

/* ── Daemon singleton ─────────────────────────────────────────
 * The daemon is lazy-created on the first successful arm and runs for the
 * lifetime of the process. It uses ONE semaphore that the DAL ISR hook
 * gives on every edge; the daemon then scans all IRQ-armed slots and
 * arms per-slot debounce timers for any with irq_pending set. */
static pal_os_sem_t         s_daemon_sem  = NULL;
static pal_os_task_handle_t s_daemon_task = NULL;
static bool                 s_daemon_started = false;

/* ISR-safe hook: called by DAL from the shared GPIO ISR when a button
 * with BACKEND_IRQ observes an edge. Just give the daemon sem — no LOG,
 * no malloc, no timer_start. */
static void bal_irq_daemon_wake_isr(void *ctx) {
    (void)ctx;
    if (s_daemon_sem != NULL) {
        WINK_IGNORE_RESULT(pal_os_sem_give_isr(s_daemon_sem));
    }
}

/* Round up ms → soft-timer tick multiple. wink_soft_timer requires
 * period_ms to be a multiple of WINK_RUNTIME_TICK_MS (10 ms). */
static uint32_t align_tick(uint32_t ms) {
    if (ms < 10u) { return 10u; }
    /* Round up to nearest 10 ms. */
    return ((ms + 9u) / 10u) * 10u;
}

/* ── Debounce timer callback ──────────────────────────────────
 * One-shot per slot. On expiry we read the RAW pin (not DAL's stale
 * stable_pressed, which is only maintained in poll mode) and drive the
 * shared PRESS/RELEASE state machine. If more edges arrived while we
 * were pending, dal_button_consume_irq_pending() will re-arm this timer
 * on the next daemon wake. */
static wink_status_t irq_debounce_cb(void *arg) {
    button_event_slot_t *s = (button_event_slot_t *)arg;
    if (s == NULL || s->btn == NULL) { return WINK_OK; }
    bool raw = false;
    wink_status_t rs = pal_gpio_read(s->btn->config.pin, &raw);
    if (wink_status_is_error(rs)) { return WINK_OK; }
    /* active_low: pressed when raw == LOW (== !active_low). */
    bool stable_pressed = (raw != s->btn->config.active_low);
    wink_button_events_dispatch_stable(s, stable_pressed, pal_os_get_ms());
    return WINK_OK;
}

/* ── Long-press timer callback ───────────────────────────────
 * One-shot. Fires long_press_ms after a confirmed PRESS. If the button
 * is still held (not released before we get here), dispatch LONG_PRESS. */
static wink_status_t irq_longpress_cb(void *arg) {
    button_event_slot_t *s = (button_event_slot_t *)arg;
    if (s == NULL || s->btn == NULL) { return WINK_OK; }
    wink_button_events_dispatch_long_press(s);
    return WINK_OK;
}

/* Called from wink_button_events.c dispatch_stable() on PRESS transition. */
void wink_button_events_irq_arm_longpress(button_event_slot_t *s) {
    if (s == NULL || s->irq_longpress_h < 0 || s->long_press_ms == 0u) { return; }
    /* Restart the one-shot from now. change_period is zero-stall for
     * PERIODIC but for ONESHOT we just stop+start after aligning to tick. */
    WINK_IGNORE_RESULT(wink_soft_timer_stop(s->irq_longpress_h));
    WINK_IGNORE_RESULT(wink_soft_timer_change_period(s->irq_longpress_h,
                                                    align_tick(s->long_press_ms)));
    WINK_IGNORE_RESULT(wink_soft_timer_start(s->irq_longpress_h));
}

void wink_button_events_irq_cancel_longpress(button_event_slot_t *s) {
    if (s == NULL || s->irq_longpress_h < 0) { return; }
    WINK_IGNORE_RESULT(wink_soft_timer_stop(s->irq_longpress_h));
}

/* ── Daemon task body ─────────────────────────────────────────
 * Blocks on s_daemon_sem, wakes on any edge from any IRQ-armed button,
 * scans all slots and (re-)arms the per-slot debounce timer for those
 * with a pending flag. */
static void bal_irq_daemon_task(void *arg) {
    (void)arg;
    for (;;) {
        if (pal_os_sem_take(s_daemon_sem, WINK_MUTEX_WAIT_FOREVER) != WINK_OK) {
            continue;
        }
        for (int i = 0; i < WINK_BUTTON_EVENTS_MAX; i++) {
            button_event_slot_t *s = &g_button_event_slots[i];
            if (s->btn == NULL) { continue; }
            /* Belt-and-suspenders: skip slots that are not (or no longer)
             * GPIO_IRQ-driven, and slots whose debounce timer handle is
             * invalid. Both conditions are set by wink_button_events_irq_disarm
             * BEFORE it destroys the timers, so a daemon wake racing with
             * disarm on this slot will bail out here rather than stop/start
             * a soft-timer handle that is about to be destroyed (or has
             * already been reused by a subsequent arm). */
            if (s->drive != WINK_BUTTON_DRIVE_GPIO_IRQ) { continue; }
            if (s->irq_debounce_h < 0) { continue; }
            if (!dal_button_consume_irq_pending(s->btn)) { continue; }
            /* Fresh edge (or coalesced burst): (re-)arm the debounce timer.
             * If it was already running, stop+start restarts the settle
             * window — exactly what we want when bounces keep coming.
             * Re-check the handle: disarm may have raced in between the
             * pre-check above and the DAL consume. */
            if (s->irq_debounce_h >= 0 &&
                s->drive == WINK_BUTTON_DRIVE_GPIO_IRQ) {
                WINK_IGNORE_RESULT(wink_soft_timer_stop(s->irq_debounce_h));
                WINK_IGNORE_RESULT(wink_soft_timer_start(s->irq_debounce_h));
            }
        }
    }
}

/* Lazy-init the daemon on first arm. Idempotent. */
static wink_status_t ensure_daemon_started(void) {
    if (s_daemon_started) { return WINK_OK; }
    if (s_daemon_sem == NULL) {
        s_daemon_sem = pal_os_sem_create();
        if (s_daemon_sem == NULL) { return WINK_ERR_NO_MEM; }
    }
    /* Install the ISR hook exactly once — DAL only invokes it for buttons
     * with event_backend == BACKEND_IRQ, so setting it before any arm is
     * harmless. */
    dal_button_set_irq_hook(bal_irq_daemon_wake_isr, NULL);

    /* Priority: same as the default periodic (WINK_PERIODIC_DEFAULT_PRIORITY = 2).
     * The daemon is not latency-critical — a debounce window of tens of ms
     * dominates. Stack: 2 KiB is enough for the sem_take loop + one soft-timer
     * start (no allocation, no printf). */
    wink_status_t st = pal_os_task_create(
        bal_irq_daemon_task,
        "wink_btn_irq",
        2048u,
        NULL,
        /*prio=*/2,
        PAL_OS_CORE_ANY,
        &s_daemon_task);
    if (wink_status_is_error(st)) {
        LOG_E("bal.btn.irq: daemon task_create failed: %d", (int)st);
        return st;
    }
    s_daemon_started = true;
    return WINK_OK;
}

/* ── Public: is IRQ supported here? ──────────────────────────
 * We are the ESP_PLATFORM TU, so yes. */
bool wink_button_events_irq_supported(void) {
    return true;
}

/* ── Public: arm / disarm ────────────────────────────────────
 * arm order (rollback-safe):
 *   1. Create debounce timer.
 *   2. Create long-press timer (only if long_press_ms > 0).
 *   3. Set DAL event backend to IRQ.
 *   4. Ensure daemon running (creates sem + task on first call).
 *   5. Enable DAL shared GPIO ISR.
 * Anything failing rolls back the previous steps. */
wink_status_t wink_button_events_irq_arm(button_event_slot_t *slot,
                                         const wink_button_event_config_t *cfg)
{
    if (slot == NULL || slot->btn == NULL || cfg == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    uint32_t debounce_ms = cfg->debounce_ms > 0u ? cfg->debounce_ms : 20u;
    /* long_press_ms: read from DAL (set by dal_button_set_long_press_ms or
     * DAL default). We don't have a JSON knob for it in cfg (yet); fall
     * back to the DAL default via a private read. */
    uint32_t long_press_ms = slot->btn->long_press_ms;
    slot->long_press_ms = long_press_ms;

    /* Step 1: debounce timer (one-shot, aligned to 10 ms tick). */
    int32_t dh = wink_soft_timer_create(irq_debounce_cb, slot,
                                        WINK_TIMER_ONESHOT,
                                        align_tick(debounce_ms));
    if (dh < 0) {
        LOG_E("bal.btn.irq: debounce timer create failed: %d", (int)dh);
        return (wink_status_t)dh;
    }
    slot->irq_debounce_h = dh;

    /* Step 2: long-press timer (only if long_press_ms > 0). */
    if (long_press_ms > 0u) {
        int32_t lh = wink_soft_timer_create(irq_longpress_cb, slot,
                                            WINK_TIMER_ONESHOT,
                                            align_tick(long_press_ms));
        if (lh < 0) {
            LOG_E("bal.btn.irq: longpress timer create failed: %d", (int)lh);
            WINK_IGNORE_RESULT(wink_soft_timer_destroy(dh));
            slot->irq_debounce_h = -1;
            return (wink_status_t)lh;
        }
        slot->irq_longpress_h = lh;
    }

    /* Step 3: tell DAL this button is now IRQ-driven so its ISR sets
     * irq_pending + fires the hook. */
    dal_button_set_event_backend(slot->btn, DAL_BUTTON_BACKEND_IRQ);

    /* Step 4: lazy-init daemon (sem, hook, task). */
    wink_status_t st = ensure_daemon_started();
    if (wink_status_is_error(st)) {
        dal_button_set_event_backend(slot->btn, DAL_BUTTON_BACKEND_NONE);
        WINK_IGNORE_RESULT(wink_soft_timer_destroy(dh));
        if (slot->irq_longpress_h >= 0) {
            WINK_IGNORE_RESULT(wink_soft_timer_destroy(slot->irq_longpress_h));
            slot->irq_longpress_h = -1;
        }
        slot->irq_debounce_h = -1;
        return st;
    }

    /* Step 5: enable the shared GPIO ISR (refcount +1 in DAL). */
    st = dal_button_enable_gpio_isr(slot->btn);
    if (wink_status_is_error(st)) {
        LOG_E("bal.btn.irq: dal_button_enable_gpio_isr failed: %d", (int)st);
        dal_button_set_event_backend(slot->btn, DAL_BUTTON_BACKEND_NONE);
        WINK_IGNORE_RESULT(wink_soft_timer_destroy(dh));
        if (slot->irq_longpress_h >= 0) {
            WINK_IGNORE_RESULT(wink_soft_timer_destroy(slot->irq_longpress_h));
            slot->irq_longpress_h = -1;
        }
        slot->irq_debounce_h = -1;
        return st;
    }

    LOG_I("bal.btn.irq: armed pin=%u deb=%ums lp=%ums",
          (unsigned)slot->btn->config.pin,
          (unsigned)debounce_ms, (unsigned)long_press_ms);
    return WINK_OK;
}

void wink_button_events_irq_disarm(button_event_slot_t *slot)
{
    if (slot == NULL || slot->btn == NULL) { return; }
    /* Stop the shared ISR path first (drops the IRQ ref in DAL). Order:
     * clear event_backend BEFORE disable_gpio_isr so DAL's refcount check
     * sees that the IRQ consumer is gone. */
    dal_button_set_event_backend(slot->btn, DAL_BUTTON_BACKEND_NONE);
    dal_button_disable_gpio_isr(slot->btn);

    /* Snapshot handles + neutralise slot state BEFORE destroying the timers.
     * Race window: the daemon task may have been woken by a final edge and
     * be about to (re-)stop+start slot->irq_debounce_h. By writing
     *   drive = SOFT_POLL, irq_debounce_h = -1, irq_longpress_h = -1
     * first, any daemon wake in the window sees a slot that is either
     *   (a) not GPIO_IRQ any more (skips), or
     *   (b) has invalid timer handles (skips the stop+start),
     * so we never stop/start a soft-timer slot that we're about to destroy
     * or that has already been reused by a later arm. */
    int32_t dh = slot->irq_debounce_h;
    int32_t lh = slot->irq_longpress_h;
    slot->drive           = WINK_BUTTON_DRIVE_SOFT_POLL;
    slot->irq_debounce_h  = -1;
    slot->irq_longpress_h = -1;

    /* Destroy timers using the snapshot (safe on -1). */
    if (dh >= 0) {
        WINK_IGNORE_RESULT(wink_soft_timer_stop(dh));
        WINK_IGNORE_RESULT(wink_soft_timer_destroy(dh));
    }
    if (lh >= 0) {
        WINK_IGNORE_RESULT(wink_soft_timer_stop(lh));
        WINK_IGNORE_RESULT(wink_soft_timer_destroy(lh));
    }
    /* Daemon stays running (singleton) — no cost when no button is armed
     * (sem is not given so daemon just blocks). */
    LOG_I("bal.btn.irq: disarmed");
}

#else /* !ESP_PLATFORM or button pruned — stubs */

bool wink_button_events_irq_supported(void) {
    return false;
}

wink_status_t wink_button_events_irq_arm(button_event_slot_t *slot,
                                         const wink_button_event_config_t *cfg)
{
    (void)slot;
    (void)cfg;
    /* Should never be called: wink_button_enable_events checks
     * irq_supported() first. Return UNSUPPORTED as a hard-fail sentinel. */
    return WINK_ERR_UNSUPPORTED;
}

void wink_button_events_irq_disarm(button_event_slot_t *slot) {
    (void)slot;
    /* No-op stub: IRQ backend never armed on non-ESP32 targets. */
}

/* Long-press helpers called unconditionally by wink_button_events.c
 * dispatch_stable() on PRESS/RELEASE transitions — must be defined on
 * every target. On host/wasm the drive is always SOFT_POLL, so these
 * are technically unreachable, but keeping them as no-ops means the
 * dispatch_stable code stays branch-free at call sites. */
void wink_button_events_irq_arm_longpress(button_event_slot_t *s) {
    (void)s;
}
void wink_button_events_irq_cancel_longpress(button_event_slot_t *s) {
    (void)s;
}

#endif /* ESP_PLATFORM && WINK_USE_BUTTON */
