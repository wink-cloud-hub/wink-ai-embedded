// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_adc_esp32.c
 * @brief ESP32 target PAL ADC subsystem implementation (ESP-IDF v6.0.1 adc_oneshot + adc_cali).
 */
#include "hal/pal_adc.h"
#include "pal_resource.h"
#include <string.h>

#if defined(ESP_PLATFORM)

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h"
#include "soc/soc_caps.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define ESP32_ADC_NUM_UNITS SOC_ADC_PERIPH_NUM
#define ADC_LOCK_TIMEOUT_MS 200

typedef struct {
    bool              is_initialized;
    bool              has_sample;
    wink_pin_t        pin;
    adc_unit_t        unit_id;
    adc_channel_t     adc_chan;
    uint16_t          full_scale_mv;
    uint8_t           resolution_bits;
    adc_atten_t       atten;
    adc_cali_handle_t cali_handle;
    bool              has_cali;
    uint16_t          last_raw;
    uint16_t          last_mv;
} esp32_adc_channel_state_t;

static esp32_adc_channel_state_t s_channels[PAL_ADC_CHANNELS];

static SemaphoreHandle_t s_ch_locks[PAL_ADC_CHANNELS];
static StaticSemaphore_t s_ch_lock_bufs[PAL_ADC_CHANNELS];
static SemaphoreHandle_t s_unit_mux;
static StaticSemaphore_t s_unit_mux_buf;
static portMUX_TYPE      s_init_mux = portMUX_INITIALIZER_UNLOCKED;

static adc_oneshot_unit_handle_t s_unit_handles[ESP32_ADC_NUM_UNITS];
static uint8_t                   s_unit_refcount[ESP32_ADC_NUM_UNITS];

static void pal_adc_ensure_locks(void) {
    portENTER_CRITICAL(&s_init_mux);
    if (s_unit_mux == NULL) {
        s_unit_mux = xSemaphoreCreateMutexStatic(&s_unit_mux_buf);
    }
    for (int i = 0; i < PAL_ADC_CHANNELS; i++) {
        if (s_ch_locks[i] == NULL) {
            s_ch_locks[i] = xSemaphoreCreateMutexStatic(&s_ch_lock_bufs[i]);
        }
    }
    portEXIT_CRITICAL(&s_init_mux);
}

#ifndef SOC_ADC_RTC_MIN_BITWIDTH
#define SOC_ADC_RTC_MIN_BITWIDTH 9
#endif

static inline bool ch_lock_take(uint8_t ch) {
    if (s_ch_locks[ch] == NULL) pal_adc_ensure_locks();
    return xSemaphoreTake(s_ch_locks[ch], pdMS_TO_TICKS(ADC_LOCK_TIMEOUT_MS)) == pdTRUE;
}
static inline void ch_lock_give(uint8_t ch) {
    xSemaphoreGive(s_ch_locks[ch]);
}
static inline bool unit_lock_take(void) {
    if (s_unit_mux == NULL) pal_adc_ensure_locks();
    return xSemaphoreTake(s_unit_mux, pdMS_TO_TICKS(ADC_LOCK_TIMEOUT_MS)) == pdTRUE;
}
static inline void unit_lock_give(void) {
    xSemaphoreGive(s_unit_mux);
}

static wink_status_t esp_err_to_wink_status(esp_err_t err) {
    switch (err) {
        case ESP_OK:                  return WINK_OK;
        case ESP_ERR_INVALID_ARG:     return WINK_ERR_INVALID_ARG;
        case ESP_ERR_INVALID_STATE:   return WINK_ERR_UNSUPPORTED;
        case ESP_ERR_NOT_FOUND:       return WINK_ERR_NOT_FOUND;
        case ESP_ERR_TIMEOUT:         return WINK_ERR_TIMEOUT;
        case ESP_ERR_NO_MEM:          return WINK_ERR_NO_MEMORY;
        default:                      return WINK_ERR_HARDWARE;
    }
}

static wink_status_t select_attenuation(uint16_t full_scale_mv, adc_atten_t *out_atten) {
    if (full_scale_mv == 0 || full_scale_mv >= 2400) {
        *out_atten = ADC_ATTEN_DB_12;
        return WINK_OK;
    } else if (full_scale_mv <= 1100) {
        *out_atten = ADC_ATTEN_DB_0;
        return WINK_OK;
    } else if (full_scale_mv <= 1500) {
        *out_atten = ADC_ATTEN_DB_2_5;
        return WINK_OK;
    } else if (full_scale_mv <= 2200) {
        *out_atten = ADC_ATTEN_DB_6;
        return WINK_OK;
    }
    *out_atten = ADC_ATTEN_DB_12;
    return WINK_OK;
}

