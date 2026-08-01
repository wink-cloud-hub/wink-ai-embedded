/**
 * @file wink_led_blink.c
 * @brief BAL LED blink helper —toggles a DAL LED at 50% duty via the
 *        runtime periodic scheduler (LIGHT path by default).
 *
 * Slot management (fixes the LIFO bug from the original samples/common
 * helper): uses a free-list scan instead of a monotonically-incremented
 * s_next cursor.  blink_stop() marks the slot free (led = NULL), so
 * slots are reclaimed and start/stop cycles never permanently exhaust
 * the pool (test_start_stop_loop_100_does_not_exhaust regression guard).
 *
 * Handle encoding: we return the underlying wink_periodic_handle_t
 * directly (>=1 = valid, 0 = INVALID, <0 = error passthrough) so the
 * caller can pass it to wink_led_blink_stop() without an indirection
 * table.  blink_ctx_t slots are indexed via a reverse-lookup on stop
 * (iterating the slot array is O(WINK_LED_BLINK_MAX) —4, negligible).
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "bal.blink"

#include "output/wink_led_blink.h"
#include "wink_tasks.h"
#include "wink_status.h"
#include "pal_log.h"

#include <stddef.h>

/* DAL pruning (WINK_UNAVAILABLE): force the MAX==0 stub path when the
 * LED driver is off so ESP32 still links wink_bal (ADR-0039). */
#if !defined(WINK_USE_LED) || !(WINK_USE_LED)
#  undef WINK_LED_BLINK_MAX
#  define WINK_LED_BLINK_MAX 0
#endif

#if WINK_LED_BLINK_MAX > 0

/* ?? per-blink context ?????????????????????????????????????????
 * A slot with led == NULL is free.  period_h is only meaningful when
 * led != NULL.  BSS zero-init gives us led=NULL for free-slot state. */
typedef struct {
    dal_led_t             *led;
    wink_periodic_handle_t  period_h;
} blink_ctx_t;

static blink_ctx_t s_slots[WINK_LED_BLINK_MAX];

/* ?? internal helpers ?????????????????????????????????????????? */

/* Map BAL core-affinity enum to pal_os_core_id_t for the runtime call. */
static pal_os_core_id_t map_core(wink_bal_core_t c) {
    switch (c) {
        case WINK_BAL_CORE_0: return PAL_OS_CORE_0;
        case WINK_BAL_CORE_1: return PAL_OS_CORE_1;
        case WINK_BAL_CORE_ANY:        /* fallthrough */
        case WINK_BAL_CORE_INVALID:    /* fallthrough */
        default:             return PAL_OS_CORE_ANY;
    }
}

/* Find a free slot index (led == NULL), or -1 if pool exhausted. */
static int find_free_slot(void) {
    for (int i = 0; i < WINK_LED_BLINK_MAX; i++) {
        if (s_slots[i].led == NULL) {
            return i;
        }
    }
    return -1;
}

/* Find slot index currently owning @p h, or -1. */
static int find_slot_by_handle(wink_periodic_handle_t h) {
    for (int i = 0; i < WINK_LED_BLINK_MAX; i++) {
        if (s_slots[i].led != NULL && s_slots[i].period_h == h) {
            return i;
        }
    }
    return -1;
}

/* ?? periodic callback (LIGHT path —void return, void* ctx) ??? */
static void blink_tick(void *arg) {
    blink_ctx_t *ctx = (blink_ctx_t *)arg;
    /* Toggle: track on/off state via the DAL LED's own is_on field (public
     * member of dal_led_t per dal_led.h contract) so we don't double-store it. */
    bool next_on = !ctx->led->is_on;
    WINK_IGNORE_RESULT(dal_led_set(ctx->led, next_on));
}

/* ?? public API ???????????????????????????????????????????????? */

