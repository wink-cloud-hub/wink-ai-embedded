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
#endif

/* Soft timer scheduler (ADR-0007) */
#include "wink_soft_timer.h"

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

    start_us = pal_get_us();
    callback();
    elapsed_us = pal_get_us() - start_us;

    /* Individual callback WCET threshold: 50% of tick period */
    if (elapsed_us > (WINK_RUNTIME_TICK_MS * 1000U / 2U)) {
        wink_trace_fault(WINK_WARN_WCET_EXCEEDED);
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

    start_us = pal_get_us();
    callback();
    elapsed_us = pal_get_us() - start_us;

    /* Individual callback WCET threshold: 50% of tick period */
    if (elapsed_us > (WINK_RUNTIME_TICK_MS * 1000U / 2U)) {
        wink_trace_fault(WINK_WARN_WCET_EXCEEDED);
    }
}

/* ============================================================
 *  Runtime Entry Point
 * ============================================================ */

void wink_app_delay_ms(uint32_t ms) {
    pal_delay_ms(ms);
}

wink_status_t wink_runtime_run(const wink_app_callbacks_t* callbacks, uint32_t max_ticks) {
    pal_reset_reason_t rr;
    uint32_t tick;

    if (callbacks == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    /* ============================================================
     *  BOOT SAFE-LOCK (Hard enforcement, no user code bypass)
     *  If last reset was WDT/PANIC:
     *    1. Trace fault condition
     *    2. Disable all actuators immediately
     *    3. Return locked error - NO USER INIT/LOOP IS EXECUTED
     * ============================================================ */
    rr = pal_get_reset_reason();
    if (rr == PAL_RESET_REASON_WATCHDOG || rr == PAL_RESET_REASON_PANIC) {
        wink_trace_fault(WINK_FAULT_BOOT_AFTER_RESET);
        wink_actuator_safe_off_all();
        /* Enter fault handler but DO NOT call user init */
        wink_runtime_fault(callbacks, WINK_FAULT_BOOT_AFTER_RESET);
        return WINK_ERR_LOCKED;
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

    tick = 0;
    /* max_ticks == 0 => infinite loop (embedded/wasm); host tests pass a finite value. */
    while ((max_ticks == 0U) || (tick < max_ticks)) {
        uint64_t tick_start_us = pal_get_us();
        uint64_t tick_elapsed_us;

        /* --- Soft timer callbacks first --- */
        wink_soft_timer_dispatch();

        /* --- Run user loop callback with individual WCET monitoring --- */
        wink_runtime_monitor_wcet_loop(callbacks->loop, "app_loop");

        /* --- Global tick WCET check (backup safety net) --- */
        tick_elapsed_us = pal_get_us() - tick_start_us;
        if (tick_elapsed_us > WINK_RUNTIME_TICK_MS * 1000U) {
            wink_trace_fault(WINK_WARN_TICK_OVERRUN);
        }

        /* Method C: Wasm interrupt dispatch at tick boundary (before delay/Asyncify suspend).
         * Wasm is in normal running state here (not Asyncify sleeping), so ISR dispatch is safe.
         * Equivalent to ESP32/FreeRTOS bottom-half queue consumption (ADR-0002).
         * Non-SIMULATION targets (host/esp32) have this removed at compile time -- zero overhead. */
#ifdef SIMULATION
        pal_wasm_dispatch_pending_interrupts();
#endif

        wink_app_delay_ms(WINK_RUNTIME_TICK_MS);
        tick++;
    }
    return WINK_OK;
}

void wink_runtime_fault(const wink_app_callbacks_t* callbacks, uint32_t fault_code) {
    wink_trace_fault(fault_code);
    wink_actuator_safe_off_all();
    if (callbacks != NULL && callbacks->on_fault != NULL) {
        callbacks->on_fault(fault_code);
    }
}
