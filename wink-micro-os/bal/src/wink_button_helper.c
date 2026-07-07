/**
 * @file wink_button_helper.c
 * @brief BAL button auto-poll helper — polls a DAL button at a fixed
 *        period via the runtime periodic scheduler (LIGHT path).
 *
 * Slot management: uses a free-list scan (not a monotonically-incremented
 * cursor) so that stop() marks the slot free (btn = NULL) and the pool
 * is recycled correctly across start/stop cycles.
 *
 * Callback runs on the LIGHT (soft-timer) path: dal_button_poll() is a
 * pure non-blocking GPIO read + debounce state machine, which matches
 * the LIGHT contract (no blocking calls, < 100 us work).
 *
 * Unlike the old samples/common helper (which called only
 * wink_soft_timer_stop and leaked soft_timer slots), this implementation
 * goes through wink_periodic_start_ex / wink_periodic_stop, which fully
 * tears down the underlying slot on stop.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "bal.btn"

#include "wink_button_helper.h"
#include "wink_tasks.h"
#include "wink_status.h"
#include "wink_pt_debug.h"  /* WINK_ASSERT_NONBLOCKING() (ADR-0017 layer 3) */
#include "pal_log.h"
#include "pal_osal.h"      /* pal_os_get_us() for execution-time budget watchdog */

#include <stddef.h>

/* LIGHT callback execution-time budget (P1-3 Step 5): if a single poll
 * invocation exceeds this, we LOG_W to expose AI-/developer-written event
 * callbacks that block or do too much work in the LIGHT (soft-timer)
 * context. The entire runtime tick is 10 ms; LIGHT timers, polls, and
 * app_loop all share that budget. 100 us keeps button poll at ≤1% of tick. */
#define WINK_BTN_POLL_BUDGET_US  100u

/* ── per-button slot ────────────────────────────────────────────
 * A slot with btn == NULL is free. period_h is only meaningful when
 * btn != NULL. BSS zero-init gives us btn=NULL for free-slot state. */
typedef struct {
    dal_button_t           *btn;
    wink_periodic_handle_t  period_h;
} btn_ctx_t;

static btn_ctx_t s_slots[WINK_BUTTON_HELPER_MAX];

/* ── internal helpers ─────────────────────────────────────────── */

/* Find slot index currently tracking @p btn, or -1 if not tracked. */
static int find_slot_by_btn(const dal_button_t *btn) {
    for (int i = 0; i < WINK_BUTTON_HELPER_MAX; i++) {
        if (s_slots[i].btn == btn) {
            return i;
        }
    }
    return -1;
}

/* Find a free slot index (btn == NULL), or -1 if pool exhausted. */
static int find_free_slot(void) {
    for (int i = 0; i < WINK_BUTTON_HELPER_MAX; i++) {
        if (s_slots[i].btn == NULL) {
            return i;
        }
    }
    return -1;
}

/* ── periodic callback (LIGHT path — void return, void* ctx) ─── */
static void btn_poll_tick(void *arg) {
    /* ADR-0017 layer 3: if a DAL blocking API is called from within this
     * LIGHT (soft-timer) dispatch, WINK_ASSERT_NONBLOCKING escalates to a
     * fault (WINK_FAULT_LIGHT_BLOCKING=8006) under WINK_PT_DEBUG builds. */
    WINK_ASSERT_NONBLOCKING();

    uint64_t t_start = pal_os_get_us();
    dal_button_t *btn = (dal_button_t *)arg;
    /* Transient poll errors are non-fatal (next tick retries); deliberately
     * discard them here to keep the callback non-blocking and to match the
     * documented contract that the periodic callback keeps running. */
    WINK_IGNORE_RESULT(dal_button_poll(btn));
    uint64_t t_elapsed = pal_os_get_us() - t_start;

    /* Execution-time budget watchdog (P1-3 Step 5): if the user's event
     * callback (fired synchronously inside dal_button_poll — e.g. short-
     * press / long-press handlers) does heavy work, we LOG_W to surface the
     * contract violation rather than silently starving other timers/app_loop. */
    if (t_elapsed > WINK_BTN_POLL_BUDGET_US) {
        LOG_W("button poll took %llu us (budget=%u us); event callback may be blocking",
              (unsigned long long)t_elapsed, (unsigned)WINK_BTN_POLL_BUDGET_US);
    }
}

/* ── public API ──────────────────────────────────────────────── */

wink_status_t wink_button_helper_start(dal_button_t *btn, uint32_t poll_ms)
{
    if (btn == NULL || poll_ms == 0u) {
        LOG_D("start: invalid arg (btn=%p poll_ms=%u)",
              (void *)btn, (unsigned)poll_ms);
        return WINK_ERR_INVALID_ARG;
    }

    if (find_slot_by_btn(btn) >= 0) {
        LOG_D("start: btn=%p already auto-polled", (void *)btn);
        return WINK_ERR_INVALID_STATE;
    }

    int free_idx = find_free_slot();
    if (free_idx < 0) {
        LOG_D("start: out of button slots (%d)", WINK_BUTTON_HELPER_MAX);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    /* Preflight probe: fail fast if the button is not initialised. This
     * mirrors the blink/servo preflight pattern (see wink_blink_helper.c:136,
     * wink_servo_helper.c:121) so callers get NOT_INITIALIZED at start()
     * rather than a silent forever-ticking periodic against an un-inited
     * device ("blinking in the void"). Use is_pressed() which is non-
     * mutating (no debounce state change). BUSY/other transient errors are
     * tolerated — the periodic tick will retry next poll. */
    bool pressed = false;
    wink_status_t probe_st = dal_button_is_pressed(btn, &pressed);
    if (probe_st == WINK_ERR_NOT_INITIALIZED) {
        LOG_D("start: button not initialized");
        return WINK_ERR_NOT_INITIALIZED;
    }

    btn_ctx_t *ctx = &s_slots[free_idx];
    ctx->btn      = btn;
    ctx->period_h = WINK_PERIODIC_INVALID;

    /* Button poll is pure non-blocking GPIO + debounce FSM → LIGHT path.
     * Priority 2 = background polling (default). Core = ANY (no pinning). */
    wink_periodic_handle_t h = wink_periodic_start_ex(
        "btn_poll",
        0u,                  /* stack_hint: ignored for LIGHT */
        poll_ms,
        btn_poll_tick,
        btn,
        WINK_PERIODIC_LIGHT,
        WINK_PERIODIC_DEFAULT_PRIORITY,
        PAL_OS_CORE_ANY);
    if (h < 0) {
        LOG_D("start: periodic_start failed: %d", (int)h);
        ctx->btn = NULL; /* roll back slot allocation */
        return (wink_status_t)h;
    }

    ctx->period_h = h;
    return WINK_OK;
}

wink_status_t wink_button_helper_stop(dal_button_t *btn)
{
    if (btn == NULL) {
        return WINK_OK;
    }
    int idx = find_slot_by_btn(btn);
    if (idx < 0) {
        return WINK_OK;  /* not tracked: no-op */
    }

    btn_ctx_t *ctx = &s_slots[idx];
    wink_periodic_stop(ctx->period_h);
    ctx->period_h = WINK_PERIODIC_INVALID;
    ctx->btn      = NULL;   /* mark slot free for reuse */
    return WINK_OK;
}