static wink_status_t select_bitwidth(uint8_t bits, adc_bitwidth_t *out_bw) {
    if (bits == 0) bits = 12;
    if (bits < SOC_ADC_RTC_MIN_BITWIDTH || bits > SOC_ADC_RTC_MAX_BITWIDTH) {
        return WINK_ERR_INVALID_ARG;
    }
    switch (bits) {
#ifdef ADC_BITWIDTH_9
        case 9:  *out_bw = ADC_BITWIDTH_9;  return WINK_OK;
#endif
#ifdef ADC_BITWIDTH_10
        case 10: *out_bw = ADC_BITWIDTH_10; return WINK_OK;
#endif
#ifdef ADC_BITWIDTH_11
        case 11: *out_bw = ADC_BITWIDTH_11; return WINK_OK;
#endif
#ifdef ADC_BITWIDTH_12
        case 12: *out_bw = ADC_BITWIDTH_12; return WINK_OK;
#endif
#ifdef ADC_BITWIDTH_13
        case 13: *out_bw = ADC_BITWIDTH_13; return WINK_OK;
#endif
        default: return WINK_ERR_INVALID_ARG;
    }
}

static void destroy_cali_locked(esp32_adc_channel_state_t *st) {
    if (!st->has_cali || st->cali_handle == NULL) return;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_delete_scheme_curve_fitting(st->cali_handle);
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_delete_scheme_line_fitting(st->cali_handle);
#endif
    st->has_cali = false;
    st->cali_handle = NULL;
}

static wink_status_t create_cali_locked(esp32_adc_channel_state_t *st, adc_bitwidth_t bitwidth) {
    int created = 0;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    {
        adc_cali_curve_fitting_config_t cfg = {
            .unit_id = st->unit_id,
            .chan    = st->adc_chan,
            .atten   = st->atten,
            .bitwidth = bitwidth,
        };
        if (adc_cali_create_scheme_curve_fitting(&cfg, &st->cali_handle) == ESP_OK) {
            st->has_cali = true;
            created = 1;
        }
    }
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!created) {
        adc_cali_line_fitting_config_t cfg = {
            .unit_id  = st->unit_id,
            .atten    = st->atten,
            .bitwidth = bitwidth,
        };
        if (adc_cali_create_scheme_line_fitting(&cfg, &st->cali_handle) == ESP_OK) {
            st->has_cali = true;
            created = 1;
        }
    }
#endif
    (void)created;
    return WINK_OK;
}

wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg) {
    if (ch >= PAL_ADC_CHANNELS || cfg == NULL || cfg->pin < 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_adc_ensure_locks();
    if (s_ch_locks[ch] == NULL) return WINK_ERR_NO_MEMORY;

    if (!ch_lock_take(ch)) return WINK_ERR_TIMEOUT;
    if (s_channels[ch].is_initialized) {
        ch_lock_give(ch);
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    adc_unit_t unit_id;
    adc_channel_t adc_chan;
    esp_err_t err = adc_oneshot_io_to_channel((int)cfg->pin, &unit_id, &adc_chan);
    if (err != ESP_OK || (unsigned)unit_id >= ESP32_ADC_NUM_UNITS) {
        ch_lock_give(ch);
        return WINK_ERR_INVALID_ARG;
    }

    uint8_t res_bits = (cfg->resolution_bits > 0) ? cfg->resolution_bits : 12;
    adc_bitwidth_t bitwidth;
    wink_status_t st = select_bitwidth(res_bits, &bitwidth);
    if (st != WINK_OK) {
        ch_lock_give(ch);
        return st;
    }

    adc_atten_t atten;
    st = select_attenuation(cfg->full_scale_mv, &atten);
    if (st != WINK_OK) {
        ch_lock_give(ch);
        return st;
    }

    if (!unit_lock_take()) {
        ch_lock_give(ch);
        return WINK_ERR_TIMEOUT;
    }
    if (s_unit_refcount[unit_id] == 0) {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id  = unit_id,
            .clk_src  = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLED,
        };
        err = adc_oneshot_new_unit(&init_config, &s_unit_handles[unit_id]);
        if (err != ESP_OK) {
            unit_lock_give();
            ch_lock_give(ch);
            return esp_err_to_wink_status(err);
        }
    }
    s_unit_refcount[unit_id]++;
    unit_lock_give();

    err = adc_oneshot_config_channel(s_unit_handles[unit_id], adc_chan,
        &(adc_oneshot_chan_cfg_t){ .atten = atten, .bitwidth = bitwidth });
    if (err != ESP_OK) {
        unit_lock_take();
        if (s_unit_refcount[unit_id] > 0) {
            s_unit_refcount[unit_id]--;
            if (s_unit_refcount[unit_id] == 0) {
                adc_oneshot_del_unit(s_unit_handles[unit_id]);
                s_unit_handles[unit_id] = NULL;
            }
        }
        unit_lock_give();
        ch_lock_give(ch);
        return esp_err_to_wink_status(err);
    }

    s_channels[ch].is_initialized  = true;
    s_channels[ch].has_sample      = false;
    s_channels[ch].pin             = cfg->pin;
    s_channels[ch].unit_id         = unit_id;
    s_channels[ch].adc_chan        = adc_chan;
    s_channels[ch].full_scale_mv   = (cfg->full_scale_mv > 0) ? cfg->full_scale_mv : 3100;
    s_channels[ch].resolution_bits = res_bits;
    s_channels[ch].atten           = atten;
    s_channels[ch].cali_handle     = NULL;
    s_channels[ch].has_cali        = false;
    s_channels[ch].last_raw        = 0;
    s_channels[ch].last_mv         = 0;

    create_cali_locked(&s_channels[ch], bitwidth);
    ch_lock_give(ch);
    return WINK_OK;
}

void pal_adc_deinit(pal_adc_channel_t ch) {
    if (ch >= PAL_ADC_CHANNELS) return;
    if (s_ch_locks[ch] == NULL) return;
    if (!ch_lock_take(ch)) return;
    if (!s_channels[ch].is_initialized) {
        ch_lock_give(ch);
        return;
    }

    destroy_cali_locked(&s_channels[ch]);

    adc_unit_t unit_id = s_channels[ch].unit_id;
    unit_lock_take();
    if ((unsigned)unit_id < ESP32_ADC_NUM_UNITS && s_unit_refcount[unit_id] > 0) {
        s_unit_refcount[unit_id]--;
        if (s_unit_refcount[unit_id] == 0 && s_unit_handles[unit_id] != NULL) {
            adc_oneshot_del_unit(s_unit_handles[unit_id]);
            s_unit_handles[unit_id] = NULL;
        }
    }
    unit_lock_give();

    memset(&s_channels[ch], 0, sizeof(esp32_adc_channel_state_t));
    ch_lock_give(ch);
}

wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin) {
    if (ch >= PAL_ADC_CHANNELS || out_pin == NULL) return WINK_ERR_INVALID_ARG;
    if (s_ch_locks[ch] == NULL) return WINK_ERR_NOT_INITIALIZED;
    if (!ch_lock_take(ch)) return WINK_ERR_TIMEOUT;
    if (!s_channels[ch].is_initialized) { ch_lock_give(ch); return WINK_ERR_NOT_INITIALIZED; }
    *out_pin = s_channels[ch].pin;
    ch_lock_give(ch);
    return WINK_OK;
}

wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch) {
    if (out_ch == NULL || pin < 0) return WINK_ERR_INVALID_ARG;
    pal_adc_ensure_locks();
    for (uint8_t i = 0; i < PAL_ADC_CHANNELS; i++) {
        if (!ch_lock_take(i)) {
            for (int16_t k = (int16_t)i - 1; k >= 0; k--) ch_lock_give((uint8_t)k);
            return WINK_ERR_TIMEOUT;
        }
    }
    wink_status_t st = WINK_ERR_NOT_FOUND;
    for (uint8_t i = 0; i < PAL_ADC_CHANNELS; i++) {
        if (s_channels[i].is_initialized && s_channels[i].pin == pin) {
            *out_ch = i;
            st = WINK_OK;
            break;
        }
    }
    for (int16_t j = (int16_t)PAL_ADC_CHANNELS - 1; j >= 0; j--) {
        ch_lock_give((uint8_t)j);
    }
    return st;
}

wink_status_t pal_adc_full_scale_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    if (ch >= PAL_ADC_CHANNELS || out_mv == NULL) return WINK_ERR_INVALID_ARG;
    if (s_ch_locks[ch] == NULL) return WINK_ERR_NOT_INITIALIZED;
    if (!ch_lock_take(ch)) return WINK_ERR_TIMEOUT;
    if (!s_channels[ch].is_initialized) { ch_lock_give(ch); return WINK_ERR_NOT_INITIALIZED; }
    *out_mv = s_channels[ch].full_scale_mv;
    ch_lock_give(ch);
    return WINK_OK;
}

