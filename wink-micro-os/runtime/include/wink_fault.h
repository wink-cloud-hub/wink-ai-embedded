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
#define WINK_WARN_WCET_EXCEEDED        8002u   /* warn: single callback >5ms */
#define WINK_WARN_TICK_OVERRUN         8003u   /* warn: whole tick >10ms */

/* ── DAL drivers 9000-9899 (100 slots per driver) ─────────────── */
#define WINK_FAULT_DAL_LED             9100u
#define WINK_FAULT_DAL_BUTTON          9200u
#define WINK_FAULT_DAL_SERVO           9300u
#define WINK_FAULT_DAL_ULTRASONIC      9400u
#define WINK_FAULT_DAL_SSD1306         9500u
#define WINK_FAULT_DAL_EEPROM          9600u
#define WINK_FAULT_DAL_GPS             9700u

/* ── App 10000+ ──────────────────────────────────────────────── */
#define WINK_FAULT_APP(n)              (10000u + (n))

/**
 * @brief Convenience: raise a fault if @p st is an error.
 *
 * Usage inside init/setup code:
 * @code
 *     WINK_CHECK(dal_led_init(&led, &cfg), WINK_FAULT_DAL_LED);
 * @endcode
 *
 * On error this calls wink_runtime_raise_fault(code), which triggers
 * actuator safe-off and dispatches to on_fault callback.  This macro does
 * NOT return — it lets control fall through so the caller keeps going
 * only when st == WINK_OK.
 *
 * Note: because wink_runtime_raise_fault() does not longjmp/abort on host
 * (it invokes on_fault and returns in the default fault path), execution
 * continues after the fault.  In init code this is usually fine because
 * subsequent calls fail fast on NOT_INITIALIZED.
 */
#define WINK_CHECK(st, code) do { \
    wink_status_t _st = (st); \
    if (wink_status_is_error(_st)) { wink_runtime_raise_fault(code); } \
} while (0)

/**
 * @brief Convenience for init functions that return wink_status_t:
 * return immediately on error.
 *
 * @code
 *     static wink_status_t app_init(void) {
 *         WINK_TRY(dal_led_init(&led, &cfg));
 *         WINK_TRY(dal_button_init(&btn, &btn_cfg));
 *         return WINK_OK;
 *     }
 * @endcode
 */
#define WINK_TRY(st) do { \
    wink_status_t _s = (st); \
    if (wink_status_is_error(_s)) return _s; \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* WINK_FAULT_H */
