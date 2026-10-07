/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline esp_err_t esp_crt_bundle_attach(void *conf) {
    (void)conf;
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif
