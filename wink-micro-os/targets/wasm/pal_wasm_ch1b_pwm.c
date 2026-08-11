// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_ch1b_pwm.c
 * @brief Wasm target Axis A (CH1b) PWM duty-cycle output & observation implementation.
 */

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "pal_hal.h"
#include "pal_pwm_router.h"
#include "pal_wasm_common.h"
#include "wasm_bridge.h"

#define WASM_PWM_MAX_CHANNELS 16

static float s_pwm_duty_percent[WASM_PWM_MAX_CHANNELS] = {0.0f};

wink_status_t pal_pwm_init(uint8_t channel, uint32_t frequency_hz)
{
    pal_pwm_config_t cfg = { .freq_hz = frequency_hz };
    return pal_pwm_init_ex(channel, &cfg);
}

wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg)
{
    if (cfg == NULL || cfg->freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->clock_requirement == PAL_PWM_CLOCK_STABLE_REQUIRED) {
        return WINK_ERR_UNSUPPORTED;
    }

    uint8_t bits = cfg->resolution_bits ? cfg->resolution_bits : 13u;
    if (bits == 0u || bits > 20u) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_pwm_timer_profile_t prof = {
        .freq_hz = cfg->freq_hz,
        .resolution_bits = bits,
        .clock_source = PAL_PWM_EFF_CLK_PLATFORM_AUTO,
    };
    uint8_t timer_num = 0;
    return pal_pwm_router_acquire(channel, &prof, &timer_num);
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_cycle_percent)
{
    if (channel >= WASM_PWM_MAX_CHANNELS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (duty_cycle_percent < 0.0f || duty_cycle_percent > 100.0f) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!pal_pwm_router_channel_ready(channel)) {
        return WINK_ERR_INVALID_STATE;
    }
    s_pwm_duty_percent[channel] = duty_cycle_percent;
    js_pal_pwm_set_duty(channel, duty_cycle_percent);
    return WINK_OK;
}

wink_status_t pal_pwm_set_freq(uint8_t channel, uint32_t freq_hz)
{
    if (!pal_pwm_router_channel_ready(channel) || freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    return pal_pwm_router_set_freq(channel, freq_hz);
}

void pal_pwm_deinit(uint8_t channel)
{
    if (channel < WASM_PWM_MAX_CHANNELS) {
        s_pwm_duty_percent[channel] = 0.0f;
    }
    pal_pwm_router_release(channel);
}

wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin)
{
    if (out_pin == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (channel >= WASM_PWM_MAX_CHANNELS) {
        return WINK_ERR_INVALID_ARG;
    }
    return WINK_ERR_UNSUPPORTED;
}

EMSCRIPTEN_KEEPALIVE
float pal_wasm_get_pwm_duty_percent(uint8_t channel)
{
    if (channel >= WASM_PWM_MAX_CHANNELS) {
        return 0.0f;
    }
    return s_pwm_duty_percent[channel];
}

void pal_wasm_ch1b_pwm_reset(void)
{
    memset(s_pwm_duty_percent, 0, sizeof(s_pwm_duty_percent));
}
