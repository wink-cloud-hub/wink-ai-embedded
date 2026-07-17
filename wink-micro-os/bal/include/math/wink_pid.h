#ifndef WINK_PID_H
#define WINK_PID_H

#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PID 静态配置参数（只读，可驻留于 Flash 或通过 NVS 反序列化）
 */
typedef struct {
    float kp;               /* 比例增益 */
    float ki;               /* 积分增益 */
    float kd;               /* 微分增益 */
    float min_output;       /* 控制输出下限限制 */
    float max_output;       /* 控制输出上限限制 */
    float min_integral;     /* 积分限幅下限（积分防饱和） */
    float max_integral;     /* 积分限幅上限 */
} wink_pid_config_t;

/**
 * @brief PID 运行期动态状态（驻留于 RAM，由算法自身维护）
 */
typedef struct {
    wink_pid_config_t cfg;  /* 当前运行期生效的配置 */
    float integral;         /* 积分项累加器 */
    float prev_feedback;    /* 上一次的实测反馈值 */
    bool first_run;         /* 首次运行标志，用于避免初次计算 D 项时产生阶跃突变 */
} wink_pid_t;

/**
 * @brief 初始化 PID 状态
 * 
 * @param pid PID 状态结构体指针
 * @param cfg 参数配置结构体指针
 */
void wink_pid_init(wink_pid_t *pid, const wink_pid_config_t *cfg);

/**
 * @brief 更新 PID 环路计算（反馈微分模式）
 * 
 * @param pid PID 状态结构体指针
 * @param setpoint 目标设定值
 * @param feedback 当前实测反馈值
 * @param dt 运行步长时间（秒，由实测得出）
 * @param out_output 计算输出值接收缓冲区
 * @return WINK_OK / WINK_ERR_INVALID_ARG (指针为 NULL 或 dt <= 0.0f)
 * 
 * @note 核心算法规范：
 *   1. 采用 Derivative-on-Measurement：微分项仅针对 feedback 计算（d_feedback / dt），
 *      避免设定值（setpoint）发生阶跃时输出产生微分震荡（微分冲击）。
 *   2. Anti-windup 采用前限制饱和：计算积分累加后，先限制其对 I-Term 的贡献，再反向约束累加器。
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_pid_update(wink_pid_t *pid, float setpoint, float feedback, float dt, float *out_output);

/**
 * @brief 重置 PID 积分与历史误差（用于切模式或停机时复位）
 * 
 * @param pid PID 状态结构体指针
 */
void wink_pid_reset(wink_pid_t *pid);

#ifdef __cplusplus
}
#endif

#endif /* WINK_PID_H */
