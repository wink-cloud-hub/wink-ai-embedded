/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef WINK_H_GUARD_ESP_FLASH_H
#define WINK_H_GUARD_ESP_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_flash_t esp_flash_t;

/**
 * @brief Get flash chip size of the specified chip
 *
 * @param chip Pointer to identify the flash chip. Pass NULL to use default main flash.
 * @param[out] out_size Pointer to store flash chip size in bytes.
 * @return esp_err_t ESP_OK on success, or error code on failure.
 */
esp_err_t esp_flash_get_size(esp_flash_t *chip, uint32_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* WINK_H_GUARD_ESP_FLASH_H */
