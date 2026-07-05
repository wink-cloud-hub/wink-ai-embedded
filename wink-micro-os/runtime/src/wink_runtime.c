/**
 * @file wink_runtime.c
 * @brief Cooperative main loop (callback injection) + fail-safe / boot safe-lock (Phase 5).
 *
 * Safety Hierarchy:
 * 1. Boot Safe-Lock: WDT/PANIC reset → never execute user init/loop
 * 2. Fine-grained WCET: Per-callback timing at both init and loop level
 * 3. Global tick WCET: Backup for total tick duration
 *
 * Wave 2 (2026-07): added on_boot / init_status / on_fault_status callbacks,
 * wink_runtime_raise_fault(), poll registration, get_stats, trigger_wdt.
 */
#include "wink_runtime.h"
#include "wink_fault.h"
#include "wink_tasks.h"
#include "wink_soft_timer.h"
#include "wink_actuator_registry.h"
#include "wink_trace.h"
#include "pal_osal.h"
#include <string.h>

/* Method C: poll-based interrupt dispatch at tick boundary (wasm simulation target only).
 * Included only under SIMULATION macro; host/esp32 targets skip this header at compile time. */
#ifdef SIMULATION
#include "pal_wasm_internal.h"
#include "wink_sim_scheduler.h"
#include <stdlib.h>   /* getenv / strtoul for H5 seed injection */
#endif

/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* ── Static state ─────────────────────────────────────────── */
static const wink_app_callbacks_t *s_active_cbs = NULL;

/* Poll registry (for DAL auto-poll, e.g. button debounce). */
#define WINK_MAX_POLL_CBS 16
static struct {
    void (*fn)(void *ctx);
    void *ctx;
} s_poll_cbs[WINK_MAX_POLL_CBS];
static uint8_t s_poll_count = 0;

/* ── Helpers ─────────────────────────────────────────────── */

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

/* Invoke init via whichever variant is present. */
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

/* Invoke on_fault; returns WINK_ERR_LOCKED if app wants us to halt. */
static wink_status_t wink_runtime_invoke_on_fault(const wink_app_callbacks_t *cb, uint32_t code) {
    if (cb->on_fault_status != NULL) {
        return cb->on_fault_status(code);
    }
    if (cb->on_fault != NULL) {
        cb->on_fault(code);
        /* Legacy void-returning on_fault: do NOT spin forever. Callers that
         * want halt can use on_fault_status returning LOCKED, or we are
         * being called from a non-loop context (tests / explicit raise). */
        return WINK_OK;
    }
    return WINK_OK; /* no fault handler: nothing to do */
}

/* ── Public API ──────────────────────────────────────────── */

void wink_app_delay_ms(uint32_t ms) {
    pal_os_sleep_ms(ms);
}

/* Translate PAL reset reason → runtime reset reason (simple identity map
 * since enum values match by design). */
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

        /* --- Soft timer callbacks first --- */
        wink_soft_timer_dispatch();

        /* --- Registered poll callbacks (DAL auto-poll) --- */
        for (uint8_t i = 0; i < s_poll_count; i++) {
            s_poll_cbs[i].fn(s_poll_cbs[i].ctx);
        }

        /* --- Run user loop callback with individual WCET monitoring --- */
        wink_runtime_monitor_wcet_loop(callbacks->loop, "app_loop");

        /* --- Global tick WCET check (backup safety net) --- */
        tick_elapsed_us = pal_os_get_us() - tick_start_us;
        if (tick_elapsed_us > WINK_RUNTIME_TICK_MS * 1000U) {
            wink_trace_warn(WINK_WARN_TICK_OVERRUN);
        }

        /* ADR-0010: healthy milestone */
        if (tick == WINK_BOOT_HEALTHY_TICKS) {
            pal_os_set_abnormal_boot_count(0);
        }

        wink_app_delay_ms(WINK_RUNTIME_TICK_MS);
        tick++;
    }
}
#endif

