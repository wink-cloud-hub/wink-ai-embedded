/**
 * @file wink_servo_helper.h
 * @brief BAL helper: sweep and control a DAL servo via the runtime scheduler.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_SERVO_HELPER_H
#define WINK_SERVO_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "dal_servo.h"
#include "wink_helper_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINK_SERVO_HELPER_MAX
# ifdef WINK_APP_MAX_SERVO_INSTANCES
#  define WINK_SERVO_HELPER_MAX WINK_APP_MAX_SERVO_INSTANCES
# else
#  define WINK_SERVO_HELPER_MAX 4
# endif
#endif

/**
 * @brief Start a periodic sweep on @p servo between @p min_angle and @p max_angle.
 *
 * @param servo      Initialised servo instance.
 * @param min_angle  Minimum sweep angle (degrees).
 * @param max_angle  Maximum sweep angle (degrees).
 * @param period_ms  Update period in ms. Recommended >= 20ms.
 * @return WINK_OK on success.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_servo_sweep_start(dal_servo_t *servo, float min_angle, float max_angle, uint32_t period_ms);

/**
 * @brief Extended sweep start with custom task options.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_servo_sweep_start_ex(dal_servo_t *servo, float min_angle, float max_angle, uint32_t period_ms,
                                        const wink_helper_opts_t *opts);

/**
 * @brief Stop the periodic sweep on @p servo.
 */
wink_status_t wink_servo_helper_stop(dal_servo_t *servo);

/**
 * @brief Update the sweep update period on @p servo.
 */
wink_status_t wink_servo_helper_set_period(dal_servo_t *servo, uint32_t period_ms);

/**
 * @brief Check if sweep is running on @p servo.
 */
bool wink_servo_helper_is_running(dal_servo_t *servo);

/**
 * @brief Set angle directly (oneshot, non-periodic).
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_servo_set_angle(dal_servo_t *servo, float angle);

/**
 * @brief Reset the helper slot pool, stopping all active sweeps. (Mainly for tests).
 */
void wink_servo_helper_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WINK_SERVO_HELPER_H */
