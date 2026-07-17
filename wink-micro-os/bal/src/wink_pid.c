#include "math/wink_pid.h"
#include <string.h>

void wink_pid_init(wink_pid_t *pid, const wink_pid_config_t *cfg)
{
    if (pid == NULL || cfg == NULL) {
        return;
    }
    memset(pid, 0, sizeof(wink_pid_t));
    memcpy(&pid->cfg, cfg, sizeof(wink_pid_config_t));
    pid->first_run = true;
}

wink_status_t wink_pid_update(wink_pid_t *pid, float setpoint, float feedback, float dt, float *out_output)
{
    if (pid == NULL || out_output == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (dt <= 0.0f) {
        return WINK_ERR_INVALID_ARG;
    }

    float error = setpoint - feedback;

    /* 1. Proportional term (P) */
    float p_out = pid->cfg.kp * error;

    /* 2. Integral term (I) with Anti-windup */
    pid->integral += error * dt;
    float i_out = pid->cfg.ki * pid->integral;

    if (i_out > pid->cfg.max_integral) {
        i_out = pid->cfg.max_integral;
        pid->integral = (pid->cfg.ki != 0.0f) ? (i_out / pid->cfg.ki) : 0.0f;
    } else if (i_out < pid->cfg.min_integral) {
        i_out = pid->cfg.min_integral;
        pid->integral = (pid->cfg.ki != 0.0f) ? (i_out / pid->cfg.ki) : 0.0f;
    }

    /* 3. Derivative term (D) on Measurement (Feedback) */
    float d_out = 0.0f;
    if (pid->first_run) {
        pid->first_run = false;
        pid->prev_feedback = feedback;
    } else {
        /* Derivative on measurement: error = setpoint - feedback.
         * Assuming setpoint is constant, d_error/dt = -d_feedback/dt.
         * This prevents derivative kick when setpoint steps. */
        float d_feedback = feedback - pid->prev_feedback;
        d_out = -pid->cfg.kd * (d_feedback / dt);
        pid->prev_feedback = feedback;
    }

    /* 4. Total Output calculation and output clamping */
    float output = p_out + i_out + d_out;
    if (output > pid->cfg.max_output) {
        output = pid->cfg.max_output;
    } else if (output < pid->cfg.min_output) {
        output = pid->cfg.min_output;
    }

    *out_output = output;
    return WINK_OK;
}

void wink_pid_reset(wink_pid_t *pid)
{
    if (pid == NULL) {
        return;
    }
    pid->integral = 0.0f;
    pid->prev_feedback = 0.0f;
    pid->first_run = true;
}
