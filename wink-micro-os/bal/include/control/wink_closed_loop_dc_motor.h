// SPDX-License-Identifier: Apache-2.0
#ifndef WINK_CLOSED_LOOP_DC_MOTOR_H
#define WINK_CLOSED_LOOP_DC_MOTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "actuator/dal_dc_motor.h"
#include "sensor/dal_encoder.h"
#include "math/wink_pid.h"
#include "wink_bal_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Closed-loop DC motor control configuration struct
 */
typedef struct {
    wink_pid_config_t pid_cfg;  /**< PID controller config */
    uint32_t period_ms;         /**< Control loop period in ms */
    uint32_t timeout_ms;        /**< Encoder feedback timeout threshold in ms */
    float counts_per_rev;       /**< Encoder pulses per revolution */
} wink_closed_loop_dc_motor_config_t;

/**
 * @brief Start closed-loop DC motor control session
 *
 * @param[in,out] motor DC motor driver handle.
 * @param[in,out] encoder Encoder sensor handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_closed_loop_dc_motor_start(dal_dc_motor_t *motor, 
                                           dal_encoder_t *encoder,
                                           const wink_closed_loop_dc_motor_config_t *cfg);

/**
 * @brief Start closed-loop DC motor control session with options
 *
 * @param[in,out] motor DC motor driver handle.
 * @param[in,out] encoder Encoder sensor handle.
 * @param[in] cfg Configuration struct.
 * @param[in] opts Options struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_closed_loop_dc_motor_start_ex(dal_dc_motor_t *motor, 
                                              dal_encoder_t *encoder,
                                              const wink_closed_loop_dc_motor_config_t *cfg,
                                              const wink_bal_opts_t *opts);

/**
 * @brief Stop closed-loop DC motor control session
 *
 * @param[in,out] motor DC motor driver handle.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t wink_closed_loop_dc_motor_stop(dal_dc_motor_t *motor);

/**
 * @brief Set closed-loop target speed
 *
 * @param[in,out] motor DC motor driver handle.
 * @param[in] target_speed Target speed in counts/sec.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t wink_closed_loop_dc_motor_set_speed(dal_dc_motor_t *motor, float target_speed);

/**
 * @brief Get estimated current speed
 *
 * @param[in] motor DC motor driver handle.
 * @param[out] out_speed Output pointer for current speed in counts/sec.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t wink_closed_loop_dc_motor_get_speed(dal_dc_motor_t *motor, float *out_speed);

#if defined(PLATFORM_host) || defined(SIMULATION)
WINK_WARN_UNUSED_RESULT
wink_status_t wink_closed_loop_dc_motor_debug_get_integral(dal_dc_motor_t *motor,
                                                       float *out_integral);
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINK_CLOSED_LOOP_DC_MOTOR_H */
