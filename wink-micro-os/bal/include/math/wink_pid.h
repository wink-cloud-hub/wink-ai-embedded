// SPDX-License-Identifier: Apache-2.0
#ifndef WINK_PID_H
#define WINK_PID_H

#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PID controller static configuration struct
 */
typedef struct {
    float kp;               /**< Proportional gain */
    float ki;               /**< Integral gain */
    float kd;               /**< Derivative gain */
    float min_output;       /**< Minimum output limit */
    float max_output;       /**< Maximum output limit */
    float min_integral;     /**< Minimum integral limit (anti-windup) */
    float max_integral;     /**< Maximum integral limit (anti-windup) */
} wink_pid_config_t;

/**
 * @brief PID controller runtime state struct
 */
typedef struct {
    wink_pid_config_t cfg;  /**< Configuration copy */
    float integral;         /**< Integral accumulator */
    float prev_feedback;    /**< Previous feedback value */
    bool first_run;         /**< First run flag */
} wink_pid_t;

/**
 * @brief Initialize PID controller instance
 *
 * @param[out] pid PID controller state handle.
 * @param[in] cfg Configuration struct.
 */
void wink_pid_init(wink_pid_t *pid, const wink_pid_config_t *cfg);

/**
 * @brief Compute PID update step (Derivative-on-Measurement)
 *
 * @param[in,out] pid PID controller state handle.
 * @param[in] setpoint Target setpoint value.
 * @param[in] feedback Current feedback value.
 * @param[in] dt Time step in seconds.
 * @param[out] out_output Output buffer for calculated output.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_pid_update(wink_pid_t *pid, float setpoint, float feedback, float dt, float *out_output);

/**
 * @brief Reset PID integral accumulator and error history
 *
 * @param[in,out] pid PID controller state handle.
 */
void wink_pid_reset(wink_pid_t *pid);

#ifdef __cplusplus
}
#endif

#endif /* WINK_PID_H */
