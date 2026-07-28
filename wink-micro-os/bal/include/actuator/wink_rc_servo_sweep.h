/**
 * @file wink_rc_servo_sweep.h
 * @brief BAL: sweep and control a DAL servo via the runtime scheduler.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_RC_SERVO_SWEEP_H
#define WINK_RC_SERVO_SWEEP_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"
#include "dal_rc_servo.h"
#include "wink_bal_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINK_RC_SERVO_SWEEP_MAX
# ifdef WINK_APP_MAX_SERVO_INSTANCES
#  define WINK_RC_SERVO_SWEEP_MAX WINK_APP_MAX_SERVO_INSTANCES
# else
#  define WINK_RC_SERVO_SWEEP_MAX 4
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
wink_status_t wink_rc_servo_sweep_start(dal_rc_servo_t *servo, float min_angle, float max_angle, uint32_t period_ms);

/**
 * @brief Extended sweep start with custom task options.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_rc_servo_sweep_start_ex(dal_rc_servo_t *servo, float min_angle, float max_angle, uint32_t period_ms,
                                        const wink_bal_opts_t *opts);

/**
 * @brief Stop the periodic sweep on @p servo.
 */
wink_status_t wink_rc_servo_sweep_stop(dal_rc_servo_t *servo);

/**
 * @brief Update the sweep update period on @p servo.
 */
wink_status_t wink_rc_servo_sweep_set_period(dal_rc_servo_t *servo, uint32_t period_ms);

/**
 * @brief Check if sweep is running on @p servo.
 */
bool wink_rc_servo_sweep_is_running(dal_rc_servo_t *servo);

/**
 * @brief Set angle directly (oneshot, non-periodic).
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_rc_servo_set_angle(dal_rc_servo_t *servo, float angle);

/**
 * @brief Reset the sweep slot pool, stopping all active sweeps. (Mainly for tests).
 */
void wink_rc_servo_sweep_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* WINK_RC_SERVO_SWEEP_H */
