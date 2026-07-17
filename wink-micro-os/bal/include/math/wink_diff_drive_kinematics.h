#ifndef WINK_DIFF_DRIVE_KINEMATICS_H
#define WINK_DIFF_DRIVE_KINEMATICS_H

#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 双轮差速底盘物理/几何参数配置
 */
typedef struct {
    float wheel_base;      /* 两轮轴距（米，W） */
    float wheel_radius;    /* 车轮半径（米，R） */
    float counts_per_rev;  /* 车轮旋转一圈对应的编码器总脉冲数（ticks） */
} wink_diff_drive_params_t;

/**
 * @brief 逆运动学：输入车体期望线速度与角速度，计算左右轮的目标物理速度
 * 
 * @param params 几何参数
 * @param linear_v 车体中心线速度 (m/s)
 * @param angular_w 车体中心角速度 (rad/s)
 * @param out_vl 左轮目标线速度 (m/s)
 * @param out_vr 右轮目标线速度 (m/s)
 * @return WINK_OK / WINK_ERR_INVALID_ARG
 */
wink_status_t wink_diff_drive_to_wheel_speeds(const wink_diff_drive_params_t *params,
                                             float linear_v,
                                             float angular_w,
                                             float *out_vl,
                                             float *out_vr);

/**
 * @brief 正运动学：输入左右轮的实际物理速度，计算车体当前的线速度与角速度
 * 
 * @param params 几何参数
 * @param vl 左轮实际线速度 (m/s)
 * @param vr 右轮实际线速度 (m/s)
 * @param out_linear_v 计算得到的车体线速度 (m/s)
 * @param out_angular_w 计算得到的车体角速度 (rad/s)
 * @return WINK_OK / WINK_ERR_INVALID_ARG
 */
wink_status_t wink_diff_drive_to_chassis_speeds(const wink_diff_drive_params_t *params,
                                               float vl,
                                               float vr,
                                               float *out_linear_v,
                                               float *out_angular_w);

/**
 * @brief 单位换算：将车轮线速度 (m/s) 换算为编码器脉冲速度 (counts/s)
 * 
 * @param params 几何参数
 * @param speed_m_s 车轮线速度 (m/s)
 * @return 对应的脉冲速度 (counts/s)
 */
float wink_diff_drive_speed_to_counts(const wink_diff_drive_params_t *params, float speed_m_s);

/**
 * @brief 单位换算：将编码器脉冲速度 (counts/s) 换算为车轮实际线速度 (m/s)
 * 
 * @param params 几何参数
 * @param counts_s 脉冲速度 (counts/s)
 * @return 对应的车轮线速度 (m/s)
 */
float wink_diff_drive_counts_to_speed(const wink_diff_drive_params_t *params, float counts_s);

#ifdef __cplusplus
}
#endif

#endif /* WINK_DIFF_DRIVE_KINEMATICS_H */
