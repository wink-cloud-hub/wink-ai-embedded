/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_SIM_HANDLE_H
#define ESP_SIM_HANDLE_H

#include <stdbool.h>
#include <stdint.h>

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
};

/* Encoded pointer values are opaque tokens. Never dereference them. */
uint32_t esp_sim_handle_issue(uint32_t kind, uint32_t slot);
bool esp_sim_handle_decode(const void *handle, uint32_t expected_kind,
                           uint32_t capacity, uint32_t *out_slot);

uint32_t esp_sim_handle_get_sequence(void);
void esp_sim_handle_set_sequence_base(uint32_t base);

#endif
