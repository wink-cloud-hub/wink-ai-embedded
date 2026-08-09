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

/* pal_wasm_get_pwm_duty_percent relocated to pal_wasm_ch2b_pwm.c */

void wasm_dev_servo_set_duty(uint8_t channel, float duty_cycle_percent) {
    if (channel >= MAX_PWM_CHANNELS) {
        return;
    }
    s_pwm_duty_percent[channel] = duty_cycle_percent;
}

