/* SPDX-License-Identifier: CC0-1.0 */
#pragma once
#include "esp_err.h"

typedef struct esp_tls esp_tls_t;
typedef void* esp_tls_error_handle_t;

static inline esp_err_t esp_tls_get_and_clear_last_error(esp_tls_error_handle_t h, int *mbedtls_err, int *other_err) {
    (void)h;
    if (mbedtls_err) *mbedtls_err = 0;
    if (other_err) *other_err = 0;
    return ESP_OK;
}
