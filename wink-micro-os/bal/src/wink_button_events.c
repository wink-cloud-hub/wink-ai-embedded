/**
 * @file wink_button_events.c
 * @brief BAL button event stream — soft-poll implementation + IRQ dispatch shim (S3).
 *
 * This file owns the slot pool, the periodic LIGHT tick, the shared event
 * dispatch state machine, and start/stop routing. The GPIO-IRQ backend
 * lives in wink_button_events_irq.c (target-gated) and shares state
 * through wink_button_events_internal.h.
 *
 * S2 shipped the soft-poll body here; S3 factors the PRESS/RELEASE/
 * LONG_PRESS state machine into wink_button_events_dispatch_stable() so
 * both the poll tick and the IRQ debounce timeout can drive the same
 * event postings.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "bal.btn"

#include "wink_button_events.h"
#include "wink_button_events_internal.h"
#include "wink_tasks.h"
#include "wink_status.h"
#include "wink_event.h"
#include "wink_fault.h"      /* WINK_WARN_BUTTON_IRQ_DEGRADED */
#include "wink_trace.h"      /* wink_trace_warn() (S4 degrade signal) */
#include "wink_pt_debug.h"  /* WINK_ASSERT_NONBLOCKING() (ADR-0017 layer 3) */
#include "pal_log.h"
#include "pal_osal.h"       /* pal_os_get_us() / pal_os_get_ms() for state machine + budget */

#include <stddef.h>

/* LIGHT callback execution-time budget (P1-3 Step 5): see wink_button_
 * helper.c predecessor for the rationale — same 100 us envelope. */
#define WINK_BTN_POLL_BUDGET_US  100u

/* Slot pool (definition; internal header has `extern` decl). */
button_event_slot_t g_button_event_slots[WINK_BUTTON_EVENTS_MAX];

/* ── internal helpers ─────────────────────────────────────────── */

int wink_button_events_find_slot(const dal_button_t *btn) {
    for (int i = 0; i < WINK_BUTTON_EVENTS_MAX; i++) {
        if (g_button_event_slots[i].btn == btn) {
            return i;
        }
    }
    return -1;
}

/* Find a free slot index (btn == NULL), or -1 if pool exhausted. */
static int find_free_slot(void) {
    for (int i = 0; i < WINK_BUTTON_EVENTS_MAX; i++) {
        if (g_button_event_slots[i].btn == NULL) {
            return i;
        }
    }
    return -1;
}

/* ── shared event dispatch (task context, called by poll tick + IRQ deb) ── */

/* Post one WINK_EVENT_BUTTON_* to the runtime event queue. Also invokes
 * the slot's captured orig_cb (if any) — this is the shim that used to
 * live inline in the S2 dispatch adapter. */
static void post_button_event(button_event_slot_t *s, dal_button_event_t evt,
                              uint64_t now_ms)
{
    if (s->orig_cb != NULL) {
        s->orig_cb(evt, s->orig_cb_ctx);
    }
    wink_event_t event;
    event.device    = s->btn;
    event.timestamp = now_ms;
    event.param     = (uint32_t)evt;
    switch (evt) {
        case DAL_BUTTON_EVT_PRESS:      event.type = WINK_EVENT_BUTTON_PRESSED;    break;
        case DAL_BUTTON_EVT_RELEASE:    event.type = WINK_EVENT_BUTTON_RELEASED;   break;
        case DAL_BUTTON_EVT_LONG_PRESS: event.type = WINK_EVENT_BUTTON_LONG_PRESS; break;
        default: return;
    }
    WINK_IGNORE_RESULT(wink_event_post(&event));
}

/* Forward decl: long-press timer cancel helper (implemented in IRQ TU
 * when ESP_PLATFORM, otherwise a no-op stub weakly linked). */
extern void wink_button_events_irq_cancel_longpress(button_event_slot_t *s);
extern void wink_button_events_irq_arm_longpress(button_event_slot_t *s);

void wink_button_events_dispatch_stable(button_event_slot_t *s,
                                        bool stable_pressed,
                                        uint64_t now_ms)
{
    /* Edge detection: only fire on transitions. */
    if (stable_pressed != s->last_pressed) {
        s->last_pressed = stable_pressed;
        if (stable_pressed) {
            /* Fresh press: clear long-press latch and arm the long-press
             * timer if we have one (IRQ backend); poll backend handles
             * long-press through DAL's own poll-driven timer. */
            s->long_press_fired = false;
            post_button_event(s, DAL_BUTTON_EVT_PRESS, now_ms);
            if (s->drive == WINK_BUTTON_DRIVE_GPIO_IRQ) {
                wink_button_events_irq_arm_longpress(s);
            }
        } else {
            /* Release: cancel pending long-press. */
            if (s->drive == WINK_BUTTON_DRIVE_GPIO_IRQ) {
                wink_button_events_irq_cancel_longpress(s);
            }
            s->long_press_fired = false;
            post_button_event(s, DAL_BUTTON_EVT_RELEASE, now_ms);
        }
    }
}

