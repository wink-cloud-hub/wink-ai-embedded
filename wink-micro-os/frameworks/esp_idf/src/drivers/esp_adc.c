/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/pal_adc.h"
#include "soc/adc_channel.h"
#include "esp_sim_handle.h"
#include <string.h>

#define MAX_ADC_UNITS 2
#define MAX_ADC_CHANNELS 10
#define MAX_CALI_HANDLES 8

typedef struct {
    bool in_use;
    adc_unit_t unit_id;
    uint32_t token;
    struct {
        bool configured;
        adc_atten_t atten;
        adc_bitwidth_t bitwidth;
        uint16_t full_scale_mv;
        pal_adc_channel_t pal_ch;
    } channels[MAX_ADC_CHANNELS];
} esp_adc_unit_t;

static esp_adc_unit_t s_adc_units[MAX_ADC_UNITS];

typedef struct {
    bool in_use;
    adc_unit_t unit_id;
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
    uint16_t full_scale_mv;
    uint32_t token;
} esp_cali_scheme_t;

static esp_cali_scheme_t s_cali_schemes[MAX_CALI_HANDLES];

static uint16_t get_atten_full_scale_mv(adc_atten_t atten) {
    switch (atten) {
        case ADC_ATTEN_DB_0:   return 950;
        case ADC_ATTEN_DB_2_5: return 1250;
        case ADC_ATTEN_DB_6:   return 1750;
        case ADC_ATTEN_DB_12:
        default:               return 3100;
    }
}

static wink_pin_t adc_unit_channel_to_pin(adc_unit_t unit, adc_channel_t channel) {
    if (unit == ADC_UNIT_1) {
        switch (channel) {
            case ADC_CHANNEL_0: return 36;
            case ADC_CHANNEL_1: return 37;
            case ADC_CHANNEL_2: return 38;
            case ADC_CHANNEL_3: return 39;
            case ADC_CHANNEL_4: return 32;
            case ADC_CHANNEL_5: return 33;
            case ADC_CHANNEL_6: return 34;
            case ADC_CHANNEL_7: return 35;
            default: return -1;
        }
    } else if (unit == ADC_UNIT_2) {
        switch (channel) {
            case ADC_CHANNEL_0: return 4;
            case ADC_CHANNEL_1: return 0;
            case ADC_CHANNEL_2: return 2;
            case ADC_CHANNEL_3: return 15;
            case ADC_CHANNEL_4: return 13;
            case ADC_CHANNEL_5: return 12;
            case ADC_CHANNEL_6: return 14;
            case ADC_CHANNEL_7: return 27;
            case ADC_CHANNEL_8: return 25;
            case ADC_CHANNEL_9: return 26;
            default: return -1;
        }
    }
    return -1;
}

esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *init_config, adc_oneshot_unit_handle_t *ret_unit) {
    if (!init_config || !ret_unit) {
        return ESP_ERR_INVALID_ARG;
    }
    if (init_config->unit_id >= MAX_ADC_UNITS) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t slot = (uint32_t)init_config->unit_id;
    esp_adc_unit_t *u = &s_adc_units[slot];
    if (u->in_use) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(u, 0, sizeof(*u));
    u->in_use = true;
    u->unit_id = init_config->unit_id;
    u->token = esp_sim_handle_issue(ESP_SIM_HANDLE_ADC_ONESHOT, slot);

    *ret_unit = (adc_oneshot_unit_handle_t)(uintptr_t)u->token;
    return ESP_OK;
}

esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle, adc_channel_t channel, const adc_oneshot_chan_cfg_t *config) {
    if (!handle || !config || channel >= MAX_ADC_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t slot = 0;
    if (!esp_sim_handle_decode(handle, ESP_SIM_HANDLE_ADC_ONESHOT, MAX_ADC_UNITS, &slot)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_adc_unit_t *u = &s_adc_units[slot];
    if (!u->in_use || u->token != (uint32_t)(uintptr_t)handle) {
        return ESP_ERR_INVALID_STATE;
    }

    wink_pin_t pin = adc_unit_channel_to_pin(u->unit_id, channel);
    if (pin < 0) {
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t fs_mv = get_atten_full_scale_mv(config->atten);
    uint8_t bits = (config->bitwidth == ADC_BITWIDTH_DEFAULT) ? 12 : (uint8_t)config->bitwidth;

    pal_adc_config_t pal_cfg = {
        .pin = pin,
        .full_scale_mv = fs_mv,
        .resolution_bits = bits,
    };

    pal_adc_channel_t pal_ch = 0;
    wink_status_t st = pal_adc_acquire(pin, &pal_cfg, &pal_ch);
    if (st != WINK_OK) {
        return ESP_FAIL;
    }

    u->channels[channel].configured = true;
    u->channels[channel].atten = config->atten;
    u->channels[channel].bitwidth = config->bitwidth;
    u->channels[channel].full_scale_mv = fs_mv;
    u->channels[channel].pal_ch = pal_ch;

    return ESP_OK;
}

esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle, adc_channel_t chan, int *out_raw) {
    if (!handle || !out_raw || chan >= MAX_ADC_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t slot = 0;
    if (!esp_sim_handle_decode(handle, ESP_SIM_HANDLE_ADC_ONESHOT, MAX_ADC_UNITS, &slot)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_adc_unit_t *u = &s_adc_units[slot];
    if (!u->in_use || u->token != (uint32_t)(uintptr_t)handle) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!u->channels[chan].configured) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw_val = 0;
    wink_status_t st = pal_adc_read_raw(u->channels[chan].pal_ch, &raw_val);
    if (st != WINK_OK) {
        return ESP_FAIL;
    }

    *out_raw = (int)raw_val;
    return ESP_OK;
}

esp_err_t adc_oneshot_del_unit(adc_oneshot_unit_handle_t handle) {
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t slot = 0;
    if (!esp_sim_handle_decode(handle, ESP_SIM_HANDLE_ADC_ONESHOT, MAX_ADC_UNITS, &slot)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_adc_unit_t *u = &s_adc_units[slot];
    if (!u->in_use || u->token != (uint32_t)(uintptr_t)handle) {
        return ESP_ERR_INVALID_STATE;
    }

    for (int i = 0; i < MAX_ADC_CHANNELS; i++) {
        if (u->channels[i].configured) {
            (void)pal_adc_release(u->channels[i].pal_ch);
            u->channels[i].configured = false;
        }
    }

    u->in_use = false;
    return ESP_OK;
}

esp_err_t adc_oneshot_io_to_channel(int io_num, adc_unit_t *unit_id, adc_channel_t *channel) {
    if (!unit_id || !channel) return ESP_ERR_INVALID_ARG;
    for (int u = 0; u < MAX_ADC_UNITS; u++) {
        for (int ch = 0; ch < MAX_ADC_CHANNELS; ch++) {
            if (adc_unit_channel_to_pin((adc_unit_t)u, (adc_channel_t)ch) == (wink_pin_t)io_num) {
                *unit_id = (adc_unit_t)u;
                *channel = (adc_channel_t)ch;
                return ESP_OK;
            }
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t adc_oneshot_channel_to_io(adc_unit_t unit_id, adc_channel_t channel, int *io_num) {
    if (!io_num) return ESP_ERR_INVALID_ARG;
    wink_pin_t pin = adc_unit_channel_to_pin(unit_id, channel);
    if (pin < 0) return ESP_ERR_NOT_FOUND;
    *io_num = (int)pin;
    return ESP_OK;
}

/* --- Calibration Line Fitting --- */

esp_err_t adc_cali_create_scheme_line_fitting(const adc_cali_line_fitting_config_t *config, adc_cali_handle_t *ret_handle) {
    if (!config || !ret_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    int free_slot = -1;
    for (int i = 0; i < MAX_CALI_HANDLES; i++) {
        if (!s_cali_schemes[i].in_use) {
            free_slot = i;
            break;
        }
    }

    if (free_slot < 0) {
        return ESP_ERR_NO_MEM;
    }

    esp_cali_scheme_t *c = &s_cali_schemes[free_slot];
    c->in_use = true;
    c->unit_id = config->unit_id;
    c->atten = config->atten;
    c->bitwidth = config->bitwidth;
    c->full_scale_mv = get_atten_full_scale_mv(config->atten);
    c->token = esp_sim_handle_issue(ESP_SIM_HANDLE_ADC_CALI, (uint32_t)free_slot);

    *ret_handle = (adc_cali_handle_t)(uintptr_t)c->token;
    return ESP_OK;
}

esp_err_t adc_cali_delete_scheme_line_fitting(adc_cali_handle_t handle) {
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t slot = 0;
    if (!esp_sim_handle_decode(handle, ESP_SIM_HANDLE_ADC_CALI, MAX_CALI_HANDLES, &slot)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_cali_scheme_t *c = &s_cali_schemes[slot];
    if (!c->in_use || c->token != (uint32_t)(uintptr_t)handle) {
        return ESP_ERR_INVALID_STATE;
    }

    c->in_use = false;
    return ESP_OK;
}

esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t handle, int raw, int *voltage) {
    if (!handle || !voltage) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t slot = 0;
    if (!esp_sim_handle_decode(handle, ESP_SIM_HANDLE_ADC_CALI, MAX_CALI_HANDLES, &slot)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_cali_scheme_t *c = &s_cali_schemes[slot];
    if (!c->in_use || c->token != (uint32_t)(uintptr_t)handle) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t max_raw = (c->bitwidth == ADC_BITWIDTH_DEFAULT || c->bitwidth == ADC_BITWIDTH_12) ? 4095 : ((1U << c->bitwidth) - 1U);
    if (raw < 0) raw = 0;
    if ((uint32_t)raw > max_raw) raw = (int)max_raw;

    uint32_t mv = (uint32_t)(((uint64_t)raw * c->full_scale_mv + (max_raw / 2)) / max_raw);
    *voltage = (int)mv;
    return ESP_OK;
}

esp_err_t adc_cali_check_scheme(adc_cali_scheme_ver_t *scheme_mask) {
    if (!scheme_mask) return ESP_ERR_INVALID_ARG;
    *scheme_mask = ADC_CALI_SCHEME_VER_LINE_FITTING;
    return ESP_OK;
}

esp_err_t adc_cali_scheme_line_fitting_check_efuse(adc_cali_line_fitting_efuse_val_t *cali_val) {
    if (!cali_val) return ESP_ERR_INVALID_ARG;
    *cali_val = ADC_CALI_LINE_FITTING_EFUSE_VAL_DEFAULT_VREF;
    return ESP_OK;
}

esp_err_t adc_oneshot_get_calibrated_result(adc_oneshot_unit_handle_t handle, adc_cali_handle_t cali_handle, adc_channel_t chan, int *cali_result) {
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(handle, chan, &raw);
    if (ret != ESP_OK) {
        return ret;
    }
    return adc_cali_raw_to_voltage(cali_handle, raw, cali_result);
}
