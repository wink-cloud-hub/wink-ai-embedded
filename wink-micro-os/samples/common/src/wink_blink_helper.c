/**
 * @file wink_blink_helper.c
 * @brief Soft_timer-based LED blink helper (samples/common).
 */
#define LOG_TAG "blink"

#include "wink_blink_helper.h"
#include "wink_soft_timer.h"
#include "wink_status.h"
#include "pal_log.h"

/* ── per-handle context (soft_timer callback is one-arg) ────────────────── */
typedef struct {
    dal_led_t *led;
    bool        on;
} blink_ctx_t;

/* Soft-timer callback: toggle the LED each tick.  Returns WINK_OK so the
 * periodic timer keeps running.  WINK_IGNORE_RESULT pattern for warn_unused_result. */
static wink_status_t blink_tick(void *arg)
{
    blink_ctx_t *ctx = (blink_ctx_t *)arg;
    ctx->on = !ctx->on;
    WINK_IGNORE_RESULT(ctx->on ? dal_led_on(ctx->led) : dal_led_off(ctx->led));
    return WINK_OK;
}

int32_t wink_led_blink_start(dal_led_t *led, uint32_t period_ms)
{
    if (led == NULL || period_ms == 0u) {
        return (int32_t)WINK_ERR_INVALID_ARG;
    }

    /* Allocate a static slot for the context.  blink_start is expected to be
     * called at most WINK_BLINK_MAX times in a sample; we keep a small pool
     * to avoid malloc (embedded: deterministic). */
    static blink_ctx_t s_ctxs[4];
    static size_t      s_next = 0;
    if (s_next >= sizeof(s_ctxs) / sizeof(s_ctxs[0])) {
        LOG_E("blink: out of blink slots (%u)", (unsigned)(sizeof(s_ctxs)/sizeof(s_ctxs[0])));
        return (int32_t)WINK_ERR_RESOURCE_EXHAUSTED;
    }
    blink_ctx_t *ctx = &s_ctxs[s_next++];
    ctx->led = led;
    ctx->on  = true;

    /* Half-period per toggle -> full on+off cycle = period_ms. */
    uint32_t half = period_ms / 2u;
    if (half == 0u) half = 1u;

    int32_t h = wink_soft_timer_create(blink_tick, ctx, WINK_TIMER_PERIODIC, half);
    if (h < 0) {
        s_next--; /* recycle slot */
        return h;
    }
    wink_status_t st = wink_soft_timer_start(h);
    if (wink_status_is_error(st)) {
        WINK_IGNORE_RESULT(wink_soft_timer_stop(h));
        s_next--;
        return (int32_t)st;
    }

    /* Start with LED on to match ctx->on initial state. */
    WINK_IGNORE_RESULT(dal_led_on(led));
    return h;
}

void wink_led_blink_stop(int32_t handle)
{
    if (handle >= 0) {
        WINK_IGNORE_RESULT(wink_soft_timer_stop(handle));
    }
}
