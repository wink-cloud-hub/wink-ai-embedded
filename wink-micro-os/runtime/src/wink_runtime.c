// SPDX-License-Identifier: Apache-2.0
/**
 * @file wink_runtime.c
 * @brief Cooperative main loop + fail-safe / boot safe-lock implementation.
 */
#include "wink_runtime.h"
#include "wink_fault.h"
#include "wink_tasks.h"
#include "wink_soft_timer.h"
#include "wink_event.h"
#include "wink_actuator_registry.h"
#include "wink_trace.h"
#include "pal_osal.h"
#include <string.h>

#ifdef SIMULATION
#include "pal_wasm_internal.h"
#include "wink_sim_scheduler.h"
#include <stdlib.h>
#include <stdio.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static const wink_app_callbacks_t *s_active_cbs = NULL;

#define WINK_MAX_POLL_CBS 16
static struct {
    void (*fn)(void *ctx);
    void *ctx;
} s_poll_cbs[WINK_MAX_POLL_CBS];
static uint8_t s_poll_count = 0;

static void wink_runtime_monitor_wcet_loop(void (*callback)(void), const char* name) {
    uint64_t start_us;
    uint64_t elapsed_us;

    if (callback == NULL) {
        return;
    }

    start_us = pal_os_get_us();
    callback();
    elapsed_us = pal_os_get_us() - start_us;

    if (elapsed_us > (WINK_RUNTIME_TICK_MS * 1000U / 2U)) {
        wink_trace_warn(WINK_WARN_WCET_EXCEEDED);
    }
    (void)name;
}

static wink_status_t wink_runtime_invoke_init(const wink_app_callbacks_t *cb) {
    if (cb->init_status != NULL) {
        return cb->init_status();
    }
    if (cb->init != NULL) {
        cb->init();
        return WINK_OK;
    }
    return WINK_OK;
}

static wink_status_t wink_runtime_invoke_on_fault(const wink_app_callbacks_t *cb, uint32_t code) {
    if (cb->on_fault_status != NULL) {
        return cb->on_fault_status(code);
    }
    if (cb->on_fault != NULL) {
        cb->on_fault(code);
        return WINK_OK;
    }
    return WINK_OK;
}

void wink_app_delay_ms(uint32_t ms) {
    pal_os_sleep_ms(ms);
}

static wink_reset_reason_t map_reset_reason(pal_os_reset_reason_t rr) {
    return (wink_reset_reason_t)rr;
}

#ifdef SIMULATION
static void sim_app_main_task(void* arg) {
    const wink_app_callbacks_t* callbacks = (const wink_app_callbacks_t*)arg;
    uint32_t tick = 0;
    while (1) {
        uint64_t tick_start_us = pal_os_get_us();
        uint64_t tick_elapsed_us;

        wink_soft_timer_dispatch();

        for (uint8_t i = 0; i < s_poll_count; i++) {
            s_poll_cbs[i].fn(s_poll_cbs[i].ctx);
        }

        if (callbacks->on_event != NULL) {
            wink_event_t event;
            while (wink_event_pend(&event, 0) == WINK_OK) {
                callbacks->on_event(&event);
            }
        }

        wink_runtime_monitor_wcet_loop(callbacks->loop, "app_loop");

        tick_elapsed_us = pal_os_get_us() - tick_start_us;
        if (tick_elapsed_us > WINK_RUNTIME_TICK_MS * 1000U) {
            wink_trace_warn(WINK_WARN_TICK_OVERRUN);
        }

        if (tick == WINK_BOOT_HEALTHY_TICKS) {
            pal_os_set_abnormal_boot_count(0);
        }

        uint32_t elapsed_ms = (uint32_t)(tick_elapsed_us / 1000U);
        uint32_t sleep_ms = 0;
        if (elapsed_ms < WINK_RUNTIME_TICK_MS) {
            sleep_ms = WINK_RUNTIME_TICK_MS - elapsed_ms;
        }

        if (sleep_ms > 0) {
            if (callbacks->on_event != NULL) {
                wink_event_t event;
                wink_status_t st = wink_event_pend(&event, sleep_ms);
                if (st == WINK_OK) {
                    callbacks->on_event(&event);
                }
            } else {
                wink_app_delay_ms(sleep_ms);
            }
        }
        tick++;
    }
}
#endif

