/**
 * @file pal_hal_adc_host.c
 * @brief host 平台 PAL ADC 子系统实现及测试注入 API。
 */
#include "hal/pal_adc.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"
#include <string.h>

typedef struct {
    bool             is_initialized;
    pal_adc_config_t cfg;
    uint16_t         last_raw;
    uint16_t         last_mv;
    uint64_t         last_sample_us;
} host_adc_channel_state_t;

static host_adc_channel_state_t s_adc_channels[PAL_ADC_CHANNELS];

wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg) {
    if (ch >= PAL_ADC_CHANNELS || cfg == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (cfg->pin < 0) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_adc_channels[ch].is_initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    s_adc_channels[ch].is_initialized = true;
    s_adc_channels[ch].cfg.pin = cfg->pin;
    s_adc_channels[ch].cfg.full_scale_mv = (cfg->full_scale_mv > 0) ? cfg->full_scale_mv : 3300;
    s_adc_channels[ch].cfg.resolution_bits = (cfg->resolution_bits > 0) ? cfg->resolution_bits : 12;
    s_adc_channels[ch].last_raw = 0;
    s_adc_channels[ch].last_mv = 0;
    s_adc_channels[ch].last_sample_us = 0;

    return WINK_OK;
}

void pal_adc_deinit(pal_adc_channel_t ch) {
    if (ch >= PAL_ADC_CHANNELS) {
        return;
    }
    memset(&s_adc_channels[ch], 0, sizeof(host_adc_channel_state_t));
}

wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin) {
    if (ch >= PAL_ADC_CHANNELS || out_pin == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_adc_channels[ch].is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_pin = s_adc_channels[ch].cfg.pin;
    return WINK_OK;
}

wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch) {
    if (out_ch == NULL || pin < 0) {
        return WINK_ERR_INVALID_ARG;
    }
    for (uint8_t i = 0; i < PAL_ADC_CHANNELS; i++) {
        if (s_adc_channels[i].is_initialized && s_adc_channels[i].cfg.pin == pin) {
            *out_ch = i;
            return WINK_OK;
        }
    }
    return WINK_ERR_NOT_FOUND;
}

wink_status_t pal_adc_full_scale_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    if (ch >= PAL_ADC_CHANNELS || out_mv == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_adc_channels[ch].is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_mv = s_adc_channels[ch].cfg.full_scale_mv;
    return WINK_OK;
}

#ifndef WINK_STRICT_NONBLOCKING
wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw) {
    if (ch >= PAL_ADC_CHANNELS || out_raw == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_adc_channels[ch].is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_raw = s_adc_channels[ch].last_raw;
    return WINK_OK;
}

wink_status_t pal_adc_read_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    if (ch >= PAL_ADC_CHANNELS || out_mv == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_adc_channels[ch].is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_mv = s_adc_channels[ch].last_mv;
    return WINK_OK;
}
#endif /* WINK_STRICT_NONBLOCKING */

/* --- Host 测试注入 API --- */

void pal_host_adc_inject_raw(pal_adc_channel_t ch, uint16_t raw) {
    if (ch >= PAL_ADC_CHANNELS) {
        return;
    }
    uint16_t full_scale = s_adc_channels[ch].cfg.full_scale_mv ? s_adc_channels[ch].cfg.full_scale_mv : 3300;
    uint8_t bits = s_adc_channels[ch].cfg.resolution_bits ? s_adc_channels[ch].cfg.resolution_bits : 12;
    uint32_t max_raw = (1U << bits) - 1U;

    if (raw > max_raw) {
        raw = (uint16_t)max_raw;
    }

    s_adc_channels[ch].last_raw = raw;
    s_adc_channels[ch].last_mv = (uint16_t)(((uint32_t)raw * full_scale + (max_raw / 2U)) / max_raw);
}

void pal_host_adc_inject_mv(pal_adc_channel_t ch, uint16_t mv) {
    if (ch >= PAL_ADC_CHANNELS) {
        return;
    }
    uint16_t full_scale = s_adc_channels[ch].cfg.full_scale_mv ? s_adc_channels[ch].cfg.full_scale_mv : 3300;
    uint8_t bits = s_adc_channels[ch].cfg.resolution_bits ? s_adc_channels[ch].cfg.resolution_bits : 12;
    uint32_t max_raw = (1U << bits) - 1U;

    if (mv > full_scale) {
        mv = full_scale;
    }

    s_adc_channels[ch].last_mv = mv;
    s_adc_channels[ch].last_raw = (uint16_t)(((uint32_t)mv * max_raw + (full_scale / 2U)) / full_scale);
}
