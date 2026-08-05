/**
 * @file pal_hal_adc_esp32.c
 * @brief ESP32 target 的 PAL ADC 子系统实现（基于 ESP-IDF v6.0.1 adc_oneshot 及 adc_cali 驱动）。
 */
#include "hal/pal_adc.h"
#include "pal_resource.h"
#include <string.h>

#if defined(ESP_PLATFORM)

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "portmacro.h"

#ifndef ADC_ATTEN_DB_12
#define ADC_ATTEN_DB_12 ADC_ATTEN_DB_11
#endif

#define ESP32_ADC_NUM_UNITS 2

typedef struct {
    bool              is_initialized;
    wink_pin_t        pin;
    adc_unit_t        unit_id;
    adc_channel_t     adc_chan;
    uint16_t          full_scale_mv;
    uint8_t           resolution_bits;
    adc_cali_handle_t cali_handle;
    bool              has_cali;
    uint16_t          last_raw;
    uint16_t          last_mv;
} esp32_adc_channel_state_t;

static esp32_adc_channel_state_t s_channels[PAL_ADC_CHANNELS];
static adc_oneshot_unit_handle_t s_unit_handles[ESP32_ADC_NUM_UNITS];
static uint8_t                   s_unit_refcount[ESP32_ADC_NUM_UNITS];
static portMUX_TYPE              s_adc_spinlock = portMUX_INITIALIZER_UNLOCKED;

static wink_status_t esp_err_to_wink_status(esp_err_t err) {
    switch (err) {
        case ESP_OK:                  return WINK_OK;
        case ESP_ERR_INVALID_ARG:     return WINK_ERR_INVALID_ARG;
        case ESP_ERR_INVALID_STATE:   return WINK_ERR_UNSUPPORTED;
        case ESP_ERR_NOT_FOUND:       return WINK_ERR_NOT_FOUND;
        case ESP_ERR_TIMEOUT:         return WINK_ERR_TIMEOUT;
        default:                      return WINK_ERR_HARDWARE;
    }
}

static adc_atten_t select_attenuation(uint16_t full_scale_mv) {
    if (full_scale_mv == 0 || full_scale_mv >= 3100) {
        return ADC_ATTEN_DB_12;
    } else if (full_scale_mv <= 1100) {
        return ADC_ATTEN_DB_0;
    } else if (full_scale_mv <= 1500) {
        return ADC_ATTEN_DB_2_5;
    } else {
        return ADC_ATTEN_DB_6;
    }
}

wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg) {
    if (ch >= PAL_ADC_CHANNELS || cfg == NULL || cfg->pin < 0) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_channels[ch].is_initialized) {
        return WINK_ERR_ALREADY_INITIALIZED;
    }

    adc_unit_t unit_id;
    adc_channel_t adc_chan;
    esp_err_t err = adc_oneshot_io_to_channel((gpio_num_t)cfg->pin, &unit_id, &adc_chan);
    if (err != ESP_OK || (uint8_t)unit_id >= ESP32_ADC_NUM_UNITS) {
        return WINK_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&s_adc_spinlock);
    if (s_unit_refcount[unit_id] == 0) {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = unit_id,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        err = adc_oneshot_new_unit(&init_config, &s_unit_handles[unit_id]);
        if (err != ESP_OK) {
            portEXIT_CRITICAL(&s_adc_spinlock);
            return esp_err_to_wink_status(err);
        }
    }
    s_unit_refcount[unit_id]++;
    portEXIT_CRITICAL(&s_adc_spinlock);

    uint8_t res_bits = (cfg->resolution_bits > 0) ? cfg->resolution_bits : 12;
    adc_bitwidth_t bitwidth = (res_bits == 12) ? ADC_BITWIDTH_12 : ADC_BITWIDTH_DEFAULT;
    adc_atten_t atten = select_attenuation(cfg->full_scale_mv);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = atten,
        .bitwidth = bitwidth,
    };

    err = adc_oneshot_config_channel(s_unit_handles[unit_id], adc_chan, &chan_cfg);
    if (err != ESP_OK) {
        portENTER_CRITICAL(&s_adc_spinlock);
        s_unit_refcount[unit_id]--;
        if (s_unit_refcount[unit_id] == 0) {
            adc_oneshot_del_unit(s_unit_handles[unit_id]);
            s_unit_handles[unit_id] = NULL;
        }
        portEXIT_CRITICAL(&s_adc_spinlock);
        return esp_err_to_wink_status(err);
    }

    s_channels[ch].is_initialized = true;
    s_channels[ch].pin = cfg->pin;
    s_channels[ch].unit_id = unit_id;
    s_channels[ch].adc_chan = adc_chan;
    s_channels[ch].full_scale_mv = (cfg->full_scale_mv > 0) ? cfg->full_scale_mv : 3100;
    s_channels[ch].resolution_bits = res_bits;
    s_channels[ch].has_cali = false;
    s_channels[ch].cali_handle = NULL;
    s_channels[ch].last_raw = 0;
    s_channels[ch].last_mv = 0;

    /* 校准方案创建 (curve fitting 优先，回退 line fitting) */
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit_id,
        .chan = adc_chan,
        .atten = atten,
        .bitwidth = bitwidth,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &s_channels[ch].cali_handle) == ESP_OK) {
        s_channels[ch].has_cali = true;
    }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit_id,
        .atten = atten,
        .bitwidth = bitwidth,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_config, &s_channels[ch].cali_handle) == ESP_OK) {
        s_channels[ch].has_cali = true;
    }
