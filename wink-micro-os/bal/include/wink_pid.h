#ifndef WINK_PID_H
#define WINK_PID_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PID 控制器结构体
 */
typedef struct {
    float kp;             /* 比例系数 */
    float ki;             /* 积分系数 */
    float kd;             /* 微分系数 */
    
    float min_output;     /* 输出下限 */
    float max_output;     /* 输出上限 */
    
    float integral;       /* 积分误差累积 */
    float prev_error;     /* 上一次的误差 */
    
    float max_integral;   /* 积分项上限（防饱和，若不设则取 max_output） */
    float min_integral;   /* 积分项下限（防饱和，若不设则取 min_output） */
} wink_pid_t;

/**
 * @brief 初始化 PID 控制器参数
 * 
 * @param pid PID 结构体指针
 * @param kp 比例系数
 * @param ki 积分系数
 * @param kd 微分系数
 * @param min_output 最小输出限制
 * @param max_output 最大输出限制
 */
void wink_pid_init(wink_pid_t *pid, float kp, float ki, float kd, float min_output, float max_output);

/**
 * @brief 更新 PID 控制器，计算输出值
 * 
 * @param pid PID 结构体指针
 * @param setpoint 目标设定值
 * @param feedback 当前反馈值
 * @param dt 时间增量（秒）
 * @return float 控制输出值
 */
float wink_pid_update(wink_pid_t *pid, float setpoint, float feedback, float dt);

/**
 * @brief 重置 PID 控制器状态（积分项和历史误差归零）
 * 
 * @param pid PID 结构体指针
 */
void wink_pid_reset(wink_pid_t *pid);

#ifdef __cplusplus
}
#endif

#endif /* WINK_PID_H */
