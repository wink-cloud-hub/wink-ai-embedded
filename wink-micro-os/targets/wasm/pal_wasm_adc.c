/**
 * @file pal_wasm_adc.c
 * @brief Wasm 仿真 target 的 PAL ADC 子系统实现及退化引擎接线。
 */
#include "hal/pal_adc.h"
#include "wasm_bridge.h"
#include "pal_wasm_internal.h"
#include "wink_sim_physical.h"
#include "pal_osal.h"
#include <string.h>
#include <math.h>

typedef struct {
    bool                       is_initialized;
    wink_pin_t                  pin;
    uint16_t                   full_scale_mv;
    uint8_t                    resolution_bits;
    wink_phys_rc_lowpass_ctx_t rc_ctx;
    uint32_t                   seed;
    uint64_t                   power_on_us;
    uint64_t                   last_sample_us;
    uint16_t                   last_raw;
    uint16_t                   last_mv;
} wasm_adc_channel_state_t;

static wasm_adc_channel_state_t s_channels[PAL_ADC_CHANNELS];

static uint32_t hash_pin(wink_pin_t pin) {
    uint32_t h = (uint32_t)pin * 2654435761U;
    return (h == 0) ? 1U : h;
}

wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg) {
    if (ch >= PAL_ADC_CHANNELS || cfg == NULL || cfg->pin < 0) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_channels[ch].is_initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    s_channels[ch].is_initialized = true;
    s_channels[ch].pin = cfg->pin;
    s_channels[ch].full_scale_mv = (cfg->full_scale_mv > 0) ? cfg->full_scale_mv : 3300;
    s_channels[ch].resolution_bits = (cfg->resolution_bits > 0) ? cfg->resolution_bits : 12;
    s_channels[ch].seed = hash_pin(cfg->pin);
    s_channels[ch].power_on_us = pal_os_get_us();
    s_channels[ch].last_sample_us = 0;
    s_channels[ch].last_raw = 0;
    s_channels[ch].last_mv = 0;
    memset(&s_channels[ch].rc_ctx, 0, sizeof(wink_phys_rc_lowpass_ctx_t));

    return WINK_OK;
}

void pal_adc_deinit(pal_adc_channel_t ch) {
    if (ch >= PAL_ADC_CHANNELS) {
        return;
    }
    memset(&s_channels[ch], 0, sizeof(wasm_adc_channel_state_t));
}

wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin) {
    if (ch >= PAL_ADC_CHANNELS || out_pin == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_channels[ch].is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_pin = s_channels[ch].pin;
    return WINK_OK;
}

wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch) {
    if (out_ch == NULL || pin < 0) {
        return WINK_ERR_INVALID_ARG;
    }
    for (uint8_t i = 0; i < PAL_ADC_CHANNELS; i++) {
        if (s_channels[i].is_initialized && s_channels[i].pin == pin) {
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
    if (!s_channels[ch].is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_mv = s_channels[ch].full_scale_mv;
    return WINK_OK;
}

#ifndef WINK_STRICT_NONBLOCKING
wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw) {
    if (ch >= PAL_ADC_CHANNELS || out_raw == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_channels[ch].is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    uint64_t now_us = pal_os_get_us();
    wink_sim_faults_t *faults = pal_wasm_get_faults_ref();

    /* 检查预热与采样间隔限制 */
    wink_status_t warm_st = wink_phys_warmup_check(now_us, s_channels[ch].power_on_us,
                                                    faults ? faults->warmup_us : 0,
                                                    faults ? faults->sample_interval_us : 0,
                                                    &s_channels[ch].last_sample_us);
    if (warm_st != WINK_OK) {
        return warm_st;
    }

    /* 从 JS 物理总线拉取归一化电压 [0.0, 1.0] */
    float norm_ideal = js_pal_adc_read_norm((uint16_t)s_channels[ch].pin);
    if (isnan(norm_ideal) || isinf(norm_ideal) || norm_ideal < 0.0f) {
        norm_ideal = 0.0f;
    } else if (norm_ideal > 1.0f) {
        norm_ideal = 1.0f;
    }

    float rc_tau_s = faults ? faults->rc_tau_s : 0.0f;
    float adc_noise_v = faults ? faults->adc_noise_v : 0.0f;

    /* 经过 C 侧物理退化引擎 (RC 低通 + Per-channel 独立 PRNG 噪声) */
    float norm_filt = wink_phys_rc_lowpass(&s_channels[ch].rc_ctx, norm_ideal, now_us,
                                            rc_tau_s, adc_noise_v,
                                            &s_channels[ch].seed);
    if (norm_filt < 0.0f) norm_filt = 0.0f;
    if (norm_filt > 1.0f) norm_filt = 1.0f;

    uint32_t max_raw = (1U << s_channels[ch].resolution_bits) - 1U;
    s_channels[ch].last_raw = (uint16_t)(norm_filt * (float)max_raw + 0.5f);
    s_channels[ch].last_mv = (uint16_t)(norm_filt * (float)s_channels[ch].full_scale_mv + 0.5f);

    *out_raw = s_channels[ch].last_raw;
    return WINK_OK;
}

wink_status_t pal_adc_read_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    if (ch >= PAL_ADC_CHANNELS || out_mv == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!s_channels[ch].is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    /* 优先复用上一帧 Raw 转换结果（避免同一微秒内二次触发采样间隔拦截） */
    if (s_channels[ch].last_sample_us == 0) {
        uint16_t dummy_raw = 0;
        wink_status_t status = pal_adc_read_raw(ch, &dummy_raw);
        if (status != WINK_OK) {
            return status;
        }
    }

    *out_mv = s_channels[ch].last_mv;
    return WINK_OK;
}
#endif /* WINK_STRICT_NONBLOCKING */
