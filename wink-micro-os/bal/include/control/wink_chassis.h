#ifndef WINK_CHASSIS_H
#define WINK_CHASSIS_H

#include "actuator/dal_motor.h"
#include "sensor/dal_encoder.h"
#include "control/wink_closed_loop_motor.h"
#include "math/wink_diff_drive_kinematics.h"
#include "wink_bal_opts.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 差速底盘控制器配置参数
 */
typedef struct {
    wink_diff_drive_params_t kinematics_params;  /* 几何参数，用于正逆运动学 */
    wink_closed_loop_motor_config_t left_motor_cfg;  /* 左轮闭环控制参数 */
    wink_closed_loop_motor_config_t right_motor_cfg; /* 右轮闭环控制参数 */
} wink_chassis_config_t;

/**
 * @brief 启动差速底盘控制器 (Class A)
 * 
 * @param left_motor 左侧电机实例句柄
 * @param left_encoder 左侧编码器实例句柄
 * @param right_motor 右侧电机实例句柄
 * @param right_encoder 右侧编码器实例句柄
 * @param cfg 控制参数配置
 * @return
 *   WINK_OK                 启动成功，底层左右闭环电机控制器已启动。
 *   WINK_ERR_RESOURCE_EXHAUSTED  Slot 池已满（由 WINK_CHASSIS_MAX 决定）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_chassis_start(dal_motor_t *left_motor, dal_encoder_t *left_encoder,
                                 dal_motor_t *right_motor, dal_encoder_t *right_encoder,
                                 const wink_chassis_config_t *cfg);

/**
 * @brief 启动差速底盘控制器（专家版，支持重写运行核心与栈大小）
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_chassis_start_ex(dal_motor_t *left_motor, dal_encoder_t *left_encoder,
                                    dal_motor_t *right_motor, dal_encoder_t *right_encoder,
                                    const wink_chassis_config_t *cfg,
                                    const wink_bal_opts_t *opts);

/**
 * @brief 停止底盘控制器 (Class A)
 * @note 停止的同时会停止底层左右闭环电机会话，并释放 Slot 空间。
 * @param left_motor 左电机句柄（作为该底盘会话的主 Key）
 */
wink_status_t wink_chassis_stop(dal_motor_t *left_motor);

/**
 * @brief 设置车体目标线速度与角速度 (Class C)
 * 
 * @param left_motor 左电机句柄（主 Key）
 * @param linear_v 期望车体中心线速度 (m/s)
 * @param angular_w 期望车体中心角速度 (rad/s)
 * @return WINK_OK / WINK_ERR_INVALID_STATE (底盘未激活)
 */
wink_status_t wink_chassis_set_velocity(dal_motor_t *left_motor, float linear_v, float angular_w);

/**
 * @brief 获取车体实测当前线速度与角速度（里程计估算）(Class C)
 * 
 * @param left_motor 左电机句柄（主 Key）
 * @param out_linear_v 接收车体当前实测线速度 (m/s)
 * @param out_angular_w 接收车体当前实测角速度 (rad/s)
 * @return WINK_OK / WINK_ERR_INVALID_STATE (底盘未激活)
 */
wink_status_t wink_chassis_get_velocity(dal_motor_t *left_motor, float *out_linear_v, float *out_angular_w);

#ifdef __cplusplus
}
#endif

#endif /* WINK_CHASSIS_H */