#endif

    return WINK_OK;
}

void pal_adc_deinit(pal_adc_channel_t ch) {
    if (ch >= PAL_ADC_CHANNELS || !s_channels[ch].is_initialized) {
        return;
    }

    if (s_channels[ch].has_cali && s_channels[ch].cali_handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(s_channels[ch].cali_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_delete_scheme_line_fitting(s_channels[ch].cali_handle);
#endif
        s_channels[ch].has_cali = false;
        s_channels[ch].cali_handle = NULL;
    }

    adc_unit_t unit_id = s_channels[ch].unit_id;
    portENTER_CRITICAL(&s_adc_spinlock);
    if ((uint8_t)unit_id < ESP32_ADC_NUM_UNITS && s_unit_refcount[unit_id] > 0) {
        s_unit_refcount[unit_id]--;
        if (s_unit_refcount[unit_id] == 0) {
            adc_oneshot_del_unit(s_unit_handles[unit_id]);
            s_unit_handles[unit_id] = NULL;
        }
    }
    portEXIT_CRITICAL(&s_adc_spinlock);

    memset(&s_channels[ch], 0, sizeof(esp32_adc_channel_state_t));
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

    int raw_val = 0;
    adc_unit_t unit_id = s_channels[ch].unit_id;
    adc_channel_t adc_chan = s_channels[ch].adc_chan;

    esp_err_t err = adc_oneshot_read(s_unit_handles[unit_id], adc_chan, &raw_val);
    if (err != ESP_OK) {
        return esp_err_to_wink_status(err);
    }

    if (raw_val < 0) raw_val = 0;
    uint32_t max_raw = (1U << s_channels[ch].resolution_bits) - 1U;
    if ((uint32_t)raw_val > max_raw) raw_val = (int)max_raw;

    s_channels[ch].last_raw = (uint16_t)raw_val;

    /* 若已创建校准，计算电压缓存 */
    if (s_channels[ch].has_cali && s_channels[ch].cali_handle) {
        int voltage_mv = 0;
        if (adc_cali_raw_to_voltage(s_channels[ch].cali_handle, raw_val, &voltage_mv) == ESP_OK) {
            if (voltage_mv < 0) voltage_mv = 0;
            if (voltage_mv > s_channels[ch].full_scale_mv) voltage_mv = s_channels[ch].full_scale_mv;
            s_channels[ch].last_mv = (uint16_t)voltage_mv;
        } else {
            s_channels[ch].last_mv = (uint16_t)(((uint32_t)raw_val * s_channels[ch].full_scale_mv) / max_raw);
        }
    } else {
        s_channels[ch].last_mv = (uint16_t)(((uint32_t)raw_val * s_channels[ch].full_scale_mv) / max_raw);
    }

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

    /* 优先复用上一帧缓存 Raw 进行校准换算（若未更新过，先触发一次 read_raw） */
    if (s_channels[ch].last_raw == 0 && s_channels[ch].last_mv == 0) {
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

#else /* Non-ESP_PLATFORM Stub */

wink_status_t pal_adc_init(pal_adc_channel_t ch, const pal_adc_config_t *cfg) {
    (void)ch; (void)cfg;
    return WINK_ERR_UNSUPPORTED;
}
void pal_adc_deinit(pal_adc_channel_t ch) { (void)ch; }
wink_status_t pal_adc_channel_pin(pal_adc_channel_t ch, wink_pin_t *out_pin) {
    (void)ch; (void)out_pin;
    return WINK_ERR_UNSUPPORTED;
}
wink_status_t pal_adc_pin_channel(wink_pin_t pin, pal_adc_channel_t *out_ch) {
    (void)pin; (void)out_ch;
    return WINK_ERR_UNSUPPORTED;
}
wink_status_t pal_adc_full_scale_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    (void)ch; (void)out_mv;
    return WINK_ERR_UNSUPPORTED;
}
#ifndef WINK_STRICT_NONBLOCKING
wink_status_t pal_adc_read_raw(pal_adc_channel_t ch, uint16_t *out_raw) {
    (void)ch; (void)out_raw;
    return WINK_ERR_UNSUPPORTED;
}
wink_status_t pal_adc_read_mv(pal_adc_channel_t ch, uint16_t *out_mv) {
    (void)ch; (void)out_mv;
    return WINK_ERR_UNSUPPORTED;
}
#endif

#endif /* ESP_PLATFORM */
