// SPDX-License-Identifier: Apache-2.0
/**
 * @file wasm_dev_servo.c
 * @brief Wasm simulation SG90 servo virtual peripheral model implementation.
 */
#include "wasm_sim_registry.h"
#include <stdio.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define MAX_PWM_CHANNELS 16

static float s_virtual_servo_angles[MAX_PWM_CHANNELS];
static float s_pwm_duty_percent[MAX_PWM_CHANNELS];

void wasm_dev_servo_reset(void) {
    for (int i = 0; i < MAX_PWM_CHANNELS; i++) {
        s_virtual_servo_angles[i] = 0.0f;
        s_pwm_duty_percent[i] = 0.0f;
    }
}

EMSCRIPTEN_KEEPALIVE float pal_wasm_get_servo_angle(uint8_t channel) {
    if (channel >= MAX_PWM_CHANNELS) {
        return 0.0f;
    }
    return s_virtual_servo_angles[channel];
}

EMSCRIPTEN_KEEPALIVE float pal_wasm_get_pwm_duty_percent(uint8_t channel) {
    if (channel >= MAX_PWM_CHANNELS) {
        return 0.0f;
    }
    return s_pwm_duty_percent[channel];
}

void wasm_dev_servo_set_duty(uint8_t channel, float duty_cycle_percent) {
    if (channel >= MAX_PWM_CHANNELS) {
        return;
    }

    s_pwm_duty_percent[channel] = duty_cycle_percent;

    float pulse_us = duty_cycle_percent * 200.0f;

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