void wink_button_events_dispatch_long_press(button_event_slot_t *s)
{
    /* Called from the IRQ long-press one-shot timer callback. Only fire
     * if the slot is still held (in case a release raced the timer). */
    if (!s->last_pressed || s->long_press_fired) { return; }
    s->long_press_fired = true;
    post_button_event(s, DAL_BUTTON_EVT_LONG_PRESS, pal_os_get_ms());
}

/* ── periodic callback (LIGHT path — SOFT_POLL) ────────────────
 * NOTE: For SOFT_POLL we still delegate to dal_button_poll(), which runs
 * its own event callback (registered via dal_button_on_event) to drive
 * event posting. We deliberately do NOT double-dispatch by also calling
 * wink_button_events_dispatch_stable here — that would duplicate PRESS
 * events. The registered DAL callback (button_events_dispatch_dal_cb) is
 * the bridge; it forwards each DAL event into the same post_button_event
 * used by the IRQ path. */
static void button_events_dispatch_dal_cb(dal_button_event_t evt, void *ctx) {
    dal_button_t *btn = (dal_button_t *)ctx;
    int idx = wink_button_events_find_slot(btn);
    if (idx < 0) { return; }
    button_event_slot_t *s = &g_button_event_slots[idx];
    /* Keep last_pressed in sync so LONG_PRESS-cancel-on-release logic
     * (if any listener wants it) works cross-backend. */
    if (evt == DAL_BUTTON_EVT_PRESS)   { s->last_pressed = true;  s->long_press_fired = false; }
    if (evt == DAL_BUTTON_EVT_RELEASE) { s->last_pressed = false; s->long_press_fired = false; }
    if (evt == DAL_BUTTON_EVT_LONG_PRESS) { s->long_press_fired = true; }
    post_button_event(s, evt, pal_os_get_ms());
}

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

/* ── S2 degrade helper: pick an effective soft_poll period ─────
 * When cfg->drive == GPIO_IRQ but the runtime cannot yet honour an IRQ
 * path (S3 pending on non-ESP32 targets), we fall back to periodic polling.
 * If the JSON did not supply auto_poll_ms (0), pick something reasonable
 * derived from debounce_ms so the fallback is not silently sluggish.
 * Formula matches S4 default: max(debounce_ms, 10 ms).
 *
 * Gated out under WINK_BUTTON_IRQ_STRICT (S4): strict builds hard-fail
 * the misconfiguration instead of degrading, so this helper has no
 * remaining call sites — leaving the definition would trip
 * -Wunused-function under -Werror. */
#ifndef WINK_BUTTON_IRQ_STRICT
static uint32_t effective_poll_ms(const wink_button_event_config_t *cfg) {
    if (cfg->auto_poll_ms != 0u) {
        return cfg->auto_poll_ms;
    }
    return (cfg->debounce_ms > 10u) ? cfg->debounce_ms : 10u;
}
#endif

/* ── public API ──────────────────────────────────────────────── */

