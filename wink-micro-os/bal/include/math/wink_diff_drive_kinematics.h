// SPDX-License-Identifier: Apache-2.0
#ifndef WINK_DIFF_DRIVE_KINEMATICS_H
#define WINK_DIFF_DRIVE_KINEMATICS_H

#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Differential drive chassis physical/kinematic parameters
 */
typedef struct {
    float wheel_base;      /**< Wheel base distance in meters (W) */
    float wheel_radius;    /**< Wheel radius in meters (R) */
    float counts_per_rev;  /**< Total encoder pulses per wheel revolution */
} wink_diff_drive_params_t;

/**
 * @brief Inverse kinematics: calculate wheel speeds from chassis target speeds
 *
 * @param[in] params Kinematics parameters.
 * @param[in] linear_v Chassis linear velocity in m/s.
 * @param[in] angular_w Chassis angular velocity in rad/s.
 * @param[out] out_vl Output left wheel speed in m/s.
 * @param[out] out_vr Output right wheel speed in m/s.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t wink_diff_drive_to_wheel_speeds(const wink_diff_drive_params_t *params,
                                             float linear_v,
                                             float angular_w,
                                             float *out_vl,
                                             float *out_vr);

/**
 * @brief Forward kinematics: calculate chassis speeds from wheel actual speeds
 *
 * @param[in] params Kinematics parameters.
 * @param[in] vl Left wheel speed in m/s.
 * @param[in] vr Right wheel speed in m/s.
 * @param[out] out_linear_v Output chassis linear velocity in m/s.
 * @param[out] out_angular_w Output chassis angular velocity in rad/s.
 * @return WINK_OK on success, error status code otherwise.
 */
wink_status_t wink_diff_drive_to_chassis_speeds(const wink_diff_drive_params_t *params,
                                               float vl,
                                               float vr,
                                               float *out_linear_v,
                                               float *out_angular_w);

/**
 * @brief Convert wheel linear speed (m/s) to encoder pulse speed (counts/s)
 *
 * @param[in] params Kinematics parameters.
 * @param[in] speed_m_s Wheel linear speed in m/s.
 * @return Pulse speed in counts/s.
 */
float wink_diff_drive_speed_to_counts(const wink_diff_drive_params_t *params, float speed_m_s);

/**
 * @brief Convert encoder pulse speed (counts/s) to wheel linear speed (m/s)
 *
 * @param[in] params Kinematics parameters.
 * @param[in] counts_s Pulse speed in counts/s.
 * @return Wheel linear speed in m/s.
 */
float wink_diff_drive_counts_to_speed(const wink_diff_drive_params_t *params, float counts_s);

#ifdef __cplusplus
}
#endif

#endif /* WINK_DIFF_DRIVE_KINEMATICS_H */
