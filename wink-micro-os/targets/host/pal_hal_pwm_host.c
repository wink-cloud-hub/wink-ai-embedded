// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_pwm_host.c
 * @brief Host target PAL HAL PWM subsystem implementation (LEDC simulation).
 */
#include "hal/pal_pwm.h"
#include "hal/pal_target_caps.h"
#include "hal/pal_pwm_router.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"
#include <stdint.h>
#include <stdbool.h>

extern void host_record_pwm(uint8_t channel, float duty);

static wink_pin_t s_host_pwm_pins[PAL_PWM_CHANNELS] = {
    -1, 4, 5, -1, -1, -1, -1, -1
};

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq) {
    pal_pwm_config_t cfg = { .freq_hz = freq };
    return pal_pwm_init_ex(channel, &cfg);
}

wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg) {
    if (channel >= PAL_PWM_CHANNELS || cfg == NULL || cfg->freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->clock_requirement == PAL_PWM_CLOCK_STABLE_REQUIRED) {
        return WINK_ERR_UNSUPPORTED;
    }

    uint8_t bits = cfg->resolution_bits ? cfg->resolution_bits : 13u;
    if (bits == 0u || bits > 20u) {
        return WINK_ERR_INVALID_ARG;
    }

    wink_pin_t pin = (cfg->pin != WINK_PIN_NC) ? cfg->pin : s_host_pwm_pins[channel];
    if (pin >= 0) {
        s_host_pwm_pins[channel] = pin;
    }

    pal_pwm_timer_profile_t prof = {
        .freq_hz = cfg->freq_hz,
        .resolution_bits = bits,
        .clock_source = PAL_PWM_EFF_CLK_PLATFORM_AUTO,
    };
    uint8_t timer_num = 0;
    wink_status_t rs = pal_pwm_router_acquire(channel, &prof, &timer_num);
    if (wink_status_is_error(rs)) { return rs; }
    return WINK_OK;
}

wink_status_t pal_pwm_set_duty_bp(uint8_t channel, uint16_t basis_points) {
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_STATE; }
    if (basis_points > 10000u) { return WINK_ERR_INVALID_ARG; }
    float duty = (float)basis_points / 100.0f;
    host_record_pwm(channel, duty);
    return WINK_OK;
}

#ifndef PAL_PWM_HIDE_FLOAT_API
wink_status_t pal_pwm_set_duty(uint8_t channel, float duty) {
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    if (duty < 0.0f || duty > 100.0f) { return WINK_ERR_INVALID_ARG; }
    host_record_pwm(channel, duty);
    return WINK_OK;
}
#endif

wink_status_t pal_pwm_set_freq(uint8_t channel, uint32_t freq_hz) {
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    if (!pal_pwm_router_channel_ready(channel) || freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    return pal_pwm_router_set_freq(channel, freq_hz);
}

wink_status_t pal_pwm_deinit(uint8_t channel) {
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_STATE; }
    pal_pwm_router_release(channel);
    return WINK_OK;
}

wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin) {
    if (out_pin == NULL) { return WINK_ERR_INVALID_ARG; }
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    if (s_host_pwm_pins[channel] < 0) {
        return WINK_ERR_UNSUPPORTED;
    }
    *out_pin = s_host_pwm_pins[channel];
    return WINK_OK;
}
