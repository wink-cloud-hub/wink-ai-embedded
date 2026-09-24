/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_PM_H
#define ESP_PM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int max_freq_mhz;
    int min_freq_mhz;
    bool light_sleep_enable;
} esp_pm_config_t;

static inline esp_err_t esp_pm_configure(const void *config) {
    (void)config;
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* ESP_PM_H */
