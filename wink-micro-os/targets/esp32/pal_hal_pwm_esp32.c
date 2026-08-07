// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_pwm_esp32.c
 * @brief ESP32 target PAL HAL PWM (LEDC) subsystem implementation.
 */
#include "pal_hal.h"
#include "pal_pwm_router.h"

#if defined(ESP_PLATFORM)
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_rom_gpio.h"
#include "soc/gpio_sig_map.h"

#ifndef SIG_GPIO_OUT_IDX
#define SIG_GPIO_OUT_IDX 256
#endif

__attribute__((weak)) const wink_pin_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};

static uint8_t s_ch_bits[PAL_PWM_CHANNELS];

static bool pwm_map_ledc_bits(uint8_t bits, ledc_timer_bit_t *out)
{
    if (out == NULL) { return false; }
    switch (bits) {
        case 8:  *out = LEDC_TIMER_8_BIT;  return true;
        case 9:  *out = LEDC_TIMER_9_BIT;  return true;
        case 10: *out = LEDC_TIMER_10_BIT; return true;
        case 11: *out = LEDC_TIMER_11_BIT; return true;
        case 12: *out = LEDC_TIMER_12_BIT; return true;
        case 13: *out = LEDC_TIMER_13_BIT; return true;
        case 14: *out = LEDC_TIMER_14_BIT; return true;
        case 15: *out = LEDC_TIMER_15_BIT; return true;
        default: return false;
    }
}

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq_hz)
{
    pal_pwm_config_t cfg = { .freq_hz = freq_hz };
    return pal_pwm_init_ex(channel, &cfg);
}

wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg)
{
    if (cfg == NULL || cfg->freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }

    uint8_t bits = cfg->resolution_bits ? cfg->resolution_bits : 13u;
    ledc_timer_bit_t duty_res;
    if (!pwm_map_ledc_bits(bits, &duty_res)) {
        return WINK_ERR_INVALID_ARG;
    }

    uint8_t eff_clk = PAL_PWM_EFF_CLK_PLATFORM_AUTO;
    ledc_clk_cfg_t clk_cfg = LEDC_AUTO_CLK;
    if (cfg->clock_requirement == PAL_PWM_CLOCK_STABLE_REQUIRED) {
        eff_clk = PAL_PWM_EFF_CLK_REF_TICK;
        clk_cfg = LEDC_USE_REF_TICK;
    }

    pal_pwm_timer_profile_t prof = {
        .freq_hz = cfg->freq_hz,
        .resolution_bits = bits,
        .clock_source = eff_clk,
    };

    uint8_t timer_num = 0;
    wink_status_t rs = pal_pwm_router_acquire(channel, &prof, &timer_num);
    if (wink_status_is_error(rs)) { return rs; }

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = duty_res,
        .timer_num = (ledc_timer_t)timer_num,
        .freq_hz = cfg->freq_hz,
        .clk_cfg = clk_cfg,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        pal_pwm_router_release(channel);
        return WINK_ERR_HARDWARE;
    }

    ledc_channel_config_t ch_cfg = {
        .gpio_num = pal_pwm_pin_map[channel],
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = (ledc_timer_t)timer_num,
        .duty = 0,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ch_cfg);
    if (err != ESP_OK) {
        pal_pwm_router_release(channel);
        return WINK_ERR_HARDWARE;
    }

    s_ch_bits[channel] = bits;
    return WINK_OK;
}

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_percent) {
    if (!pal_pwm_router_channel_ready(channel)) { return WINK_ERR_INVALID_ARG; }
    if (duty_percent < 0.0f) { duty_percent = 0.0f; }
    if (duty_percent > 100.0f) { duty_percent = 100.0f; }

    uint32_t duty = pal_pwm_percent_to_raw(duty_percent, s_ch_bits[channel]);
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    return WINK_OK;
}

wink_status_t pal_pwm_set_freq(uint8_t channel, uint32_t freq_hz) {
    if (!pal_pwm_router_channel_ready(channel) || freq_hz == 0u) {
        return WINK_ERR_INVALID_ARG;
    }
    uint8_t timer_num = pal_pwm_router_channel_timer(channel);
    if (timer_num == 0xFF) { return WINK_ERR_INVALID_STATE; }
    esp_err_t err = ledc_set_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)timer_num, freq_hz);
    if (err != ESP_OK) { return WINK_ERR_HARDWARE; }
    return pal_pwm_router_set_freq(channel, freq_hz);
}

void pal_pwm_deinit(uint8_t channel) {
    if (!pal_pwm_router_channel_ready(channel)) { return; }
    (void)ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
    (void)ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    (void)ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
    pal_pwm_router_release(channel);

    wink_pin_t pin = pal_pwm_pin_map[channel];
    if (pin >= 0 && pin < GPIO_NUM_MAX) {
        (void)gpio_reset_pin((gpio_num_t)pin);
    }
}

wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin) {
    if (out_pin == NULL) { return WINK_ERR_INVALID_ARG; }
    if (channel >= PAL_PWM_CHANNELS) { return WINK_ERR_INVALID_ARG; }
    *out_pin = pal_pwm_pin_map[channel];
    return WINK_OK;
}

#else

wink_status_t pal_pwm_init(uint8_t channel, uint32_t freq_hz)
{ (void)channel; (void)freq_hz; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_pwm_init_ex(uint8_t channel, const pal_pwm_config_t *cfg)
{ (void)channel; (void)cfg; return WINK_ERR_UNSUPPORTED; }

wink_status_t pal_pwm_set_duty(uint8_t channel, float duty_percent)
{ (void)channel; (void)duty_percent; return WINK_ERR_UNSUPPORTED; }

void pal_pwm_deinit(uint8_t channel) { (void)channel; }

wink_status_t pal_pwm_channel_pin(uint8_t channel, wink_pin_t *out_pin)
{ (void)channel; (void)out_pin; return WINK_ERR_UNSUPPORTED; }

#endif
