// SPDX-License-Identifier: Apache-2.0
#ifndef WINK_CHASSIS_H
#define WINK_CHASSIS_H

#include "actuator/dal_dc_motor.h"
#include "sensor/dal_encoder.h"
#include "control/wink_closed_loop_dc_motor.h"
#include "math/wink_diff_drive_kinematics.h"
#include "wink_bal_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Differential drive chassis controller configuration struct
 */
typedef struct {
    wink_diff_drive_params_t kinematics_params;  /**< Kinematics parameters */
    wink_closed_loop_dc_motor_config_t left_motor_cfg;  /**< Left wheel closed-loop motor config */
    wink_closed_loop_dc_motor_config_t right_motor_cfg; /**< Right wheel closed-loop motor config */
} wink_chassis_config_t;

/**
 * @brief Start differential drive chassis controller
 *
 * @param[in,out] left_motor Left DC motor instance handle.
 * @param[in,out] left_encoder Left encoder instance handle.
 * @param[in,out] right_motor Right DC motor instance handle.
 * @param[in,out] right_encoder Right encoder instance handle.
 * @param[in] cfg Configuration struct.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_chassis_start(dal_dc_motor_t *left_motor, dal_encoder_t *left_encoder,
                                 dal_dc_motor_t *right_motor, dal_encoder_t *right_encoder,
                                 const wink_chassis_config_t *cfg);

/**
 * @brief Start differential drive chassis controller with custom options
 *
 * @param[in,out] left_motor Left DC motor instance handle.
 * @param[in,out] left_encoder Left encoder instance handle.
 * @param[in,out] right_motor Right DC motor instance handle.
 * @param[in,out] right_encoder Right encoder instance handle.
 * @param[in] cfg Configuration struct.
 * @param[in] opts Execution options.
 * @return WINK_OK on success, error status code otherwise.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_chassis_start_ex(dal_dc_motor_t *left_motor, dal_encoder_t *left_encoder,
                                    dal_dc_motor_t *right_motor, dal_encoder_t *right_encoder,
                                    const wink_chassis_config_t *cfg,
                                    const wink_bal_opts_t *opts);

/**
 * @brief Stop differential drive chassis controller
 *
 * @param[in,out] left_motor Left DC motor handle (primary session key).
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t wink_chassis_stop(dal_dc_motor_t *left_motor);

/**
 * @brief Set chassis target linear velocity and angular velocity
 *
 * @param[in,out] left_motor Left DC motor handle.
 * @param[in] linear_v Target linear velocity in m/s.
 * @param[in] angular_w Target angular velocity in rad/s.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t wink_chassis_set_velocity(dal_dc_motor_t *left_motor, float linear_v, float angular_w);

/**
 * @brief Get estimated current chassis linear velocity and angular velocity
 *
 * @param[in] left_motor Left DC motor handle.
 * @param[out] out_linear_v Output pointer for linear velocity in m/s.
 * @param[out] out_angular_w Output pointer for angular velocity in rad/s.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t wink_chassis_get_velocity(dal_dc_motor_t *left_motor, float *out_linear_v, float *out_angular_w);

#ifdef __cplusplus
}
#endif

#endif /* WINK_CHASSIS_H */
