// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_runtime_tasks.c
 * @brief Periodic callback dispatch (wink_periodic_start / wink_periodic_stop).
 */
#include "wink_tasks.h"
#include "wink_soft_timer.h"
#include "wink_status.h"
#include "pal_osal.h"
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define WINK_MAX_PERIODIC        12
#define WINK_PERIODIC_STACK_DEFAULT 2048

typedef enum {
    PERIODIC_ENTRY_FREE = 0,
    PERIODIC_ENTRY_LIGHT,
    PERIODIC_ENTRY_TASK,
} periodic_entry_kind_t;

typedef struct {
    periodic_entry_kind_t kind;
    const char *name;
    void (*fn)(void *ctx);
    void *ctx;
    uint32_t period_ms;
    union {
        int32_t soft_timer_handle;
        struct {
            uint32_t stack_bytes;
            int32_t priority;
            pal_os_core_id_t core;
            pal_os_task_handle_t task_handle;
            volatile bool stop_requested;
            pal_os_sem_t wake_sem;
            pal_os_sem_t done_sem;
        } task;
    } u;
} periodic_entry_t;

static periodic_entry_t s_periodic[WINK_MAX_PERIODIC];

static wink_status_t periodic_light_cb(void *arg) {
    periodic_entry_t *e = (periodic_entry_t *)arg;
    if (e->fn != NULL) {
        e->fn(e->ctx);
    }
    return WINK_OK;
}

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
            next_wake = now;
            if (e->u.task.wake_sem != NULL) {
                WINK_IGNORE_UNUSED(pal_os_sem_take(e->u.task.wake_sem, 1u));
            } else {
                pal_os_sleep_ms(1);
            }
        } else {
            uint32_t delta_ms = (uint32_t)((delta_us + 999ULL) / 1000ULL);
            if (e->u.task.wake_sem != NULL) {
                WINK_IGNORE_UNUSED(pal_os_sem_take(e->u.task.wake_sem, delta_ms));
            } else {
                pal_os_sleep_ms(delta_ms);
            }
        }
    }
    if (e->u.task.done_sem != NULL) {
        wink_status_t give_st = pal_os_sem_give(e->u.task.done_sem);
        (void)give_st;
    }
    pal_os_task_delete(NULL);
}

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
        use_task = (period_ms >= 50) || (stack_hint >= 2048);
    }

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
        uint32_t ticks = (period_ms + WINK_RUNTIME_TICK_MS - 1) / WINK_RUNTIME_TICK_MS;
        int32_t h = wink_soft_timer_create(periodic_light_cb, e,
                                           WINK_TIMER_PERIODIC,
                                           ticks * WINK_RUNTIME_TICK_MS);
        if (h < 0) {
            memset(e, 0, sizeof(*e));
            return (wink_periodic_handle_t)WINK_ERR_RESOURCE_EXHAUSTED;
        }
        wink_soft_timer_set_name(h, name);
        wink_status_t start_st = wink_soft_timer_start(h);
        if (wink_status_is_error(start_st)) {
            WINK_IGNORE_UNUSED(wink_soft_timer_destroy(h));
            memset(e, 0, sizeof(*e));
            return (wink_periodic_handle_t)start_st;
        }
        e->u.soft_timer_handle = h;
    }

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
            WINK_IGNORE_UNUSED(wink_soft_timer_destroy(e->u.soft_timer_handle));
        }
    } else if (e->kind == PERIODIC_ENTRY_TASK) {
        e->u.task.stop_requested = true;
        if (e->u.task.wake_sem != NULL) {
            WINK_IGNORE_UNUSED(pal_os_sem_give(e->u.task.wake_sem));
        }
        if (e->u.task.done_sem != NULL) {
            wink_status_t take_st = pal_os_sem_take(e->u.task.done_sem, 500u);
            (void)take_st;
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
    if (h <= 0) return WINK_ERR_INVALID_ARG;
    if (period_ms == 0) return WINK_ERR_INVALID_ARG;

    int slot = (int)(h - 1);
    if (slot < 0 || slot >= WINK_MAX_PERIODIC) return WINK_ERR_INVALID_ARG;
    periodic_entry_t *e = &s_periodic[slot];
    if (e->kind == PERIODIC_ENTRY_FREE) return WINK_ERR_INVALID_ARG;

    if (e->kind == PERIODIC_ENTRY_LIGHT) {
        return wink_soft_timer_change_period(e->u.soft_timer_handle, period_ms);
    }

    e->period_ms = period_ms;
    if (e->u.task.wake_sem != NULL) {
        WINK_IGNORE_UNUSED(pal_os_sem_give(e->u.task.wake_sem));
    }
    return WINK_OK;
}
