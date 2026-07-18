/**
 * @file wink_ultrasonic_poll.h
 * @brief BAL: automatic periodic ultrasonic distance measurement via runtime periodic task.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_ULTRASONIC_POLL_H
#define WINK_ULTRASONIC_POLL_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "wink_bal_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINK_ULTRASONIC_POLL_MAX
# ifdef WINK_APP_MAX_ULTRASONIC_INSTANCES
#  define WINK_ULTRASONIC_POLL_MAX WINK_APP_MAX_ULTRASONIC_INSTANCES
# else
#  define WINK_ULTRASONIC_POLL_MAX 4
# endif
#endif

/**
 * @brief Start automatic periodic measurement on @p dev (A-class â€?no event queue).
 *
 * Does **not** post WINK_EVENT_DISTANCE_READY. For L1 queue consumption use
 * wink_ultrasonic_enable_distance_events() instead (ADR-0033). Same @p dev
 * cannot run both APIs at once (WINK_ERR_INVALID_STATE).
 *
 * @param dev        Initialised dal_ultrasonic_t instance.
 * @param period_ms  Measurement period in ms. Must be >= 50ms to avoid HC-SR04
 *                   acoustic crosstalk (runtime-enforced; values < 50 return
 *                   WINK_ERR_INVALID_ARG).
 * @return WINK_OK on success.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_ultrasonic_poll_start(dal_ultrasonic_t *dev, uint32_t period_ms);

/**
 * @brief Extended start with custom stack/prio/core options.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_ultrasonic_poll_start_ex(dal_ultrasonic_t *dev, uint32_t period_ms,
                                         const wink_bal_opts_t *opts);

/**
 * @brief Stop automatic periodic measurement on @p dev.
 */
wink_status_t wink_ultrasonic_poll_stop(dal_ultrasonic_t *dev);

/**
 * @brief Update the polling period of @p dev.
 */
wink_status_t wink_ultrasonic_poll_set_period(dal_ultrasonic_t *dev, uint32_t period_ms);

/**
 * @brief Check if polling is running on @p dev.
 */
bool wink_ultrasonic_poll_is_running(dal_ultrasonic_t *dev);

/**
 * @brief Reset the helper slot pool, stopping all active measurements. (Mainly for tests).
 */
void wink_ultrasonic_poll_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WINK_ULTRASONIC_POLL_H */
