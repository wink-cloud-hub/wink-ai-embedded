/**
 * @file wink_runtime.c
 * @brief Cooperative main loop (callback injection) + fail-safe / boot safe-lock (Phase 5).
 *
 * Safety Hierarchy:
 * 1. Boot Safe-Lock: WDT/PANIC reset → never execute user init/loop
 * 2. Fine-grained WCET: Per-callback timing at both init and loop level
 * 3. Global tick WCET: Backup for total tick duration
 */
#include "wink_runtime.h"
#include "pal_osal.h"
#include "wink_trace.h"
#include "wink_actuator_registry.h"
/* Method C: poll-based interrupt dispatch at tick boundary (wasm simulation target only).
 * Included only under SIMULATION macro; host/esp32 targets skip this header at compile time. */
#ifdef SIMULATION
#include "pal_wasm_internal.h"
#include "wink_sim_scheduler.h"
#include <stdlib.h>   /* getenv / strtoul for H5 seed injection */
#endif

/* Soft timer scheduler (ADR-0007) */
#include "wink_soft_timer.h"


/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API。抑制
 * -Wdeprecated-declarations 使 -Werror 下仍能编译；严格模式
 * (-DWINK_STRICT_NONBLOCKING=1) 下相关 API 声明直接消失，本 TU 会链接失败——那是设计意图。 */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* ============================================================
 *  Fine-grained WCET Monitor (ADR-0007)
 *  Wraps callback execution and measures individual duration.
 *  Triggers warning if individual callback exceeds threshold.
 * ============================================================ */

/**
 * @brief Measure and trace WCET for an init callback (void return)
 */
static void wink_runtime_monitor_wcet_init(void (*callback)(void), const char* name) {
    uint64_t start_us;
    uint64_t elapsed_us;

    if (callback == NULL) {
        return;
    }

    start_us = pal_os_get_us();
    callback();
    elapsed_us = pal_os_get_us() - start_us;

    /* Individual callback WCET threshold: 50% of tick period */
    if (elapsed_us > (WINK_RUNTIME_TICK_MS * 1000U / 2U)) {
        wink_trace_warn(WINK_WARN_WCET_EXCEEDED);
    }
}

/**
 * @brief Measure and trace WCET for a loop callback (void return)
 */
static void wink_runtime_monitor_wcet_loop(void (*callback)(void), const char* name) {
    uint64_t start_us;
    uint64_t elapsed_us;

    if (callback == NULL) {
        return;
    }

    start_us = pal_os_get_us();
    callback();
    elapsed_us = pal_os_get_us() - start_us;

    /* Individual callback WCET threshold: 50% of tick period */
    if (elapsed_us > (WINK_RUNTIME_TICK_MS * 1000U / 2U)) {
        wink_trace_warn(WINK_WARN_WCET_EXCEEDED);
    }
}

/* ============================================================
 *  Runtime Entry Point
 * ============================================================ */

void wink_app_delay_ms(uint32_t ms) {
    pal_os_sleep_ms(ms);
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

        /* --- Run user loop callback with individual WCET monitoring --- */
        wink_runtime_monitor_wcet_loop(callbacks->loop, "app_loop");

        /* --- Global tick WCET check (backup safety net) --- */
        tick_elapsed_us = pal_os_get_us() - tick_start_us;
        if (tick_elapsed_us > WINK_RUNTIME_TICK_MS * 1000U) {
            wink_trace_warn(WINK_WARN_TICK_OVERRUN);
        }

        /* fixup 计划 M3：wasm 中断 dispatch 已移到 pal_sim_scheduler_run 主 loop
         * 顶部（targets/wasm/pal_osal_wasm.c），恢复 ADR-0013 §边界 3 "O(scheduler
         * tick) 唤醒延迟" 承诺。此处不再重复 dispatch，避免 double-dispatch。 */

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

#ifdef SIMULATION
    /* 在运行任何用户初始化(app_init)之前，必须先重置调度器，
     * 否则 app_init 中注册的所有用户协程都会被随后的重置给抹除。
     * fixup 计划 H5：seed 可通过 env WINK_SIM_SEED 注入以支撑 seed sweep 测试；
     * 缺省 42，与旧硬编码行为兼容。 */
    {
        const char* seed_env = getenv("WINK_SIM_SEED");
        uint32_t seed = seed_env ? (uint32_t)strtoul(seed_env, NULL, 10) : 42u;
        sim_scheduler_reset(seed);
    }
#endif

    /* ============================================================
     *  BOOT SAFE-LOCK with recovery (ADR-0010, revises ADR-0007)
     *  Counts consecutive abnormal (WDT/PANIC) resets in persistent storage:
     *    - POWERON              → clear counter (fresh boot)
     *    - WDT/PANIC            → increment counter
     *        * >= WINK_BOOT_LOCK_THRESHOLD → LOCK: trace 8001 once + safe-off +
     *          on_fault, return WINK_ERR_LOCKED (NO user init/loop). Real death-loop guard.
     *        * < threshold              → RECOVER: fall through to normal init/loop
     *          (single/transient reset auto-recovers; not traced — recovery is not a fault)
     *    - SW/BROWNOUT/UNKNOWN  → leave counter unchanged, fall through
     *  Counter also clears at the healthy milestone (init done + HEALTHY_TICKS stable
     *  ticks) so a later isolated glitch doesn't accumulate toward a false lock.
     * ============================================================ */
    rr = pal_os_get_reset_reason();
    if (rr == PAL_OS_RESET_REASON_POWER_ON) {
        pal_os_set_abnormal_boot_count(0);
    } else if (rr == PAL_OS_RESET_REASON_WATCHDOG || rr == PAL_OS_RESET_REASON_PANIC) {
        uint32_t abnormal = pal_os_get_abnormal_boot_count() + 1u;
        pal_os_set_abnormal_boot_count(abnormal);
        if (abnormal >= WINK_BOOT_LOCK_THRESHOLD) {
            /* Death loop: lock out user code. wink_runtime_fault traces 8001 once + safe-off + on_fault. */
            wink_runtime_fault(callbacks, WINK_FAULT_BOOT_AFTER_RESET);
            return WINK_ERR_LOCKED;
        }
        /* abnormal < threshold: recover — fall through to normal init/loop */
    }

    /* Initialize soft timer subsystem before user code */
    wink_status_t st_init = wink_soft_timer_init();
    if (wink_status_is_error(st_init)) {
        return st_init;
    }

    /* Safe to proceed with user initialization - WCET monitored */
    if (callbacks->init != NULL) {
        wink_runtime_monitor_wcet_init(callbacks->init, "app_init");
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

        /* --- Run user loop callback with individual WCET monitoring --- */
        wink_runtime_monitor_wcet_loop(callbacks->loop, "app_loop");

        /* --- Global tick WCET check (backup safety net) --- */
        tick_elapsed_us = pal_os_get_us() - tick_start_us;
        if (tick_elapsed_us > WINK_RUNTIME_TICK_MS * 1000U) {
            wink_trace_warn(WINK_WARN_TICK_OVERRUN);
        }

        /* ADR-0010: healthy milestone — init succeeded + stable for HEALTHY_TICKS ticks
         * proves the prior crash path is past; clear the abnormal-reset counter so a
         * later isolated glitch doesn't accumulate toward a false lock. */
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
    if (callbacks != NULL && callbacks->on_fault != NULL) {
        callbacks->on_fault(fault_code);
    }
}
