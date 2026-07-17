#include "wink_pid.h"
#include <string.h>

void wink_pid_init(wink_pid_t *pid, float kp, float ki, float kd, float min_output, float max_output)
{
    if (!pid) {
        return;
    }
    memset(pid, 0, sizeof(wink_pid_t));
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->min_output = min_output;
    pid->max_output = max_output;
    
    /* 默认积分上下限与输出上下限一致 */
    pid->min_integral = min_output;
    pid->max_integral = max_output;
}

float wink_pid_update(wink_pid_t *pid, float setpoint, float feedback, float dt)
{
    if (!pid || dt <= 0.0f) {
        return 0.0f;
    }
    
    float error = setpoint - feedback;
    
    /* P项 */
    float p_out = pid->kp * error;
    
    /* I项 (梯形/矩形积分，这里采用标准矩形积分积分累积) */
    pid->integral += error * dt;
    
    /* 积分限幅 (Anti-windup) */
    float i_term = pid->ki * pid->integral;
    if (i_term > pid->max_integral) {
        i_term = pid->max_integral;
        pid->integral = i_term / (pid->ki != 0.0f ? pid->ki : 1.0f);
    } else if (i_term < pid->min_integral) {
        i_term = pid->min_integral;
        pid->integral = i_term / (pid->ki != 0.0f ? pid->ki : 1.0f);
    }
    
    /* D项 */
    float derivative = (error - pid->prev_error) / dt;
    float d_out = pid->kd * derivative;
    
    pid->prev_error = error;
    
    /* 计算总输出 */
    float output = p_out + i_term + d_out;
    
    /* 输出限幅 */
    if (output > pid->max_output) {
        output = pid->max_output;
    } else if (output < pid->min_output) {
        output = pid->min_output;
    }
    
    return output;
}

void wink_pid_reset(wink_pid_t *pid)
{
    if (!pid) {
        return;
    }
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
}
