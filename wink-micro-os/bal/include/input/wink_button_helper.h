/**
 * @file wink_button_helper.h
 * @brief BAL helper: automatic DAL button polling via the runtime LIGHT
 *        (soft-timer) periodic scheduler.
 *
 * Opt-in helper that wires a periodic LIGHT callback to call
 * dal_button_poll() so app_loop() does not need to poll manually. Useful
 * for lowcode/generated apps where the business loop may not remember to
 * poll.
 *
 * Lives in the BAL (Business Abstraction Layer) rather than samples/common
 * because periodic button polling is a building block used by essentially
 * every input-bearing sample and production app; centralising it avoids
 * re-inventing the soft_timer / periodic slot bookkeeping in user code.
 *
 * Layering (ADR-0023 §1): this header MUST NOT include any pal_*.h.
 *
 * @warning **Context constraint (read before using):**
 *    The LIGHT callback fires inside the runtime tick, BEFORE app_loop.
 *    Any event callback registered via dal_button_on_event() will therefore
 *    execute in this timer context, which has the same restrictions as an
 *    ISR-like handler:
 *      - Do NOT call any blocking or yielding API (sleep, mutex_lock,
 *        heavy printf, I2C/SPI blocking transfers, pal_os_delay_ms).
 *      - Do NOT perform more than ~100 us of work (the entire tick budget
 *        is 10 ms and is shared with all timers, polls, and app_loop).
 *    Defer heavy work to a dedicated task or to app_loop by posting state.
 *
 *    If you cannot honour these constraints, call dal_button_poll()
 *    manually from app_loop() instead.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_BUTTON_HELPER_H
#define WINK_BUTTON_HELPER_H

#include <stdint.h>
#include "wink_status.h"
#include "dal_button.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default / overridable slot-pool size.
 *
 * Defined at compile time so memory-constrained builds can shrink the pool;
 * defaults to 4 (matches the original samples/common helper; codegen can
 * size this precisely from the device tree via -DWINK_BUTTON_HELPER_MAX=N).
 */
#ifndef WINK_BUTTON_HELPER_MAX
#define WINK_BUTTON_HELPER_MAX 4
#endif

/**
 * @brief Start automatic polling on @p btn every @p poll_ms.
 *
 * Sets up a periodic LIGHT callback that calls dal_button_poll() on every
 * tick. Event callbacks registered via dal_button_on_event() fire from
 * inside the LIGHT callback — see the file-level @warning for context
 * restrictions.
 *
 * @param btn      Initialised dal_button_t instance.
 * @param poll_ms  Poll period in milliseconds. Must be a multiple of
 *                 WINK_RUNTIME_TICK_MS (10 ms) for exact timing;
 *                 recommended range 10-50 ms (debounce → responsiveness).
 * @return WINK_OK on success.
 *         WINK_ERR_INVALID_ARG        btn is NULL or poll_ms is 0.
 *         WINK_ERR_INVALID_STATE      btn already has auto-poll running
 *                                     (call wink_button_helper_stop first).
 *         WINK_ERR_RESOURCE_EXHAUSTED static slot pool full
 *                                     (WINK_BUTTON_HELPER_MAX).
 *         Other codes propagated from wink_periodic_start_ex.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_button_helper_start(dal_button_t *btn, uint32_t poll_ms);

/**
 * @brief Stop automatic polling on @p btn.
 *
 * Idempotent: if @p btn is not being auto-polled (or is NULL) this is a
 * no-op returning WINK_OK. The context slot is freed for reuse, and the
 * underlying periodic handle is destroyed (fixing the timer-slot leak in
 * the original samples/common helper that called only stop without
 * destroy).
 *
 * @param btn  Button instance (NULL-safe).
 * @return WINK_OK
 */
wink_status_t wink_button_helper_stop(dal_button_t *btn);

#ifdef __cplusplus
}
#endif

#endif /* WINK_BUTTON_HELPER_H */
