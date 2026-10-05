// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file pal_wasm_ch3_dac.c
 * @brief Wasm target Axis A (CH3) Analog DAC subsystem implementation (ADR-0092 Tier 1).
 */
#include "hal/pal_dac.h"
#include "wasm_bridge.h"
#include "pal_wasm_common.h"
#include "pal_osal.h"
#include <string.h>

typedef struct {
    bool                is_initialized;
    wink_pin_t          pin;
    uint16_t            full_scale_mv;
    uint8_t             resolution_bits;
    uint16_t            last_raw;
    uint16_t            last_mv;
    bool                cw_running;
    pal_dac_cw_config_t cw_cfg;
} wasm_dac_channel_state_t;

static wasm_dac_channel_state_t s_dac_channels[PAL_DAC_CHANNELS];

wink_status_t pal_dac_init(pal_dac_channel_t ch, const pal_dac_config_t *cfg) {
    if (ch >= PAL_DAC_CHANNELS || cfg == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_dac_channels[ch].is_initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    s_dac_channels[ch].is_initialized = true;
    s_dac_channels[ch].pin = (cfg->pin >= 0) ? cfg->pin : ((ch == 0) ? 25 : 26);
    s_dac_channels[ch].full_scale_mv = (cfg->full_scale_mv > 0) ? cfg->full_scale_mv : 3300;
    s_dac_channels[ch].resolution_bits = (cfg->resolution_bits > 0) ? cfg->resolution_bits : 8;
    s_dac_channels[ch].last_raw = 0;
    s_dac_channels[ch].last_mv = 0;
    s_dac_channels[ch].cw_running = false;
    memset(&s_dac_channels[ch].cw_cfg, 0, sizeof(pal_dac_cw_config_t));

    return WINK_OK;
}

void pal_dac_deinit(pal_dac_channel_t ch) {
    if (ch >= PAL_DAC_CHANNELS) {
        return;
    }
    if (s_dac_channels[ch].is_initialized) {
        js_pal_dac_write_norm((uint16_t)s_dac_channels[ch].pin, 0.0f);
    }
    memset(&s_dac_channels[ch], 0, sizeof(wasm_dac_channel_state_t));
}

wink_status_t pal_dac_write_raw(pal_dac_channel_t ch, uint16_t raw_val) {
    if (ch >= PAL_DAC_CHANNELS) return WINK_ERR_INVALID_ARG;
    if (!s_dac_channels[ch].is_initialized) return WINK_ERR_NOT_INITIALIZED;

    uint32_t max_raw = (1U << s_dac_channels[ch].resolution_bits) - 1U;
    if (raw_val > max_raw) {
        raw_val = (uint16_t)max_raw;
    }

    s_dac_channels[ch].last_raw = raw_val;
    s_dac_channels[ch].last_mv = (uint16_t)(((uint32_t)raw_val * s_dac_channels[ch].full_scale_mv + (max_raw / 2)) / max_raw);

    float norm = (max_raw > 0) ? ((float)raw_val / (float)max_raw) : 0.0f;
    js_pal_dac_write_norm((uint16_t)s_dac_channels[ch].pin, norm);

    return WINK_OK;
}

wink_status_t pal_dac_write_voltage(pal_dac_channel_t ch, uint16_t millivolts) {
    if (ch >= PAL_DAC_CHANNELS) return WINK_ERR_INVALID_ARG;
    if (!s_dac_channels[ch].is_initialized) return WINK_ERR_NOT_INITIALIZED;

    uint32_t max_raw = (1U << s_dac_channels[ch].resolution_bits) - 1U;
    if (millivolts > s_dac_channels[ch].full_scale_mv) {
        millivolts = s_dac_channels[ch].full_scale_mv;
    }

    uint16_t raw = (uint16_t)(((uint32_t)millivolts * max_raw + (s_dac_channels[ch].full_scale_mv / 2)) / s_dac_channels[ch].full_scale_mv);
    return pal_dac_write_raw(ch, raw);
}

wink_status_t pal_dac_get_voltage(pal_dac_channel_t ch, uint16_t *out_millivolts) {
    if (ch >= PAL_DAC_CHANNELS || out_millivolts == NULL) return WINK_ERR_INVALID_ARG;
    if (!s_dac_channels[ch].is_initialized) return WINK_ERR_NOT_INITIALIZED;

    *out_millivolts = s_dac_channels[ch].last_mv;
    return WINK_OK;
}

wink_status_t pal_dac_channel_pin(pal_dac_channel_t ch, wink_pin_t *out_pin) {
    if (ch >= PAL_DAC_CHANNELS || out_pin == NULL) return WINK_ERR_INVALID_ARG;
    if (!s_dac_channels[ch].is_initialized) return WINK_ERR_NOT_INITIALIZED;

    *out_pin = s_dac_channels[ch].pin;
    return WINK_OK;
}

wink_status_t pal_dac_pin_channel(wink_pin_t pin, pal_dac_channel_t *out_ch) {
    if (out_ch == NULL) return WINK_ERR_INVALID_ARG;
    for (pal_dac_channel_t i = 0; i < PAL_DAC_CHANNELS; i++) {
        if (s_dac_channels[i].is_initialized && s_dac_channels[i].pin == pin) {
            *out_ch = i;
            return WINK_OK;
        }
    }
    return WINK_ERR_NOT_FOUND;
}

wink_status_t pal_dac_acquire(wink_pin_t pin, const pal_dac_config_t *cfg, pal_dac_channel_t *out_ch) {
    if (out_ch == NULL) return WINK_ERR_INVALID_ARG;

    for (pal_dac_channel_t i = 0; i < PAL_DAC_CHANNELS; i++) {
        if (s_dac_channels[i].is_initialized && s_dac_channels[i].pin == pin) {
            *out_ch = i;
            return WINK_OK;
        }
    }

    for (pal_dac_channel_t i = 0; i < PAL_DAC_CHANNELS; i++) {
        if (!s_dac_channels[i].is_initialized) {
            pal_dac_config_t local_cfg;
            if (cfg) {
                local_cfg = *cfg;
            } else {
                local_cfg.full_scale_mv = 3300;
                local_cfg.resolution_bits = 8;
            }
            local_cfg.pin = pin;
            wink_status_t st = pal_dac_init(i, &local_cfg);
            if (st == WINK_OK) {
                *out_ch = i;
            }
            return st;
        }
    }
    return WINK_ERR_NO_MEM;
}

wink_status_t pal_dac_release(pal_dac_channel_t ch) {
    if (ch >= PAL_DAC_CHANNELS) return WINK_ERR_INVALID_ARG;
    if (!s_dac_channels[ch].is_initialized) return WINK_ERR_NOT_INITIALIZED;

    pal_dac_deinit(ch);
    return WINK_OK;
}

wink_status_t pal_dac_start_cw(pal_dac_channel_t ch, const pal_dac_cw_config_t *cw_cfg) {
    if (ch >= PAL_DAC_CHANNELS || cw_cfg == NULL) return WINK_ERR_INVALID_ARG;
    if (!s_dac_channels[ch].is_initialized) return WINK_ERR_NOT_INITIALIZED;

    s_dac_channels[ch].cw_running = true;
    s_dac_channels[ch].cw_cfg = *cw_cfg;

    uint16_t mid_raw = (1U << (s_dac_channels[ch].resolution_bits - 1));
    pal_dac_write_raw(ch, mid_raw);
    return WINK_OK;
}

wink_status_t pal_dac_stop_cw(pal_dac_channel_t ch) {
    if (ch >= PAL_DAC_CHANNELS) return WINK_ERR_INVALID_ARG;
    if (!s_dac_channels[ch].is_initialized) return WINK_ERR_NOT_INITIALIZED;

    s_dac_channels[ch].cw_running = false;
    pal_dac_write_raw(ch, 0);
    return WINK_OK;
}