#ifndef WINK_STRICT_NONBLOCKING
static wink_status_t sample_once_locked(uint8_t ch) {
    adc_oneshot_unit_handle_t unit = s_unit_handles[s_channels[ch].unit_id];
    adc_channel_t       hw_chan = s_channels[ch].adc_chan;
    adc_cali_handle_t  cali     = s_channels[ch].has_cali ? s_channels[ch].cali_handle : NULL;
    uint16_t           fs_mv    = s_channels[ch].full_scale_mv;
    uint8_t            bits     = s_channels[ch].resolution_bits;

    if (unit == NULL) return WINK_ERR_NOT_INITIALIZED;

    int raw_val = 0;
    esp_err_t err = adc_oneshot_read(unit, hw_chan, &raw_val);
    if (err != ESP_OK) return esp_err_to_wink_status(err);
    if (raw_val < 0) raw_val = 0;
    uint32_t max_raw = (1U << bits) - 1U;
    if ((uint32_t)raw_val > max_raw) raw_val = (int)max_raw;

    uint16_t mv16;
    if (cali != NULL) {
        int voltage_mv = 0;
        if (adc_cali_raw_to_voltage(cali, raw_val, &voltage_mv) == ESP_OK) {
            if (voltage_mv < 0) voltage_mv = 0;
            if (voltage_mv > fs_mv) voltage_mv = fs_mv;
            mv16 = (uint16_t)voltage_mv;
        } else {
            mv16 = (uint16_t)(((uint32_t)raw_val * fs_mv + (max_raw / 2U)) / max_raw);
        }
    } else {
        mv16 = (uint16_t)(((uint32_t)raw_val * fs_mv + (max_raw / 2U)) / max_raw);
    }

    s_channels[ch].last_raw   = (uint16_t)raw_val;
    s_channels[ch].last_mv    = mv16;
    s_channels[ch].has_sample = true;
    return WINK_OK;
}

wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw) {
    if (ch >= PAL_ADC_CHANNELS || out_raw == NULL) return WINK_ERR_INVALID_ARG;
    if (s_ch_locks[ch] == NULL) return WINK_ERR_NOT_INITIALIZED;
    if (!ch_lock_take(ch)) return WINK_ERR_TIMEOUT;
    if (!s_channels[ch].is_initialized) { ch_lock_give(ch); return WINK_ERR_NOT_INITIALIZED; }

    wink_status_t st = sample_once_locked(ch);
    if (st == WINK_OK) *out_raw = s_channels[ch].last_raw;
    ch_lock_give(ch);
    return st;
}

wink_status_t pal_adc_read_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    if (ch >= PAL_ADC_CHANNELS || out_mv == NULL) return WINK_ERR_INVALID_ARG;
    if (s_ch_locks[ch] == NULL) return WINK_ERR_NOT_INITIALIZED;
    if (!ch_lock_take(ch)) return WINK_ERR_TIMEOUT;
    if (!s_channels[ch].is_initialized) { ch_lock_give(ch); return WINK_ERR_NOT_INITIALIZED; }

    if (!s_channels[ch].has_sample) {
        wink_status_t st = sample_once_locked(ch);
        if (st != WINK_OK) { ch_lock_give(ch); return st; }
    }
    *out_mv = s_channels[ch].last_mv;
    ch_lock_give(ch);
    return WINK_OK;
}
#endif

#else

wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg) {
    (void)ch; (void)cfg;
    return WINK_ERR_UNSUPPORTED;
}
void pal_adc_deinit(pal_adc_channel_t ch) { (void)ch; }
wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin) {
    (void)ch; (void)out_pin; return WINK_ERR_UNSUPPORTED;
}
wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch) {
    (void)pin; (void)out_ch; return WINK_ERR_UNSUPPORTED;
}
wink_status_t pal_adc_full_scale_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    (void)ch; (void)out_mv; return WINK_ERR_UNSUPPORTED;
}
#ifndef WINK_STRICT_NONBLOCKING
wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw) {
    (void)ch; (void)out_raw; return WINK_ERR_UNSUPPORTED;
}
wink_status_t pal_adc_read_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    (void)ch; (void)out_mv; return WINK_ERR_UNSUPPORTED;
}
#endif

#endif
