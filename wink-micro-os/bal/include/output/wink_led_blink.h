/**
 * @file wink_led_blink.h
 * @brief BAL: blink a DAL LED via the runtime periodic scheduler.
 *
 * Lives in the BAL (Business Abstraction Layer) rather than samples/common
 * because periodic LED blink is a building block used by essentially every
 * sample and production app; centralising it avoids re-inventing the
 * soft_timer / periodic slot bookkeeping in user code.
 *
 * Layering (ADR-0023 §1): this header MUST NOT include any pal_*.h.
 * Core-affinity types go through wink_bal_opts.h (wink_bal_core_t); the
 * implementation maps to pal_os_core_id_t internally.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_LED_BLINK_H
#define WINK_LED_BLINK_H

#include <stdint.h>
#include "wink_status.h"
#include "dal_led.h"
#include "wink_bal_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default / overridable slot-pool size.
 *
 * Defined at compile time so memory-constrained builds can shrink the pool;
 * defaults to 4 (matches the original prior helper).
 */
#ifndef WINK_LED_BLINK_MAX
#define WINK_LED_BLINK_MAX  4
#endif

/**
 * @brief Start a periodic 50%-duty blink on @p led.
 *
 * Fire-and-forget: the return value is optional. Failures are logged
 * internally at LOG_D level (visible symptom = the LED simply does not blink).
 * Callers that need to stop the blink should capture the handle; callers that
 * blink for the lifetime of the process may safely ignore the return.
 *
 * Scheduling: blink runs on the LIGHT (soft-timer) path.  The callback is
 * pure non-blocking (dal_led_on/off only), so the default heuristic and the
 * explicit LIGHT flag agree.
 *
 * @param led        Initialised LED instance (must remain valid until stop()).
 * @param period_ms  Full blink period (on+off).  Must be a multiple of
 *                   WINK_RUNTIME_TICK_MS (10ms) for exact timing; smaller
 *                   values are clamped to one tick (10ms) per half-period.
 * @return >=1 blink handle on success; <0 WINK_ERR_* on failure
 *         (logged internally at LOG_D; safe to ignore).
 *
 * @note This is the simple wrapper; use wink_led_blink_start_ex() for
 *       explicit stack/priority/core/flags control.
 */
int32_t wink_led_blink_start(dal_led_t *led, uint32_t period_ms);

/**
 * @brief Extended start with explicit helper options.
 *
 * @param led        Initialised LED instance.
 * @param period_ms  Full blink period (on+off), same semantics as _start().
 * @param opts       Helper options (stack/priority/core/flags).  Pass NULL
 *                   for defaults (equivalent to wink_led_blink_start()).
 *                   NOTE: blink is purely non-blocking; WINK_PERIODIC_MAY_BLOCK
 *                   is accepted but unnecessary and will simply allocate a
 *                   dedicated task for a trivial toggle (wasteful — LIGHT
 *                   recommended).
 * @return >=1 blink handle on success; <0 WINK_ERR_* on failure.
 */
int32_t wink_led_blink_start_ex(dal_led_t *led, uint32_t period_ms,
                                const wink_bal_opts_t *opts);

/**
 * @brief Stop a blink previously started with wink_led_blink_start/_ex().
 *
 * Idempotent: stopping an already-stopped / invalid / negative (error-code)
 * handle is a silent no-op.  The context slot is freed for reuse, fixing
 * the LIFO/s_next slot-leak bug in the original prior helper.
 *
 * @param handle  Value returned by wink_led_blink_start/_ex().
 */
void wink_led_blink_stop(int32_t handle);

#ifdef __cplusplus
}
#endif

#endif /* WINK_LED_BLINK_H */