wink_status_t wink_runtime_run(const wink_app_callbacks_t* callbacks, uint32_t max_ticks) {
    pal_os_reset_reason_t rr;

    if (callbacks == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    s_active_cbs = callbacks;

#ifdef SIMULATION
    {
        const char* seed_env = getenv("WINK_SIM_SEED");
        uint32_t seed = seed_env ? (uint32_t)strtoul(seed_env, NULL, 10) : 42u;
        sim_scheduler_reset(seed);
    }
#endif

    rr = pal_os_get_reset_reason();
    if (rr == PAL_OS_RESET_REASON_POWER_ON) {
        pal_os_set_abnormal_boot_count(0);
    } else if (rr == PAL_OS_RESET_REASON_WATCHDOG || rr == PAL_OS_RESET_REASON_PANIC) {
        uint32_t abnormal = pal_os_get_abnormal_boot_count() + 1u;
        pal_os_set_abnormal_boot_count(abnormal);
        if (abnormal >= WINK_BOOT_LOCK_THRESHOLD) {
            wink_runtime_fault(callbacks, WINK_FAULT_BOOT_AFTER_RESET);
#ifdef SIMULATION
            printf("FATAL: Boot lockout count reached threshold! Entering Safe-lock.\n");
            abort();
#endif
            return WINK_ERR_LOCKED;
        }
    }

    wink_status_t st_init = wink_soft_timer_init();
    if (wink_status_is_error(st_init)) {
        return st_init;
    }

    wink_status_t st_ev = wink_event_queue_init(WINK_EVENT_QUEUE_DEFAULT_CAPACITY);
    if (wink_status_is_error(st_ev)) {
        return st_ev;
    }

    if (callbacks->on_boot != NULL) {
        wink_boot_info_t info;
        memset(&info, 0, sizeof(info));
        info.reset_reason       = map_reset_reason(rr);
        info.abnormal_boot_count = pal_os_get_abnormal_boot_count();
        info.is_healthy_recovery = (rr == PAL_OS_RESET_REASON_POWER_ON ||
                                    rr == PAL_OS_RESET_REASON_SOFTWARE ||
                                    rr == PAL_OS_RESET_REASON_BROWNOUT ||
                                    rr == PAL_OS_RESET_REASON_UNKNOWN)
                                   && (info.abnormal_boot_count == 0);
        info.uptime_ms          = 0;
        callbacks->on_boot(&info);
    }

    {
        uint64_t t0 = pal_os_get_us();
        wink_status_t init_st = wink_runtime_invoke_init(callbacks);
        uint64_t elapsed = pal_os_get_us() - t0;
        if (elapsed > (WINK_RUNTIME_TICK_MS * 1000U / 2U)) {
            wink_trace_warn(WINK_WARN_WCET_EXCEEDED);
        }
        if (wink_status_is_error(init_st)) {
            wink_runtime_fault(callbacks, WINK_FAULT_RUNTIME(50));
            wink_event_queue_deinit();
            return init_st;
        }
    }

#ifdef SIMULATION
    uint32_t main_task_id;
    wink_status_t st = sim_scheduler_register(
        sim_app_main_task, (void*)callbacks, "app_main",
        5, PAL_OS_CORE_ANY, 32*1024, &main_task_id);
    if (st != WINK_OK) {
        wink_event_queue_deinit();
        return st;
    }

    wink_status_t run_st = pal_sim_scheduler_run(callbacks, main_task_id, max_ticks);
    wink_event_queue_deinit();
    return run_st;
#else
    uint32_t tick = 0;
    while ((max_ticks == 0U) || (tick < max_ticks)) {
        uint64_t tick_start_us = pal_os_get_us();
        uint64_t tick_elapsed_us;

        wink_soft_timer_dispatch();

        for (uint8_t i = 0; i < s_poll_count; i++) {
            s_poll_cbs[i].fn(s_poll_cbs[i].ctx);
        }

        if (callbacks->on_event != NULL) {
            wink_event_t event;
            while (wink_event_pend(&event, 0) == WINK_OK) {
                callbacks->on_event(&event);
            }
        }

        wink_runtime_monitor_wcet_loop(callbacks->loop, "app_loop");

        tick_elapsed_us = pal_os_get_us() - tick_start_us;
        if (tick_elapsed_us > WINK_RUNTIME_TICK_MS * 1000U) {
            wink_trace_warn(WINK_WARN_TICK_OVERRUN);
        }

        if (tick == WINK_BOOT_HEALTHY_TICKS) {
            pal_os_set_abnormal_boot_count(0);
        }

        uint32_t elapsed_ms = (uint32_t)(tick_elapsed_us / 1000U);
        uint32_t sleep_ms = 0;
        if (elapsed_ms < WINK_RUNTIME_TICK_MS) {
            sleep_ms = WINK_RUNTIME_TICK_MS - elapsed_ms;
        }

        if (sleep_ms > 0) {
            if (callbacks->on_event != NULL) {
                wink_event_t event;
                wink_status_t st = wink_event_pend(&event, sleep_ms);
                if (st == WINK_OK) {
                    callbacks->on_event(&event);
                }
            } else {
                wink_app_delay_ms(sleep_ms);
            }
        }
        tick++;
    }
    wink_event_queue_deinit();
    return WINK_OK;
#endif
}

void wink_runtime_fault(const wink_app_callbacks_t* callbacks, uint32_t fault_code) {
    wink_trace_fault(fault_code);
    wink_actuator_safe_off_all();
    if (callbacks != NULL) {
        (void)wink_runtime_invoke_on_fault(callbacks, fault_code);
    }
}

void wink_runtime_raise_fault(uint32_t fault_code) {
    wink_runtime_fault(s_active_cbs, fault_code);
}

wink_status_t wink_runtime_register_poll(void (*fn)(void *ctx), void *ctx) {
    if (fn == NULL) return WINK_ERR_INVALID_ARG;
    if (s_poll_count >= WINK_MAX_POLL_CBS) return WINK_ERR_RESOURCE_EXHAUSTED;
    s_poll_cbs[s_poll_count].fn = fn;
    s_poll_cbs[s_poll_count].ctx = ctx;
    s_poll_count++;
    return WINK_OK;
}

void wink_runtime_get_stats(wink_runtime_stats_t *out) {
    if (out == NULL) return;
    memset(out, 0, sizeof(*out));
    out->uptime_ms           = (uint32_t)(pal_os_get_us() / 1000U);
    out->fault_count         = wink_trace_count();
    out->warn_count          = wink_warn_count();
    out->abnormal_boot_count = pal_os_get_abnormal_boot_count();
    out->last_reset_reason   = map_reset_reason(pal_os_get_reset_reason());
    out->free_heap           = pal_os_get_free_heap_size();
    out->min_free_stack      = pal_os_get_current_task_stack_free();
}

void wink_runtime_trigger_wdt_test(uint32_t timeout_ms) {
    WINK_IGNORE_UNUSED(pal_os_wdt_init(timeout_ms));
    WINK_IGNORE_UNUSED(pal_os_wdt_feed());
    while (1) {
    }
}
