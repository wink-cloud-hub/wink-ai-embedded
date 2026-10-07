/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_BOOTLOADER_DESC_MAGIC_BYTE (80)

typedef struct {
    uint8_t magic_byte;
    uint8_t reserved[2];
    uint8_t secure_version;
    uint32_t version;
    char idf_ver[32];
    char date_time[24];
    uint8_t reserved2[16];
} esp_bootloader_desc_t;

const esp_bootloader_desc_t *esp_bootloader_get_description(void);

#ifdef __cplusplus
}
#endif
