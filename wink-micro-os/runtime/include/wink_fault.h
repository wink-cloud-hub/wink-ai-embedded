#ifndef WINK_FAULT_H
#define WINK_FAULT_H

/**
 * @file wink_fault.h
 * @brief Centralized fault-code namespace + fault/warn reporting helpers.
 *
 * Fault codes are 16-bit unsigned values partitioned by subsystem so that
 * logs from different layers never collide.  Ranges:
 *
 *   1000 - 1999  Reserved for future use
 *   2000 - 7999  Reserved (DAL slots below start at 9100 for legacy compat)
 *   8000 - 8999  Runtime core  (WINK_FAULT_RUNTIME(n) = 8000+n; preserves
 *                 legacy 8001/8002/8003 codes used by boot-safe-lock & WCET)
 *   9000 - 9999  DAL drivers   (WINK_FAULT_DAL_LED=9100, etc.; 100 slots/driver)
 *   10000 - 65535 App-defined  (WINK_FAULT_APP(n) = 10000+n)
 *
 * Warn codes use the same partition but go through wink_trace_warn()
 * instead of wink_runtime_raise_fault() — they log only, do NOT trigger
 * safe-off.
 *
 * Copyright (c) 2026 Wink-AI.
 */

#include <stdint.h>
#include "wink_status.h"
#include "wink_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Runtime core 8000-8999 (preserves legacy numeric values) ──── */
/* Note: the spec originally proposed 1xxx for runtime but existing code
 * and e2e tests hard-code 8001/8002/8003; we keep the numeric values to
 * avoid churn and number the macro aliases below accordingly. */
#define WINK_FAULT_RUNTIME(n)          (8000u + (n))
#define WINK_FAULT_BOOT_AFTER_RESET    8001u   /* WDT/PANIC → boot safe-lock */
#define WINK_WARN_WCET_EXCEEDED        8002u   /* warn: single callback >5ms (legacy coarse threshold) */
#define WINK_WARN_TICK_OVERRUN         8003u   /* warn: whole tick >10ms */
#define WINK_WARN_LIGHT_OVERBUDGET     8004u   /* warn: LIGHT cb >100µs budget */
#define WINK_FAULT_LIGHT_WCET_VIOLATION 8005u  /* fault: LIGHT cb >500µs hard limit (repeated or severe) */
#define WINK_FAULT_LIGHT_BLOCKING      8006u  /* fault: WINK_ASSERT_NONBLOCKING triggered inside LIGHT dispatch */

/* ── DAL drivers 9000-9899 (100 slots per driver) ─────────────── */
#define WINK_FAULT_DAL_LED             9100u
#define WINK_FAULT_DAL_BUTTON          9200u
/* warn: BAL button-events requested GPIO_IRQ drive but the current target
 * (host/wasm/non-ESP32) does not support IRQ dispatch; the runtime has
 * transparently degraded to SOFT_POLL. See ADR-0031. Countable via
 * wink_warn_count() (aggregate, not per-code). */
#define WINK_WARN_BUTTON_IRQ_DEGRADED  9201u
#define WINK_FAULT_DAL_SERVO           9300u
#define WINK_FAULT_DAL_ULTRASONIC      9400u
/* warn: wink_event_post of DISTANCE_READY failed (queue full). ADR-0033. */
#define WINK_WARN_DISTANCE_EVENT_QUEUE_FULL 9401u
#define WINK_FAULT_DAL_SSD1306         9500u
#define WINK_FAULT_DAL_EEPROM          9600u
#define WINK_FAULT_DAL_GPS             9700u

/* ── App 10000+ ──────────────────────────────────────────────── */
#define WINK_FAULT_APP(n)              (10000u + (n))

/**
 * @brief Raise a fault if @p st is an error; continue execution either way.
 *
 * Semantics: "check and complain, but keep going".
 *
 * - On success (st == WINK_OK):  no-op, execution falls through.
 * - On error:                    calls wink_runtime_raise_fault(code) to
 *                                trigger actuator safe-off + on_fault
 *                                callback, then execution CONTINUES past
 *                                the macro (no return, no longjmp).
 *
 * Use in init/setup code where a failed device init should be reported but
 * the caller intentionally continues (e.g. subsequent unrelated inits or
 * degraded-mode operation).
 *
 * @code
 *     // LED init failure is non-fatal for smoke telemetry
 *     WINK_CHECK(dal_led_init(&led, &cfg), WINK_FAULT_DAL_LED);
 * @endcode
 *
 * Compare with WINK_TRY() which RETURNS the error code to the caller.
 *
 * Note: on host builds wink_runtime_raise_fault() invokes on_fault and
 * returns (no abort), so subsequent calls typically fail fast with
 * NOT_INITIALIZED — that is the expected degraded path.
 */
#define WINK_CHECK(st, code) do { \
    wink_status_t _st = (st); \
    if (wink_status_is_error(_st)) { wink_runtime_raise_fault(code); } \
} while (0)

/**
 * @brief Return immediately from the enclosing function if @p st is an error.
 *
 * Semantics: "try, bail out on failure".
 *
 * - On success (st == WINK_OK):  no-op, execution falls through.
 * - On error:                    executes `return st;` so the enclosing
 *                                function propagates the error to its caller.
 *
 * Use in functions whose signature returns wink_status_t and where an error
 * should abort the remaining work (common for chained init sequences).
 *
 * @code
 *     static wink_status_t my_init(void) {
 *         WINK_TRY(dal_led_init(&led, &cfg));
 *         WINK_TRY(dal_button_init(&btn, &btn_cfg));
 *         return WINK_OK;
 *     }
 * @endcode
 *
 * Compare with WINK_CHECK() which reports the fault but does NOT return.
 */
#define WINK_TRY(st) do { \
    wink_status_t _s = (st); \
    if (wink_status_is_error(_s)) return _s; \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* WINK_FAULT_H */
