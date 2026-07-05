/**
 * @file wink_button_helper.h
 * @brief BAL helper: soft_timer-driven automatic button polling.
 *
 * Opt-in helper that wires a soft_timer to periodically call
 * dal_button_poll() so app_loop() does not need to poll manually. Useful
 * for lowcode/generated apps where the business loop may not remember to
 * poll.
 *
 * @warning **Context constraint (read before using):**
 *    The soft_timer callback fires inside the runtime tick, BEFORE app_loop.
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
 * NOT part of the OS core — lives in samples/common because periodic
 * button polling is a common lowcode/BAL pattern.  Core wink_soft_timer
 * mechanism is always available for apps that want to roll their own.
 */
#ifndef WINK_BUTTON_HELPER_H
#define WINK_BUTTON_HELPER_H

#include <stdint.h>
#include "wink_status.h"
#include "dal_button.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINK_BUTTON_HELPER_MAX
/** @brief Maximum concurrent auto-polled buttons.  Override at compile
 *         time via -DWINK_BUTTON_HELPER_MAX=N (P1-4 codegen can size this
 *         precisely from the device tree). */
#define WINK_BUTTON_HELPER_MAX 4
#endif

/**
 * @brief Start automatic polling on @p btn every @p poll_ms.
 *
 * Sets up a periodic soft_timer that calls dal_button_poll() on every
 * tick.  Event callbacks registered via dal_button_on_event() fire from
 * inside the timer callback — see the file-level @warning for context
 * restrictions.
 *
 * @param btn      Initialised dal_button_t instance.
 * @param poll_ms  Poll period in milliseconds.  Must be a multiple of
 *                 WINK_RUNTIME_TICK_MS (10 ms) for exact timing;
 *                 recommended range 10-50 ms (debounce -> responsiveness).
 * @return WINK_OK on success.
 *         WINK_ERR_INVALID_ARG      btn is NULL or poll_ms is 0.
 *         WINK_ERR_INVALID_STATE    btn already has auto-poll running
 *                                   (call wink_button_helper_stop first).
 *         WINK_ERR_RESOURCE_EXHAUSTED  static slot pool full
 *                                      (WINK_BUTTON_HELPER_MAX).
 *         Other codes propagated from wink_soft_timer_create/start.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_button_helper_start(dal_button_t *btn, uint32_t poll_ms);

/**
 * @brief Stop automatic polling on @p btn.
 *
 * Idempotent: if @p btn is not being auto-polled (or is NULL) this is a
 * no-op returning WINK_OK.
 *
 * @param btn  Button instance (NULL-safe).
 * @return WINK_OK
 */
wink_status_t wink_button_helper_stop(dal_button_t *btn);

#ifdef __cplusplus
}
#endif

#endif /* WINK_BUTTON_HELPER_H */
