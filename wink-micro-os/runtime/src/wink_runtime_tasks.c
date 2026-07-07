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
            /* Wake-up semaphore: posted on period change OR stop so the
             * sleeping task body can immediately recompute its next wake
             * anchor (long-to-short change fires promptly; stop does not
             * have to wait out the current sleep). Always binary (count
             * resets to 0 on every wait iteration). */
            pal_os_sem_t wake_sem;
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

        /* Absolute-time anchor scheduling (same drift-free approach as
         * before), BUT use wake_sem instead of pal_os_sleep_ms so that
         * period changes / stop requests can wake us immediately (ADR-0023
         * §8: xTaskAbortDelay / fiber-wake equivalent via sem give).
         *
         * After wakeup (timeout OR signaled), we re-read e->period_ms
         * which may have been updated by wink_periodic_change_period().
         * Recompute the anchor so long→short changes fire promptly and
         * short→long changes extend the next wait without drift. */
        next_wake += (uint64_t)e->period_ms * 1000ULL;
        uint64_t now = pal_os_get_us();
        int64_t delta_us = (int64_t)(next_wake - now);
        if (delta_us <= 0) {
            /* Missed a tick (or many) — reset anchor to avoid catching up. */
            next_wake = now;
            /* Tiny sleep to yield rather than busy-loop; wake_sem take with
             * 1ms timeout also lets stop/change_period wake us. */
            if (e->u.task.wake_sem != NULL) {
                WINK_IGNORE_UNUSED(pal_os_sem_take(e->u.task.wake_sem, 1u));
            } else {
                pal_os_sleep_ms(1);
            }
        } else {
            uint32_t delta_ms = (uint32_t)((delta_us + 999ULL) / 1000ULL); /* ceil */
            if (e->u.task.wake_sem != NULL) {
                WINK_IGNORE_UNUSED(pal_os_sem_take(e->u.task.wake_sem, delta_ms));
            } else {
                pal_os_sleep_ms(delta_ms);
            }
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
        e->u.task.wake_sem = pal_os_sem_create();
        e->u.task.done_sem = pal_os_sem_create();
        if (e->u.task.wake_sem == NULL || e->u.task.done_sem == NULL) {
            if (e->u.task.wake_sem) pal_os_sem_destroy(e->u.task.wake_sem);
            if (e->u.task.done_sem) pal_os_sem_destroy(e->u.task.done_sem);
            memset(e, 0, sizeof(*e));
            return (wink_periodic_handle_t)WINK_ERR_RESOURCE_EXHAUSTED;
        }
        wink_status_t st = pal_os_task_create(
            periodic_task_fn, (char *)name, e->u.task.stack_bytes,
            e, e->u.task.priority, e->u.task.core, &e->u.task.task_handle);
        if (wink_status_is_error(st)) {
            pal_os_sem_destroy(e->u.task.wake_sem);
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
        /* Attach diagnostic name so LIGHT WCET/blocking faults can log
         * the offending helper (Task 1.5 / ADR-0023 §9). */
        wink_soft_timer_set_name(h, name);
        /* soft_timer_create leaves the timer in STOPPED state (active=0);
         * explicitly start it so dispatch() begins counting ticks down. */
        wink_status_t start_st = wink_soft_timer_start(h);
        if (wink_status_is_error(start_st)) {
            WINK_IGNORE_UNUSED(wink_soft_timer_destroy(h));
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
            /* destroy (not just stop) so the soft_timer slot is freed back to
             * the pool — stop() only sets active=0 and leaks the slot. */
            WINK_IGNORE_UNUSED(wink_soft_timer_destroy(e->u.soft_timer_handle));
        }
    } else if (e->kind == PERIODIC_ENTRY_TASK) {
        e->u.task.stop_requested = true;
        /* Wake the sleeping task so it observes stop_requested promptly
         * (no waiting out the current sleep). Safe even if wake_sem is
         * already signaled — binary sem, extra gives are benign. */
        if (e->u.task.wake_sem != NULL) {
            WINK_IGNORE_UNUSED(pal_os_sem_give(e->u.task.wake_sem));
        }
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
        if (e->u.task.wake_sem != NULL) {
            pal_os_sem_destroy(e->u.task.wake_sem);
            e->u.task.wake_sem = NULL;
        }
    }
    memset(e, 0, sizeof(*e));
}

uint32_t wink_periodic_active_count(void) {
    uint32_t n = 0;
    for (int i = 0; i < WINK_MAX_PERIODIC; i++) {
        if (s_periodic[i].kind != PERIODIC_ENTRY_FREE) {
            n++;
        }
    }
    return n;
}

wink_status_t wink_periodic_change_period(wink_periodic_handle_t h, uint32_t period_ms) {
    /* Match wink_periodic_stop() tolerance: INVALID / error-code handles
     * (h <= 0) are silent no-ops?  No — change_period has a return value
     * and callers check success, so we report INVALID_ARG for sentinel/
     * error handles rather than silently swallowing. */
    if (h <= 0) return WINK_ERR_INVALID_ARG;
    if (period_ms == 0) return WINK_ERR_INVALID_ARG;

    int slot = (int)(h - 1);
    if (slot < 0 || slot >= WINK_MAX_PERIODIC) return WINK_ERR_INVALID_ARG;
    periodic_entry_t *e = &s_periodic[slot];
    if (e->kind == PERIODIC_ENTRY_FREE) return WINK_ERR_INVALID_ARG;

    if (e->kind == PERIODIC_ENTRY_LIGHT) {
        /* LIGHT path: soft_timer handles period rounding internally. */
        return wink_soft_timer_change_period(e->u.soft_timer_handle, period_ms);
    }

    /* MAY_BLOCK task path:
     *   1. Update e->period_ms (task loop re-reads it after wake).
     *   2. Signal wake_sem → sem_take in task body returns; loop re-reads
     *      period_ms, recomputes next_wake, and re-sleeps with the new
     *      delta. Long→short wakes immediately; short→long also wakes
     *      but simply re-sleeps with the longer delta (one extra wake
     *      is negligible for this API's expected call frequency).
     *
     * No critical section needed: period_ms is read/written from two
     * contexts (setter + task body) but only with native 32-bit aligned
     * stores/loads, and the task re-reads after wake and uses a fresh
     * delta — tearing a uint32_t is not possible on any supported
     * target. The wake_sem post after the store guarantees the task
     * eventually picks up the new value (ADR-0014 single-virtual-core
     * host/wasm: sem_give synchronously queues the fiber; ESP32 SMP:
     * binary sem posts with proper memory visibility). */
    e->period_ms = period_ms;
    if (e->u.task.wake_sem != NULL) {
        WINK_IGNORE_UNUSED(pal_os_sem_give(e->u.task.wake_sem));
    }
    return WINK_OK;
}
