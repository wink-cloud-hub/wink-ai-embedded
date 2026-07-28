#ifndef WINK_CLOSED_LOOP_MOTOR_H
#define WINK_CLOSED_LOOP_MOTOR_H

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
 * @brief 闭环控制静态参数
 */
typedef struct {
    wink_pid_config_t pid_cfg;  /* 控制环 PID 配置 */
    uint32_t period_ms;         /* 闭环更新节拍（毫秒，通常 10~50ms） */
    uint32_t timeout_ms;        /* 反馈传感器失效超时阈值（毫秒）。若超时，强制脱扣制动 */
    float counts_per_rev;       /* 编码器物理线数（用于物理量单位转换） */
} wink_closed_loop_motor_config_t;

/**
 * @brief 启动闭环控制会话 (Class A)
 * 
 * @param motor 关联的目标电机（DAL 句柄，作为该会话的主 Key）
 * @param encoder 关联的编码器反馈源（DAL 句柄）
 * @param cfg 控制参数配置
 * @return
 *   WINK_OK                 启动成功，后台定时控制任务已激活。
 *   WINK_ERR_RESOURCE_EXHAUSTED  已达系统静态 Slot 容量上限（由 WINK_APP_MAX_MOTOR_INSTANCES 决定）。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_closed_loop_motor_start(dal_dc_motor_t *motor, 
                                           dal_encoder_t *encoder,
                                           const wink_closed_loop_motor_config_t *cfg);

/**
 * @brief 启动闭环控制会话（高级专家版，支持重写运行核心与栈大小）
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_closed_loop_motor_start_ex(dal_dc_motor_t *motor, 
                                              dal_encoder_t *encoder,
                                              const wink_closed_loop_motor_config_t *cfg,
                                              const wink_bal_opts_t *opts);

/**
 * @brief 停止闭环控制会话 (Class A)
 * @note 停止控制会话的同时，会调用底层 dal_dc_motor_safe_off（→ brake）并释放 Slot。
 */
wink_status_t wink_closed_loop_motor_stop(dal_dc_motor_t *motor);

/**
 * @brief 设置目标转速 (Class C)
 * 
 * @param motor 电机实例（必须已处于活动控制会话中）
 * @param target_speed 目标物理转速（单位统一钉死为：脉冲数/秒，即 counts/s）
 * @return WINK_OK / WINK_ERR_INVALID_STATE (闭环未激活)
 */
wink_status_t wink_closed_loop_motor_set_speed(dal_dc_motor_t *motor, float target_speed);
wink_status_t wink_closed_loop_motor_get_speed(dal_dc_motor_t *motor, float *out_speed);

/**
 * @brief Host-test observability: read active-session PID integrator (R-011).
 *
 * Not part of the production control contract; used by closed-loop host tests
 * to assert anti-windup (integrator stops climbing under saturation).
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_closed_loop_motor_debug_get_integral(dal_dc_motor_t *motor,
                                                       float *out_integral);

#ifdef __cplusplus
}
#endif

#endif /* WINK_CLOSED_LOOP_MOTOR_H */
