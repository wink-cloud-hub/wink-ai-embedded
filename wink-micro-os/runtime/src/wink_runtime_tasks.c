/**
 * @file wink_runtime_tasks.c
 * @brief Periodic callback dispatch (wink_periodic_start / wink_periodic_stop).
 *
 * Routes LIGHT callbacks into the soft-timer subsystem and MAY_BLOCK
 * callbacks into dedicated PAL tasks.  Uses absolute-time scheduling to
 * avoid cumulative drift on the task path.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#include "wink_tasks.h"
#include "wink_soft_timer.h"
#include "wink_status.h"
#include "pal_osal.h"
#include <string.h>

/* WINK_IGNORE_UNUSED is in wink_status.h (pulled in via wink_tasks.h
 * transitively from wink_status.h include chain above). */

/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING 的 pal_os_task_create /
 * pal_os_sleep_ms（spawned task body）。抑制 -Wdeprecated-declarations。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define WINK_MAX_PERIODIC        12
#define WINK_PERIODIC_STACK_DEFAULT 2048

typedef enum {
    PERIODIC_ENTRY_FREE = 0,
    PERIODIC_ENTRY_LIGHT,       /* soft-timer based */
    PERIODIC_ENTRY_TASK,        /* dedicated PAL task */
} periodic_entry_kind_t;

typedef struct {
    periodic_entry_kind_t kind;
    const char *name;
    void (*fn)(void *ctx);
    void *ctx;
    uint32_t period_ms;
    union {
        int32_t soft_timer_handle;        /* for LIGHT */
        struct {                          /* for TASK */
            uint32_t stack_bytes;
            int32_t priority;
            pal_os_core_id_t core;
            pal_os_task_handle_t task_handle;
            volatile bool stop_requested;
            /* One-shot "done" sem: posted by the task body just before it
             * returns; wink_periodic_stop() waits on it before clearing the
             * entry so we never memset a struct the task still touches. */
            pal_os_sem_t done_sem;
        } task;
    } u;
} periodic_entry_t;

static periodic_entry_t s_periodic[WINK_MAX_PERIODIC];

/* ── LIGHT path (soft-timer callback) ────────────────────── */
static wink_status_t periodic_light_cb(void *arg) {
    periodic_entry_t *e = (periodic_entry_t *)arg;
    if (e->fn != NULL) {
        e->fn(e->ctx);
    }
    return WINK_OK; /* keep timer running */
}

/* ── TASK path (dedicated preemptive task) ───────────────── */
static void periodic_task_fn(void *arg) {
    periodic_entry_t *e = (periodic_entry_t *)arg;
    uint64_t next_wake = pal_os_get_us();
    while (!e->u.task.stop_requested) {
        if (e->fn != NULL) {
            e->fn(e->ctx);
        }
        next_wake += (uint64_t)e->period_ms * 1000ULL;
        uint64_t now = pal_os_get_us();
        int64_t delta_us = (int64_t)(next_wake - now);
        if (delta_us <= 0) {
            /* Missed a tick (or many) — reset anchor to avoid catching up. */
            next_wake = now;
            pal_os_sleep_ms(1);
        } else {
            pal_os_sleep_ms((uint32_t)(delta_us / 1000ULL));
        }
    }
    /* Signal wink_periodic_stop() that this task will no longer touch *e. */
    if (e->u.task.done_sem != NULL) {
        wink_status_t give_st = pal_os_sem_give(e->u.task.done_sem);
        (void)give_st;
    }
    pal_os_task_delete(NULL);
}

/* ── Public API ──────────────────────────────────────────── */

