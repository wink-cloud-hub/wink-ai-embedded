/**
 * @file wink_button_events.c
 * @brief BAL button event stream — soft-poll implementation (S2).
 *
 * This file owns the slot pool, the periodic LIGHT tick, and the event
 * dispatch shim that translates DAL edge events into `WINK_EVENT_BUTTON_*`
 * postings on the runtime event queue.
 *
 * The soft-poll path is exactly the logic that used to live in
 * `wink_button_helper.c`; `wink_button_helper_*` is now a thin wrapper
 * that builds a SOFT_POLL cfg and calls `wink_button_events_start`.
 *
 * GPIO IRQ path (S3): when cfg->drive == GPIO_IRQ, this file currently
 * degrades to a soft-poll path so cross-target samples keep working.
 * The real IRQ registration and the strict-mode warn (WINK_WARN_BUTTON_
 * IRQ_DEGRADED) land in S3/S4.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "bal.btn"

#include "wink_button_events.h"
#include "wink_tasks.h"
#include "wink_status.h"
#include "wink_event.h"
#include "wink_pt_debug.h"  /* WINK_ASSERT_NONBLOCKING() (ADR-0017 layer 3) */
#include "pal_log.h"
#include "pal_osal.h"       /* pal_os_get_us() for execution-time budget watchdog */

#include <stddef.h>

/* LIGHT callback execution-time budget (P1-3 Step 5): see wink_button_
 * helper.c predecessor for the rationale — same 100 us envelope. */
#define WINK_BTN_POLL_BUDGET_US  100u

/* ── per-button slot ────────────────────────────────────────────
 * A slot with btn == NULL is free. period_h is only meaningful when
 * btn != NULL. BSS zero-init gives us btn=NULL for free-slot state. */
typedef struct {
    dal_button_t           *btn;
    wink_periodic_handle_t  period_h;
    dal_button_event_cb     orig_cb;
    void                   *orig_cb_ctx;
} button_event_slot_t;

static button_event_slot_t s_slots[WINK_BUTTON_EVENTS_MAX];

/* ── internal helpers ─────────────────────────────────────────── */

/* Find slot index currently tracking @p btn, or -1 if not tracked. */
static int find_slot_by_btn(const dal_button_t *btn) {
    for (int i = 0; i < WINK_BUTTON_EVENTS_MAX; i++) {
        if (s_slots[i].btn == btn) {
            return i;
        }
    }
    return -1;
}

