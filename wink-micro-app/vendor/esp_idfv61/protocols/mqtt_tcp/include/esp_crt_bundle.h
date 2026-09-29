/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include "esp_err.h"

static inline esp_err_t esp_crt_bundle_attach(void *conf) {
    (void)conf;
    return ESP_OK;
}