int32_t wink_led_blink_start_ex(dal_led_t *led, uint32_t period_ms,
                                const wink_bal_opts_t *opts)
{
    if (led == NULL || period_ms == 0u) {
        LOG_D("start: invalid arg (led=%p period_ms=%u)",
              (void *)led, (unsigned)period_ms);
        return (int32_t)WINK_ERR_INVALID_ARG;
    }

    /* Reject double-start on the same LED (matching button_helper's
     * duplicate-start guard). Caller should stop first. */
    for (int i = 0; i < WINK_LED_BLINK_MAX; i++) {
        if (s_slots[i].led == led) {
            LOG_D("start: led=%p already blinking", (void *)led);
            return (int32_t)WINK_ERR_INVALID_STATE;
        }
    }

    int free_idx = find_free_slot();
    if (free_idx < 0) {
        LOG_D("start: out of blink slots (%d)", WINK_LED_BLINK_MAX);
        return (int32_t)WINK_ERR_RESOURCE_EXHAUSTED;
    }

    /* Resolve options (NULL —defaults). */
    wink_bal_opts_t effective = WINK_BAL_OPTS_DEFAULT;
    if (opts != NULL) {
        effective = *opts;
    }

    /* Default flags for blink: LIGHT path (non-blocking GPIO toggle).
     * If caller explicitly sets flags (non-zero), honour them. */
    uint32_t flags = effective.flags;
    if (flags == 0u) {
        flags = WINK_PERIODIC_LIGHT;
    }

    /* Half-period per toggle —full on+off cycle = period_ms. */
    uint32_t half = period_ms / 2u;
    if (half == 0u) {
        half = 1u;
    }

    uint32_t stack = (effective.stack_bytes != 0u) ? effective.stack_bytes : 0u;
    int32_t  prio  = (effective.priority >= 0)       ? effective.priority
                                                     : WINK_PERIODIC_DEFAULT_PRIORITY;
    pal_os_core_id_t core = map_core(effective.core_id);

    blink_ctx_t *ctx = &s_slots[free_idx];
    ctx->led = led;
    ctx->period_h = WINK_PERIODIC_INVALID;

    /* Start with LED on to match our "first toggle turns off" cycle. */
    wink_status_t st = dal_led_on(led);
    if (wink_status_is_error(st)) {
        LOG_D("start: dal_led_on failed: %d", (int)st);
        ctx->led = NULL; /* roll back slot allocation */
        return (int32_t)st;
    }

    wink_periodic_handle_t h = wink_periodic_start_ex(
        "blink", stack, half, blink_tick, ctx, flags, prio, core);
    if (h < 0) {
        LOG_D("start: periodic_start failed: %d", (int)h);
        /* Try to restore LED-off to avoid leaving it on after failure. */
        WINK_IGNORE_RESULT(dal_led_off(led));
        ctx->led = NULL;
        return (int32_t)h;
    }

    ctx->period_h = h;
    return (int32_t)h;
}

int32_t wink_led_blink_start(dal_led_t *led, uint32_t period_ms) {
    return wink_led_blink_start_ex(led, period_ms, NULL);
}

void wink_led_blink_stop(int32_t handle)
{
    /* wink_periodic_stop() already silently no-ops on h <= 0 (covers
     * INVALID sentinel and propagated error codes). We still need to
     * clear our slot so the pool is recycled. */
    if (handle <= 0) {
        return;
    }
    wink_periodic_handle_t h = (wink_periodic_handle_t)handle;

    int idx = find_slot_by_handle(h);
    if (idx < 0) {
        /* Not owned by us (e.g. caller passed stale/random handle).
         * Still forward to wink_periodic_stop for safety —it will
         * no-op if handle doesn't correspond to a live periodic. */
        wink_periodic_stop(h);
        return;
    }

    blink_ctx_t *ctx = &s_slots[idx];
    wink_periodic_stop(ctx->period_h);
    ctx->period_h = WINK_PERIODIC_INVALID;
    ctx->led      = NULL;   /* mark slot free for reuse */
}

#else /* WINK_LED_BLINK_MAX == 0 */

int32_t wink_led_blink_start_ex(dal_led_t *led, uint32_t period_ms,
                                const wink_bal_opts_t *opts)
{
    (void)led;
    (void)period_ms;
    (void)opts;
    return (int32_t)WINK_ERR_UNSUPPORTED;
}

int32_t wink_led_blink_start(dal_led_t *led, uint32_t period_ms)
{
    (void)led;
    (void)period_ms;
    return (int32_t)WINK_ERR_UNSUPPORTED;
}

void wink_led_blink_stop(int32_t handle)
{
    (void)handle;
}

#endif /* WINK_LED_BLINK_MAX > 0 */
