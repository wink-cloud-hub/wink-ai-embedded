// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_adc_host.c
 * @brief Host platform PAL ADC subsystem implementation and test injection API.
 */
#include "hal/pal_adc.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <pthread.h>
#endif

#include <string.h>

typedef struct {
    bool             is_initialized;
    bool             has_sample;
    pal_adc_config_t cfg;
    uint16_t         last_raw;
    uint16_t         last_mv;
} host_adc_channel_state_t;

static host_adc_channel_state_t s_adc_channels[PAL_ADC_CHANNELS];

#if defined(_WIN32)
static CRITICAL_SECTION s_ch_mux[PAL_ADC_CHANNELS];
static volatile LONG     s_ch_mux_init[PAL_ADC_CHANNELS];
static void host_adc_lock(uint8_t ch) {
    if (InterlockedCompareExchange(&s_ch_mux_init[ch], 1, 0) == 0) {
        InitializeCriticalSection(&s_ch_mux[ch]);
    }
    EnterCriticalSection(&s_ch_mux[ch]);
}
static void host_adc_unlock(uint8_t ch) { LeaveCriticalSection(&s_ch_mux[ch]); }
#else
static pthread_mutex_t s_ch_mux[PAL_ADC_CHANNELS] = {
    [0 ... PAL_ADC_CHANNELS - 1] = PTHREAD_MUTEX_INITIALIZER
};
static void host_adc_lock(uint8_t ch)   { pthread_mutex_lock(&s_ch_mux[ch]); }
static void host_adc_unlock(uint8_t ch) { pthread_mutex_unlock(&s_ch_mux[ch]); }
#endif

static uint16_t host_full_scale(const host_adc_channel_state_t *st) {
    return st->cfg.full_scale_mv ? st->cfg.full_scale_mv : 3300;
}
static uint8_t host_bits(const host_adc_channel_state_t *st) {
    return st->cfg.resolution_bits ? st->cfg.resolution_bits : 12;
}
static uint32_t host_max_raw(const host_adc_channel_state_t *st) {
    return (1U << host_bits(st)) - 1U;
}

static void host_store_raw_locked(host_adc_channel_state_t *st, uint16_t raw) {
    uint32_t max_raw = host_max_raw(st);
    uint16_t fs = host_full_scale(st);
    if (raw > max_raw) raw = (uint16_t)max_raw;
    st->last_raw = raw;
    st->last_mv = (uint16_t)(((uint32_t)raw * fs + (max_raw / 2U)) / max_raw);
    st->has_sample = true;
}
static void host_store_mv_locked(host_adc_channel_state_t *st, uint16_t mv) {
    uint32_t max_raw = host_max_raw(st);
    uint16_t fs = host_full_scale(st);
    if (mv > fs) mv = fs;
    st->last_mv = mv;
    st->last_raw = (uint16_t)(((uint32_t)mv * max_raw + (fs / 2U)) / fs);
    st->has_sample = true;
}

wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg) {
    if (ch >= PAL_ADC_CHANNELS || cfg == NULL || cfg->pin < 0) {
        return WINK_ERR_INVALID_ARG;
    }
    host_adc_lock(ch);
    if (s_adc_channels[ch].is_initialized) {
        host_adc_unlock(ch);
        return WINK_ERR_ALREADY_INITIALIZED;
    }
    s_adc_channels[ch].is_initialized = true;
    s_adc_channels[ch].has_sample = false;
    s_adc_channels[ch].cfg = *cfg;
    s_adc_channels[ch].last_raw = 0;
    s_adc_channels[ch].last_mv = 0;
    host_adc_unlock(ch);
    return WINK_OK;
}

void pal_adc_deinit(pal_adc_channel_t ch) {
    if (ch >= PAL_ADC_CHANNELS) {
        return;
    }
    host_adc_lock(ch);
    memset(&s_adc_channels[ch], 0, sizeof(host_adc_channel_state_t));
    host_adc_unlock(ch);
}

wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin) {
    if (ch >= PAL_ADC_CHANNELS || out_pin == NULL) return WINK_ERR_INVALID_ARG;
    host_adc_lock(ch);
    if (!s_adc_channels[ch].is_initialized) {
        host_adc_unlock(ch);
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_pin = s_adc_channels[ch].cfg.pin;
    host_adc_unlock(ch);
    return WINK_OK;
}

wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch) {
    if (out_ch == NULL || pin < 0) return WINK_ERR_INVALID_ARG;
    for (uint8_t i = 0; i < PAL_ADC_CHANNELS; i++) {
        host_adc_lock(i);
    }
    for (uint8_t i = 0; i < PAL_ADC_CHANNELS; i++) {
        if (s_adc_channels[i].is_initialized && s_adc_channels[i].cfg.pin == pin) {
            *out_ch = i;
            for (int16_t j = (int16_t)PAL_ADC_CHANNELS - 1; j >= 0; j--) {
                host_adc_unlock((uint8_t)j);
            }
            return WINK_OK;
        }
    }
    for (int16_t j = (int16_t)PAL_ADC_CHANNELS - 1; j >= 0; j--) {
        host_adc_unlock((uint8_t)j);
    }
    return WINK_ERR_NOT_FOUND;
}

wink_status_t pal_adc_full_scale_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    if (ch >= PAL_ADC_CHANNELS || out_mv == NULL) return WINK_ERR_INVALID_ARG;
    host_adc_lock(ch);
    if (!s_adc_channels[ch].is_initialized) {
        host_adc_unlock(ch);
        return WINK_ERR_NOT_INITIALIZED;
    }
    *out_mv = host_full_scale(&s_adc_channels[ch]);
    host_adc_unlock(ch);
    return WINK_OK;
}

#ifndef WINK_STRICT_NONBLOCKING
wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw) {
    if (ch >= PAL_ADC_CHANNELS || out_raw == NULL) return WINK_ERR_INVALID_ARG;
    host_adc_lock(ch);
    if (!s_adc_channels[ch].is_initialized) {
        host_adc_unlock(ch);
        return WINK_ERR_NOT_INITIALIZED;
    }
    s_adc_channels[ch].has_sample = true;
    *out_raw = s_adc_channels[ch].last_raw;
    host_adc_unlock(ch);
    return WINK_OK;
}

wink_status_t pal_adc_read_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    if (ch >= PAL_ADC_CHANNELS || out_mv == NULL) return WINK_ERR_INVALID_ARG;
    host_adc_lock(ch);
    if (!s_adc_channels[ch].is_initialized) {
        host_adc_unlock(ch);
        return WINK_ERR_NOT_INITIALIZED;
    }
    if (!s_adc_channels[ch].has_sample) {
        s_adc_channels[ch].has_sample = true;
    }
    *out_mv = s_adc_channels[ch].last_mv;
    host_adc_unlock(ch);
    return WINK_OK;
}
#endif

void pal_host_adc_inject_raw(pal_adc_channel_t ch, uint16_t raw) {
    if (ch >= PAL_ADC_CHANNELS) return;
    host_adc_lock(ch);
    if (s_adc_channels[ch].is_initialized) {
        host_store_raw_locked(&s_adc_channels[ch], raw);
    }
    host_adc_unlock(ch);
}

void pal_host_adc_inject_mv(pal_adc_channel_t ch, uint16_t mv) {
    if (ch >= PAL_ADC_CHANNELS) return;
    host_adc_lock(ch);
    if (s_adc_channels[ch].is_initialized) {
        host_store_mv_locked(&s_adc_channels[ch], mv);
    }
    host_adc_unlock(ch);
}
