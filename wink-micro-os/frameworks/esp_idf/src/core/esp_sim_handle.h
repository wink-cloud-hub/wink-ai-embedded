/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_SIM_HANDLE_H
#define ESP_SIM_HANDLE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

enum {
    ESP_SIM_HANDLE_QUEUE = 1,
    ESP_SIM_HANDLE_SEMAPHORE = 2,
    ESP_SIM_HANDLE_EVENT_GROUP = 3,
    ESP_SIM_HANDLE_TASK = 4,
    ESP_SIM_HANDLE_NVS = 5,
    ESP_SIM_HANDLE_GPTIMER = 6,
    ESP_SIM_HANDLE_I2C_BUS = 7,
    ESP_SIM_HANDLE_I2C = 7,
    ESP_SIM_HANDLE_SPI = 8,
    ESP_SIM_HANDLE_I2C_DEV = 9,
    ESP_SIM_HANDLE_TIMER = 10,
    ESP_SIM_HANDLE_ADC_ONESHOT = 11,
    ESP_SIM_HANDLE_ADC_CALI = 12,
    ESP_SIM_HANDLE_DAC_ONESHOT = 13,
    ESP_SIM_HANDLE_ADC_CONTINUOUS = 14,
};

#ifdef __cplusplus
extern "C" {
#endif

/* Encoded pointer values are opaque tokens. Never dereference them. */
EMSCRIPTEN_KEEPALIVE uint32_t esp_sim_handle_issue(uint32_t kind, uint32_t slot);
EMSCRIPTEN_KEEPALIVE bool esp_sim_handle_decode(const void *handle, uint32_t expected_kind,
                                                uint32_t capacity, uint32_t *out_slot);

EMSCRIPTEN_KEEPALIVE uint32_t esp_sim_handle_get_sequence(void);
EMSCRIPTEN_KEEPALIVE void esp_sim_handle_set_sequence_base(uint32_t base);

#ifdef __cplusplus
}
#endif

#endif