/* Find a free slot index (btn == NULL), or -1 if pool exhausted. */
static int find_free_slot(void) {
    for (int i = 0; i < WINK_BUTTON_EVENTS_MAX; i++) {
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

    /* Execution-time budget watchdog (P1-3 Step 5): surface user event
     * callbacks that do too much work in the LIGHT context. */
    if (t_elapsed > WINK_BTN_POLL_BUDGET_US) {
        LOG_W("button poll took %llu us (budget=%u us); event callback may be blocking",
              (unsigned long long)t_elapsed, (unsigned)WINK_BTN_POLL_BUDGET_US);
    }
}

/* ── button event posting callback ────────────────────────────── */
static void button_events_dispatch(dal_button_event_t evt, void *ctx) {
    dal_button_t *btn = (dal_button_t *)ctx;
    /* Safely invoke original callback if registered */
    int idx = find_slot_by_btn(btn);
    if (idx >= 0) {
        button_event_slot_t *slot = &s_slots[idx];
        if (slot->orig_cb != NULL) {
            slot->orig_cb(evt, slot->orig_cb_ctx);
        }
    }

    wink_event_t event;
    event.device = btn;
    event.timestamp = pal_os_get_ms();
    event.param = (uint32_t)evt;
    if (evt == DAL_BUTTON_EVT_PRESS) {
        event.type = WINK_EVENT_BUTTON_PRESSED;
    } else if (evt == DAL_BUTTON_EVT_RELEASE) {
        event.type = WINK_EVENT_BUTTON_RELEASED;
    } else if (evt == DAL_BUTTON_EVT_LONG_PRESS) {
        event.type = WINK_EVENT_BUTTON_LONG_PRESS;
    } else {
        return;
    }
    WINK_IGNORE_RESULT(wink_event_post(&event));
}

/* ── S2 degrade helper: pick an effective soft_poll period ─────
 * When cfg->drive == GPIO_IRQ but the runtime cannot yet honour an IRQ
 * path (S3 pending), we fall back to periodic polling. If the JSON did
 * not supply auto_poll_ms (0), pick something reasonable derived from
 * debounce_ms so the fallback is not silently sluggish.
 * Formula matches S4 default: max(debounce_ms, 10 ms). */
static uint32_t effective_poll_ms(const wink_button_event_config_t *cfg) {
    if (cfg->auto_poll_ms != 0u) {
        return cfg->auto_poll_ms;
    }
    return (cfg->debounce_ms > 10u) ? cfg->debounce_ms : 10u;
}

/* ── public API ──────────────────────────────────────────────── */

wink_status_t wink_button_events_start(dal_button_t *btn,
                                       const wink_button_event_config_t *cfg)
{
    if (btn == NULL || cfg == NULL) {
        LOG_D("start: invalid arg (btn=%p cfg=%p)", (void *)btn, (const void *)cfg);
        return WINK_ERR_INVALID_ARG;
    }

    /* S2 degrade frame: pretend GPIO_IRQ is SOFT_POLL. S4 adds the trace
     * warn WINK_WARN_BUTTON_IRQ_DEGRADED + strict-mode failure. */
    uint32_t poll_ms;
    if (cfg->drive == WINK_BUTTON_DRIVE_GPIO_IRQ) {
        LOG_D("gpio_irq requested but not yet available; degrading to soft_poll");
        poll_ms = effective_poll_ms(cfg);
    } else {
        /* SOFT_POLL: auto_poll_ms is mandatory (codegen already validates,
         * belt-and-braces here for hand-authored callers). */
        if (cfg->auto_poll_ms == 0u) {
            LOG_D("start: soft_poll with auto_poll_ms=0");
            return WINK_ERR_INVALID_ARG;
        }
        poll_ms = cfg->auto_poll_ms;
    }

    if (find_slot_by_btn(btn) >= 0) {
        LOG_D("start: btn=%p already tracked", (void *)btn);
        return WINK_ERR_INVALID_STATE;
    }

    int free_idx = find_free_slot();
    if (free_idx < 0) {
        LOG_D("start: out of button event slots (%d)", WINK_BUTTON_EVENTS_MAX);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    /* Preflight probe: fail fast if the button is not initialised.
     * Uses is_pressed() (non-mutating). */
    bool pressed = false;
    wink_status_t probe_st = dal_button_is_pressed(btn, &pressed);
    if (probe_st == WINK_ERR_NOT_INITIALIZED) {
        LOG_D("start: button not initialized");
        return WINK_ERR_NOT_INITIALIZED;
    }

    button_event_slot_t *ctx = &s_slots[free_idx];
    ctx->btn      = btn;
    ctx->period_h = WINK_PERIODIC_INVALID;
    ctx->orig_cb  = btn->event_cb;
    ctx->orig_cb_ctx = btn->event_cb_ctx;

    /* Apply cfg->debounce_ms to the DAL button (0 = leave DAL default alone
     * so callers that omit the field don't accidentally re-tune below the
     * historical 3-sample threshold). */
    if (cfg->debounce_ms > 0u) {
        wink_status_t db_st = dal_button_set_debounce_ms(btn, cfg->debounce_ms);
        if (wink_status_is_error(db_st)) {
            LOG_D("start: dal_button_set_debounce_ms failed: %d", (int)db_st);
            ctx->btn = NULL; /* roll back slot allocation */
            return db_st;
        }
    }

    /* Register dispatch callback that posts events to the event queue. */
    probe_st = dal_button_on_event(btn, button_events_dispatch, btn);
    if (wink_status_is_error(probe_st)) {
        LOG_D("start: failed to register event callback: %d", (int)probe_st);
        ctx->btn = NULL; /* roll back slot allocation */
        return probe_st;
    }

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
        /* Best-effort unregister so we don't leave the dispatch shim
         * pointing at a slot we just released. */
        WINK_IGNORE_RESULT(dal_button_on_event(btn, ctx->orig_cb, ctx->orig_cb_ctx));
        ctx->btn = NULL; /* roll back slot allocation */
        return (wink_status_t)h;
    }

    ctx->period_h = h;
    /* wake_from_sleep: TODO(S3) — deep-sleep wake pending RTC GPIO API. */
    return WINK_OK;
}

void wink_button_events_stop(dal_button_t *btn)
{
    if (btn == NULL) {
        return;
    }
    int idx = find_slot_by_btn(btn);
    if (idx < 0) {
        return;  /* not tracked: no-op */
    }

    button_event_slot_t *ctx = &s_slots[idx];
    /* Unregister and restore original callback */
    WINK_IGNORE_RESULT(dal_button_on_event(btn, ctx->orig_cb, ctx->orig_cb_ctx));
    wink_periodic_stop(ctx->period_h);
    ctx->period_h = WINK_PERIODIC_INVALID;
    ctx->btn      = NULL;   /* mark slot free for reuse */
}
