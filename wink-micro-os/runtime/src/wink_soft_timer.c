// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_soft_timer.c
 * @brief Software timer scheduler implementation (static array + tick alignment).
 */
#include "wink_soft_timer.h"
#include "wink_runtime.h"
#include "wink_fault.h"
#include "wink_pt_debug.h"
#include "pal_osal.h"
#include "wink_trace.h"
#include <string.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(WINK_PT_DEBUG_FAULT_LIGHT_BLOCKING == WINK_FAULT_LIGHT_BLOCKING,
               "WINK_PT_DEBUG_FAULT_LIGHT_BLOCKING out of sync with WINK_FAULT_LIGHT_BLOCKING");
#endif

#define WINK_LIGHT_SOFT_BUDGET_US   100u
#define WINK_LIGHT_HARD_LIMIT_US    500u
#define WINK_LIGHT_WCET_STRIKES     3u

typedef struct {
    wink_soft_timer_callback_t callback;  /**< Callback function (NULL if free) */
    void*                       arg;       /**< User context */
    const char*                 name;      /**< Optional diagnostic name */
    wink_timer_mode_t           mode;      /**< Oneshot or periodic mode */
    uint32_t                    period_ticks;  /**< Period in ticks */
    uint32_t                    remaining_ticks; /**< Remaining ticks until expiration */
    uint8_t                     active;    /**< 1 = active, 0 = stopped */
    uint8_t                     consecutive_overruns;
} wink_timer_cb_t;

static wink_timer_cb_t s_timers[WINK_MAX_SOFT_TIMERS];
static uint8_t s_initialized = 0;

static volatile bool g_in_light_dispatch = false;

wink_status_t wink_soft_timer_init(void) {
    memset(s_timers, 0, sizeof(s_timers));
    g_in_light_dispatch = false;
    s_initialized = 1;
    return WINK_OK;
}

int32_t wink_soft_timer_create(
    wink_soft_timer_callback_t callback,
    void* arg,
    wink_timer_mode_t mode,
    uint32_t period_ms
) {
    int32_t i;
    uint32_t period_ticks;

    if (!s_initialized || callback == NULL || period_ms == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    period_ticks = period_ms / WINK_RUNTIME_TICK_MS;
    if (period_ticks == 0) {
        period_ticks = 1;
    }

    for (i = 0; i < (int32_t)WINK_MAX_SOFT_TIMERS; i++) {
        if (s_timers[i].callback == NULL) {
            break;
        }
    }

    if (i >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return WINK_ERR_NO_MEM;
    }

    s_timers[i].callback        = callback;
    s_timers[i].arg             = arg;
    s_timers[i].name            = NULL;
    s_timers[i].mode            = mode;
    s_timers[i].period_ticks    = period_ticks;
    s_timers[i].remaining_ticks = period_ticks;
    s_timers[i].active          = 0;
    s_timers[i].consecutive_overruns = 0;

    return i;
}

wink_status_t wink_soft_timer_start(int32_t handle) {
    if (!s_initialized || handle < 0 || handle >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_timers[handle].callback == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    s_timers[handle].remaining_ticks = s_timers[handle].period_ticks;
    s_timers[handle].active = 1;
    s_timers[handle].consecutive_overruns = 0;
    return WINK_OK;
}

wink_status_t wink_soft_timer_stop(int32_t handle) {
    if (!s_initialized || handle < 0 || handle >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return WINK_ERR_INVALID_ARG;
    }

    s_timers[handle].active = 0;
    return WINK_OK;
}

wink_status_t wink_soft_timer_destroy(int32_t handle) {
    if (!s_initialized || handle < 0 || handle >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return WINK_ERR_INVALID_ARG;
    }

    if (s_timers[handle].callback == NULL) {
        return WINK_OK;
    }

    memset(&s_timers[handle], 0, sizeof(s_timers[handle]));
    return WINK_OK;
}

wink_status_t wink_soft_timer_change_period(int32_t handle, uint32_t period_ms) {
    uint32_t period_ticks;

    if (!s_initialized || handle < 0 || handle >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_timers[handle].callback == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (period_ms == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    period_ticks = period_ms / WINK_RUNTIME_TICK_MS;
    if (period_ticks == 0) {
        period_ticks = 1;
    }

    s_timers[handle].period_ticks = period_ticks;
    if (s_timers[handle].active && s_timers[handle].remaining_ticks > period_ticks) {
        s_timers[handle].remaining_ticks = period_ticks;
    }
    s_timers[handle].consecutive_overruns = 0;
    return WINK_OK;
}

void wink_soft_timer_set_name(int32_t handle, const char *name) {
    if (!s_initialized || handle < 0 || handle >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return;
    }
    s_timers[handle].name = name;
}

bool wink_soft_timer_in_light_dispatch(void) {
    return g_in_light_dispatch;
}

void wink_soft_timer_dispatch(void) {
    int32_t i;

    if (!s_initialized) {
        return;
    }

    for (i = 0; i < (int32_t)WINK_MAX_SOFT_TIMERS; i++) {
        wink_timer_cb_t* timer = &s_timers[i];
        wink_status_t status;
        uint64_t start_us;
        uint64_t elapsed_us;

        if (timer->callback == NULL || !timer->active) {
            continue;
        }

        if (timer->remaining_ticks > 0) {
            timer->remaining_ticks--;
        }
        if (timer->remaining_ticks != 0) {
            continue;
        }

        start_us   = pal_os_get_us();

        g_in_light_dispatch = true;
        status     = timer->callback(timer->arg);
        g_in_light_dispatch = false;

        elapsed_us = pal_os_get_us() - start_us;

        if (elapsed_us > WINK_LIGHT_HARD_LIMIT_US) {
            timer->consecutive_overruns++;
            if (timer->consecutive_overruns >= WINK_LIGHT_WCET_STRIKES) {
                wink_trace_fault(WINK_FAULT_LIGHT_WCET_VIOLATION);
                timer->consecutive_overruns = 0;
            } else {
                wink_trace_warn(WINK_WARN_LIGHT_OVERBUDGET);
            }
        } else if (elapsed_us > WINK_LIGHT_SOFT_BUDGET_US) {
            wink_trace_warn(WINK_WARN_LIGHT_OVERBUDGET);
            timer->consecutive_overruns = 0;
        } else {
            timer->consecutive_overruns = 0;
        }

        if (status != WINK_OK || timer->mode == WINK_TIMER_ONESHOT) {
            timer->active = 0;
        } else {
            timer->remaining_ticks = timer->period_ticks;
        }
    }
}