wink_status_t wink_button_enable_events(dal_button_t *btn,
                                        const wink_button_event_config_t *cfg)
{
    if (btn == NULL || cfg == NULL) {
        LOG_D("enable: invalid arg (btn=%p cfg=%p)", (void *)btn, (const void *)cfg);
        return WINK_ERR_INVALID_ARG;
    }

    /* Surface documented no-op knobs so users don't wonder why deep-sleep
     * wake isn't happening (per-start, single line — this is documented
     * future work per ADR-0031; the RTC-GPIO PAL API is pending). */
    if (cfg->wake_from_sleep) {
        LOG_I("wake_from_sleep is not yet wired; ignoring (RTC GPIO API pending)");
    }

    /* S4: honour GPIO_IRQ only when the target actually supports it. Non-
     * ESP32 targets return false from irq_supported() — under the default
     * (permissive) build we degrade to SOFT_POLL and raise the
     * `WINK_WARN_BUTTON_IRQ_DEGRADED` warn code so telemetry/CI can see the
     * fallback happened. Under `-DWINK_BUTTON_IRQ_STRICT` (opt-in CI knob)
     * the same misconfiguration hard-fails with `WINK_ERR_UNSUPPORTED`
     * instead so codegen contracts can be enforced at build time. */
    bool use_irq = false;
    uint32_t poll_ms = 0;
    if (cfg->drive == WINK_BUTTON_DRIVE_GPIO_IRQ) {
        if (wink_button_events_irq_supported()) {
            use_irq = true;
        } else {
#ifdef WINK_BUTTON_IRQ_STRICT
            LOG_D("gpio_irq requested but unsupported on this target (STRICT)");
            return WINK_ERR_UNSUPPORTED;
#else
            /* Degrade path: signal via warn code first (observable in tests
             * and telemetry), then compute effective poll_ms and fall
             * through to the existing SOFT_POLL arming block below. */
            wink_trace_warn(WINK_WARN_BUTTON_IRQ_DEGRADED);
            LOG_D("gpio_irq unsupported on this target; degrading to soft_poll");
            poll_ms = effective_poll_ms(cfg);
#endif
        }
    } else {
        /* SOFT_POLL: auto_poll_ms is mandatory (codegen already validates,
         * belt-and-braces here for hand-authored callers). */
        if (cfg->auto_poll_ms == 0u) {
            LOG_D("enable: soft_poll with auto_poll_ms=0");
            return WINK_ERR_INVALID_ARG;
        }
        poll_ms = cfg->auto_poll_ms;
    }

    if (wink_button_events_find_slot(btn) >= 0) {
        LOG_D("enable: btn=%p already tracked", (void *)btn);
        return WINK_ERR_INVALID_STATE;
    }

    int free_idx = find_free_slot();
    if (free_idx < 0) {
        LOG_D("enable: out of button event slots (%d)", WINK_BUTTON_EVENTS_MAX);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    /* Preflight probe: fail fast if the button is not initialised.
     * Uses is_pressed() (non-mutating). */
    bool pressed = false;
    wink_status_t probe_st = dal_button_is_pressed(btn, &pressed);
    if (probe_st == WINK_ERR_NOT_INITIALIZED) {
        LOG_D("enable: button not initialized");
        return WINK_ERR_NOT_INITIALIZED;
    }

    button_event_slot_t *ctx = &g_button_event_slots[free_idx];
    ctx->btn          = btn;
    ctx->period_h     = WINK_PERIODIC_INVALID;
    ctx->orig_cb      = btn->event_cb;
    ctx->orig_cb_ctx  = btn->event_cb_ctx;
    ctx->drive        = use_irq ? WINK_BUTTON_DRIVE_GPIO_IRQ
                                : WINK_BUTTON_DRIVE_SOFT_POLL;
    ctx->debounce_ms  = cfg->debounce_ms;
    ctx->long_press_ms = 0u; /* filled by IRQ arm; poll backend uses DAL's own */
    ctx->irq_debounce_h  = -1;
    ctx->irq_longpress_h = -1;
    ctx->last_pressed    = pressed;
    ctx->long_press_fired = false;

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

    if (use_irq) {
        /* IRQ backend: arm hardware ISR + timers. No periodic poll. */
        wink_status_t arm_st = wink_button_events_irq_arm(ctx, cfg);
        if (wink_status_is_error(arm_st)) {
            LOG_D("enable: irq_arm failed: %d — rolling back to slot-free", (int)arm_st);
            ctx->btn = NULL;
            return arm_st;
        }
        /* wake_from_sleep: TODO(S3 follow-up) — RTC-GPIO PAL API not wired. */
        return WINK_OK;
    }

    /* SOFT_POLL branch: register the DAL event callback that forwards each
     * DAL event onto the event queue, then start the periodic tick. */
    probe_st = dal_button_on_event(btn, button_events_dispatch_dal_cb, btn);
    if (wink_status_is_error(probe_st)) {
        LOG_D("enable: failed to register event callback: %d", (int)probe_st);
        ctx->btn = NULL;
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
        LOG_D("enable: periodic_start failed: %d", (int)h);
        WINK_IGNORE_RESULT(dal_button_on_event(btn, ctx->orig_cb, ctx->orig_cb_ctx));
        ctx->btn = NULL;
        return (wink_status_t)h;
    }

    ctx->period_h = h;
    /* Reflect POLL backend at DAL level (informational; DAL only branches on IRQ). */
    dal_button_set_event_backend(btn, DAL_BUTTON_BACKEND_POLL);
    return WINK_OK;
}

void wink_button_disable_events(dal_button_t *btn)
{
    if (btn == NULL) {
        return;
    }
    int idx = wink_button_events_find_slot(btn);
    if (idx < 0) {
        return;  /* not tracked: no-op */
    }

    button_event_slot_t *ctx = &g_button_event_slots[idx];
    if (ctx->drive == WINK_BUTTON_DRIVE_GPIO_IRQ) {
        wink_button_events_irq_disarm(ctx);
    } else {
        /* SOFT_POLL: unregister callback, stop periodic tick. */
        WINK_IGNORE_RESULT(dal_button_on_event(btn, ctx->orig_cb, ctx->orig_cb_ctx));
        wink_periodic_stop(ctx->period_h);
        dal_button_set_event_backend(btn, DAL_BUTTON_BACKEND_NONE);
    }
    ctx->period_h     = WINK_PERIODIC_INVALID;
    ctx->btn          = NULL;   /* mark slot free for reuse */
    ctx->orig_cb      = NULL;
    ctx->orig_cb_ctx  = NULL;
    ctx->drive        = WINK_BUTTON_DRIVE_SOFT_POLL;
    ctx->irq_debounce_h  = -1;
    ctx->irq_longpress_h = -1;
}
