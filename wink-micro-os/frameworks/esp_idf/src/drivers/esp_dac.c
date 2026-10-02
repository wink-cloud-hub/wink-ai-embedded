// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file esp_dac.c
 * @brief ESP-IDF DAC driver facade (ADR-0002 dual-target, ADR-0045 zero-heap).
 */
#include "driver/dac_oneshot.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_sim_handle.h"
#include <string.h>

#define TAG "esp_dac"

#define MAX_DAC_CHANNELS 2

typedef struct {
    bool in_use;
    dac_channel_t chan_id;
    int pin;
    uint8_t last_value;
    uint32_t token;
} esp_dac_channel_t;

static esp_dac_channel_t s_dac_channels[MAX_DAC_CHANNELS];

static int dac_chan_to_pin(dac_channel_t chan) {
    if (chan == DAC_CHAN_0) {
        return 25;
    } else if (chan == DAC_CHAN_1) {
        return 26;
    }
    return -1;
}

esp_err_t dac_oneshot_new_channel(const dac_oneshot_config_t *oneshot_cfg, dac_oneshot_handle_t *ret_handle) {
    if (!oneshot_cfg || !ret_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (oneshot_cfg->chan_id >= MAX_DAC_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t slot = (uint32_t)oneshot_cfg->chan_id;
    if (s_dac_channels[slot].in_use) {
        return ESP_ERR_INVALID_STATE;
    }

    int pin = dac_chan_to_pin(oneshot_cfg->chan_id);
    if (pin < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    s_dac_channels[slot].in_use = true;
    s_dac_channels[slot].chan_id = oneshot_cfg->chan_id;
    s_dac_channels[slot].pin = pin;
    s_dac_channels[slot].last_value = 0;

    uint32_t token = esp_sim_handle_issue(ESP_SIM_HANDLE_DAC_ONESHOT, slot);
    s_dac_channels[slot].token = token;
    *ret_handle = (dac_oneshot_handle_t)(uintptr_t)token;

    ESP_LOGI(TAG, "DAC oneshot channel %d (GPIO %d) initialized", oneshot_cfg->chan_id, pin);
    return ESP_OK;
}

esp_err_t dac_oneshot_del_channel(dac_oneshot_handle_t handle) {
    uint32_t slot = 0;
    if (!esp_sim_handle_decode((const void *)(uintptr_t)handle, ESP_SIM_HANDLE_DAC_ONESHOT, MAX_DAC_CHANNELS, &slot)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_dac_channels[slot].in_use) {
        return ESP_ERR_INVALID_STATE;
    }

    s_dac_channels[slot].in_use = false;
    s_dac_channels[slot].token = 0;
    ESP_LOGI(TAG, "DAC oneshot channel %d deleted", s_dac_channels[slot].chan_id);
    return ESP_OK;
}

esp_err_t dac_oneshot_output_voltage(dac_oneshot_handle_t handle, uint8_t digi_value) {
    uint32_t slot = 0;
    if (!esp_sim_handle_decode((const void *)(uintptr_t)handle, ESP_SIM_HANDLE_DAC_ONESHOT, MAX_DAC_CHANNELS, &slot)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_dac_channels[slot].in_use) {
        return ESP_ERR_INVALID_STATE;
    }

    s_dac_channels[slot].last_value = digi_value;
    ESP_LOGD(TAG, "DAC channel %d output voltage: %d", s_dac_channels[slot].chan_id, digi_value);
    return ESP_OK;
}
