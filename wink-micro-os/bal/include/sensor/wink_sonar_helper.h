/**
 * @file wink_sonar_helper.h
 * @brief BAL helper: automatic periodic ultrasonic distance measurement via runtime periodic task.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_SONAR_HELPER_H
#define WINK_SONAR_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "wink_helper_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINK_SONAR_HELPER_MAX
# ifdef WINK_APP_MAX_ULTRASONIC_INSTANCES
#  define WINK_SONAR_HELPER_MAX WINK_APP_MAX_ULTRASONIC_INSTANCES
# else
#  define WINK_SONAR_HELPER_MAX 4
# endif
#endif

/**
 * @brief Start automatic periodic measurement on @p dev.
 *
 * @param dev        Initialised dal_ultrasonic_t instance.
 * @param period_ms  Measurement period in ms. Must be >= 50ms to avoid HC-SR04
 *                   acoustic crosstalk (runtime-enforced; values < 50 return
 *                   WINK_ERR_INVALID_ARG).
 * @return WINK_OK on success.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_sonar_helper_start(dal_ultrasonic_t *dev, uint32_t period_ms);

/**
 * @brief Extended start with custom stack/prio/core options.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_sonar_helper_start_ex(dal_ultrasonic_t *dev, uint32_t period_ms,
                                         const wink_helper_opts_t *opts);

/**
 * @brief Stop automatic periodic measurement on @p dev.
 */
wink_status_t wink_sonar_helper_stop(dal_ultrasonic_t *dev);

/**
 * @brief Update the polling period of @p dev.
 */
wink_status_t wink_sonar_helper_set_period(dal_ultrasonic_t *dev, uint32_t period_ms);

/**
 * @brief Check if polling is running on @p dev.
 */
bool wink_sonar_helper_is_running(dal_ultrasonic_t *dev);

/**
 * @brief Reset the helper slot pool, stopping all active measurements. (Mainly for tests).
 */
void wink_sonar_helper_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WINK_SONAR_HELPER_H */