wink_periodic_handle_t wink_periodic_start_ex(
    const char *name,
    uint32_t stack_hint,
    uint32_t period_ms,
    void (*fn)(void *ctx),
    void *ctx,
    uint32_t flags,
    int32_t priority,
    pal_os_core_id_t core)
{
    if (name == NULL || fn == NULL || period_ms == 0) {
        return (wink_periodic_handle_t)WINK_ERR_INVALID_ARG;
    }

    /* Decide execution model if flags don't force one. */
    bool force_light = (flags & WINK_PERIODIC_LIGHT) != 0;
    bool force_task  = (flags & WINK_PERIODIC_MAY_BLOCK) != 0;
    bool use_task;
    if (force_light && force_task) {
        return (wink_periodic_handle_t)WINK_ERR_INVALID_ARG;
    }
    if (force_task) {
        use_task = true;
    } else if (force_light) {
        use_task = false;
    } else {
        /* Heuristic: periods < 50ms → LIGHT (timer) to avoid many tasks;
         * periods >= 50ms OR stack_hint >= 2048 → dedicated task. */
        use_task = (period_ms >= 50) || (stack_hint >= 2048);
    }

    /* Find free slot. */
    int slot = -1;
    for (int i = 0; i < WINK_MAX_PERIODIC; i++) {
        if (s_periodic[i].kind == PERIODIC_ENTRY_FREE) { slot = i; break; }
    }
    if (slot < 0) return (wink_periodic_handle_t)WINK_ERR_RESOURCE_EXHAUSTED;

    periodic_entry_t *e = &s_periodic[slot];
    memset(e, 0, sizeof(*e));
    e->name      = name;
    e->fn        = fn;
    e->ctx       = ctx;
    e->period_ms = period_ms;

    if (use_task) {
        e->kind = PERIODIC_ENTRY_TASK;
        e->u.task.stack_bytes  = stack_hint ? stack_hint : WINK_PERIODIC_STACK_DEFAULT;
        e->u.task.priority     = priority;
        e->u.task.core         = core;
        e->u.task.stop_requested = false;
        e->u.task.done_sem = pal_os_sem_create();
        if (e->u.task.done_sem == NULL) {
            memset(e, 0, sizeof(*e));
            return (wink_periodic_handle_t)WINK_ERR_RESOURCE_EXHAUSTED;
        }
        wink_status_t st = pal_os_task_create(
            periodic_task_fn, (char *)name, e->u.task.stack_bytes,
            e, e->u.task.priority, e->u.task.core, &e->u.task.task_handle);
        if (wink_status_is_error(st)) {
            pal_os_sem_destroy(e->u.task.done_sem);
            memset(e, 0, sizeof(*e));
            return (wink_periodic_handle_t)st;
        }
    } else {
        e->kind = PERIODIC_ENTRY_LIGHT;
        /* soft_timer period must be multiple of tick; round up. */
        uint32_t ticks = (period_ms + WINK_RUNTIME_TICK_MS - 1) / WINK_RUNTIME_TICK_MS;
        int32_t h = wink_soft_timer_create(periodic_light_cb, e,
                                           WINK_TIMER_PERIODIC,
                                           ticks * WINK_RUNTIME_TICK_MS);
        if (h < 0) {
            memset(e, 0, sizeof(*e));
            return (wink_periodic_handle_t)WINK_ERR_RESOURCE_EXHAUSTED;
        }
        /* soft_timer_create leaves the timer in STOPPED state (active=0);
         * explicitly start it so dispatch() begins counting ticks down. */
        wink_status_t start_st = wink_soft_timer_start(h);
        if (wink_status_is_error(start_st)) {
            wink_status_t stop_st = wink_soft_timer_stop(h);
            (void)stop_st;
            memset(e, 0, sizeof(*e));
            return (wink_periodic_handle_t)start_st;
        }
        e->u.soft_timer_handle = h;
    }

    /* handle == slot+1 so we never return 0 (0 is reserved as INVALID). */
    return (wink_periodic_handle_t)(slot + 1);
}

void wink_periodic_stop(wink_periodic_handle_t h) {
    if (h <= 0) return;
    int slot = (int)(h - 1);
    if (slot < 0 || slot >= WINK_MAX_PERIODIC) return;
    periodic_entry_t *e = &s_periodic[slot];
    if (e->kind == PERIODIC_ENTRY_FREE) return;

    if (e->kind == PERIODIC_ENTRY_LIGHT) {
        if (e->u.soft_timer_handle >= 0) {
            WINK_IGNORE_UNUSED(wink_soft_timer_stop(e->u.soft_timer_handle));
        }
    } else if (e->kind == PERIODIC_ENTRY_TASK) {
        e->u.task.stop_requested = true;
        /* The task body runs on another thread (and possibly another core on
         * ESP32). Wait until it acknowledges via done_sem before we clear the
         * entry — otherwise it may still read e->stop_requested / e->fn on
         * wake-up and touch freed/zeroed memory. */
        if (e->u.task.done_sem != NULL) {
            wink_status_t take_st = pal_os_sem_take(e->u.task.done_sem, 500u);
            (void)take_st; /* best-effort; on timeout we accept the risk. */
            pal_os_sem_destroy(e->u.task.done_sem);
            e->u.task.done_sem = NULL;
        }
    }
    memset(e, 0, sizeof(*e));
}