wink_status_t wink_runtime_run(const wink_app_callbacks_t* callbacks, uint32_t max_ticks) {
    pal_os_reset_reason_t rr;

    if (callbacks == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Stash active callbacks so wink_runtime_raise_fault can find them. */
    s_active_cbs = callbacks;

#ifdef SIMULATION
    /* 在运行任何用户初始化(app_init)之前，必须先重置调度器，
     * 否则 app_init 中注册的所有用户协程都会被随后的重置给抹除。 */
    {
        const char* seed_env = getenv("WINK_SIM_SEED");
        uint32_t seed = seed_env ? (uint32_t)strtoul(seed_env, NULL, 10) : 42u;
        sim_scheduler_reset(seed);
    }
#endif

    /* ============================================================
     *  BOOT SAFE-LOCK with recovery (ADR-0010, revises ADR-0007)
     * ============================================================ */
    rr = pal_os_get_reset_reason();
    if (rr == PAL_OS_RESET_REASON_POWER_ON) {
        pal_os_set_abnormal_boot_count(0);
    } else if (rr == PAL_OS_RESET_REASON_WATCHDOG || rr == PAL_OS_RESET_REASON_PANIC) {
        uint32_t abnormal = pal_os_get_abnormal_boot_count() + 1u;
        pal_os_set_abnormal_boot_count(abnormal);
        if (abnormal >= WINK_BOOT_LOCK_THRESHOLD) {
            /* Death loop: lock out user code. */
            wink_runtime_fault(callbacks, WINK_FAULT_BOOT_AFTER_RESET);
            return WINK_ERR_LOCKED;
        }
        /* abnormal < threshold: recover — fall through */
    }

    /* Initialize soft timer subsystem before user code */
    wink_status_t st_init = wink_soft_timer_init();
    if (wink_status_is_error(st_init)) {
        return st_init;
    }

    /* ── on_boot callback ────────────────────────────────── */
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

    /* ── User init (WCET monitored, status-aware) ────────── */
    {
        uint64_t t0 = pal_os_get_us();
        wink_status_t init_st = wink_runtime_invoke_init(callbacks);
        uint64_t elapsed = pal_os_get_us() - t0;
        if (elapsed > (WINK_RUNTIME_TICK_MS * 1000U / 2U)) {
            wink_trace_warn(WINK_WARN_WCET_EXCEEDED);
        }
        if (wink_status_is_error(init_st)) {
            /* init returned error → auto fault + safe-off. */
            wink_runtime_fault(callbacks, WINK_FAULT_RUNTIME(50)); /* init-failed sentinel */
            return init_st;
        }
    }

#ifdef SIMULATION
    uint32_t main_task_id;
    wink_status_t st = sim_scheduler_register(
        sim_app_main_task, (void*)callbacks, "app_main",
        5, PAL_OS_CORE_ANY, 32*1024, &main_task_id);
    if (st != WINK_OK) return st;

    return pal_sim_scheduler_run(callbacks, main_task_id, max_ticks);
#else
    uint32_t tick = 0;
    /* max_ticks == 0 => infinite loop (embedded/wasm); host tests pass a finite value. */
    while ((max_ticks == 0U) || (tick < max_ticks)) {
        uint64_t tick_start_us = pal_os_get_us();
        uint64_t tick_elapsed_us;

        /* --- Soft timer callbacks first --- */
        wink_soft_timer_dispatch();

        /* --- Registered poll callbacks (DAL auto-poll) --- */
        for (uint8_t i = 0; i < s_poll_count; i++) {
            s_poll_cbs[i].fn(s_poll_cbs[i].ctx);
        }

        /* --- Run user loop callback with individual WCET monitoring --- */
        wink_runtime_monitor_wcet_loop(callbacks->loop, "app_loop");

        /* --- Global tick WCET check (backup safety net) --- */
        tick_elapsed_us = pal_os_get_us() - tick_start_us;
        if (tick_elapsed_us > WINK_RUNTIME_TICK_MS * 1000U) {
            wink_trace_warn(WINK_WARN_TICK_OVERRUN);
        }

        /* ADR-0010: healthy milestone */
        if (tick == WINK_BOOT_HEALTHY_TICKS) {
            pal_os_set_abnormal_boot_count(0);
        }

        wink_app_delay_ms(WINK_RUNTIME_TICK_MS);
        tick++;
    }
    return WINK_OK;
#endif
}

void wink_runtime_fault(const wink_app_callbacks_t* callbacks, uint32_t fault_code) {
    wink_trace_fault(fault_code);
    wink_actuator_safe_off_all();
    if (callbacks != NULL) {
        (void)wink_runtime_invoke_on_fault(callbacks, fault_code);
        /* Note: we do NOT spin/halt here. On real hardware a non-recoverable
         * fault should be escalated via WDT (wink_runtime_trigger_wdt_test or
         * app-specific). Returning lets host tests and non-fatal faults
         * continue; boot-safe-lock path in wink_runtime_run returns its own
         * WINK_ERR_LOCKED to the caller without entering the loop. */
    }
}

void wink_runtime_raise_fault(uint32_t fault_code) {
    /* Use stashed callbacks pointer — app code never holds this. */
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
    /* free_heap / min_free_stack are platform-specific and not yet
     * universally plumbed — leave as 0 until PAL exposes them. */
    out->free_heap      = 0;
    out->min_free_stack = 0;
}

void wink_runtime_trigger_wdt_test(uint32_t timeout_ms) {
    /* Initialise WDT, briefly feed, then spin. Real hardware will reset.
     * Host/wasm targets either no-op WDT init or loop forever (test can
     * observe the loop via max_ticks timeout). */
    WINK_IGNORE_UNUSED(pal_os_wdt_init(timeout_ms));
    /* Brief feed to let WDT arm, then stop feeding. */
    WINK_IGNORE_UNUSED(pal_os_wdt_feed());
    while (1) {
        /* Spin without feeding. */
    }
}
