/**
 * @file wasm_dev_servo.c
 * @brief Wasm 仿真侧 SG90 舵机虚拟外设模型 (C-side Model)。
 */
#include "wasm_sim_registry.h"
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define MAX_PWM_CHANNELS 16

// 记录每个 PWM 通道对应的舵机虚拟角度
static float s_virtual_servo_angles[MAX_PWM_CHANNELS];
// 记录每个 PWM 通道的原始占空比百分比
static float s_pwm_duty_percent[MAX_PWM_CHANNELS];

void wasm_dev_servo_reset(void) {
    for (int i = 0; i < MAX_PWM_CHANNELS; i++) {
        s_virtual_servo_angles[i] = 0.0f;
        s_pwm_duty_percent[i] = 0.0f;
    }
}

// 供 JS 侧获取舵机角度的导出接口
EMSCRIPTEN_KEEPALIVE float pal_wasm_get_servo_angle(uint8_t channel) {
    if (channel >= MAX_PWM_CHANNELS) {
        return 0.0f;
    }
    return s_virtual_servo_angles[channel];
}

// 供 JS 侧获取 PWM 占空比的导出接口
EMSCRIPTEN_KEEPALIVE float pal_wasm_get_pwm_duty_percent(uint8_t channel) {
    if (channel >= MAX_PWM_CHANNELS) {
        return 0.0f;
    }
    return s_pwm_duty_percent[channel];
}

// 模拟 PWM 设置角度转换
void wasm_dev_servo_set_duty(uint8_t channel, float duty_cycle_percent) {
    if (channel >= MAX_PWM_CHANNELS) {
        return;
    }

    s_pwm_duty_percent[channel] = duty_cycle_percent;

    // 假设舵机控制周期为标准 20ms (50Hz)
    // 脉宽 (ms) = (duty_cycle_percent / 100.0f) * 20.0f
    // 脉宽 (us) = duty_cycle_percent * 200.0f
    float pulse_us = duty_cycle_percent * 200.0f;

    // SG90 舵机: 500us -> 0度，2500us -> 180度
    float angle = 0.0f;
    if (pulse_us <= 500.0f) {
        angle = 0.0f;
    } else if (pulse_us >= 2500.0f) {
        angle = 180.0f;
    } else {
        angle = (pulse_us - 500.0f) * 180.0f / 2000.0f;
    }

    s_virtual_servo_angles[channel] = angle;
}
