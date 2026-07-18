#include "math/wink_diff_drive_kinematics.h"
#include <stddef.h>

#ifndef M_PI
#  define M_PI 3.14159265358979323846f
#endif

wink_status_t wink_diff_drive_to_wheel_speeds(const wink_diff_drive_params_t *params,
                                             float linear_v,
                                             float angular_w,
                                             float *out_vl,
                                             float *out_vr)
{
    if (params == NULL || out_vl == NULL || out_vr == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (params->wheel_base <= 0.0f || params->wheel_radius <= 0.0f) {
        return WINK_ERR_INVALID_ARG;
    }

    // v_L = v - (w * W) / 2
    // v_R = v + (w * W) / 2
    float half_w = (angular_w * params->wheel_base) / 2.0f;
    *out_vl = linear_v - half_w;
    *out_vr = linear_v + half_w;

    return WINK_OK;
}

wink_status_t wink_diff_drive_to_chassis_speeds(const wink_diff_drive_params_t *params,
                                               float vl,
                                               float vr,
                                               float *out_linear_v,
                                               float *out_angular_w)
{
    if (params == NULL || out_linear_v == NULL || out_angular_w == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (params->wheel_base <= 0.0f || params->wheel_radius <= 0.0f) {
        return WINK_ERR_INVALID_ARG;
    }

    // v = (v_L + v_R) / 2
    // w = (v_R - v_L) / W
    *out_linear_v = (vl + vr) / 2.0f;
    *out_angular_w = (vr - vl) / params->wheel_base;

    return WINK_OK;
}

float wink_diff_drive_speed_to_counts(const wink_diff_drive_params_t *params, float speed_m_s)
{
    if (params == NULL || params->wheel_radius <= 0.0f || params->counts_per_rev <= 0.0f) {
        return 0.0f;
    }
    // C = 2 * PI * R
    float circumference = 2.0f * (float)M_PI * params->wheel_radius;
    // counts/s = (m/s) / C * counts_per_rev
    return (speed_m_s / circumference) * params->counts_per_rev;
}

float wink_diff_drive_counts_to_speed(const wink_diff_drive_params_t *params, float counts_s)
{
    if (params == NULL || params->wheel_radius <= 0.0f || params->counts_per_rev <= 0.0f) {
        return 0.0f;
    }
    // C = 2 * PI * R
    float circumference = 2.0f * (float)M_PI * params->wheel_radius;
    // m/s = (counts/s) / counts_per_rev * C
    return (counts_s / params->counts_per_rev) * circumference;
}
