/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_APP_DESC_MAGIC_WORD (0xABCD5432)

typedef struct {
    uint32_t magic_word;
    uint32_t secure_version;
    uint32_t reserv1[2];
    char version[32];
    char project_name[32];
    char time[16];
    char date[16];
    char idf_ver[32];
    uint8_t app_elf_sha256[32];
    uint16_t min_efuse_blk_rev_full;
    uint16_t max_efuse_blk_rev_full;
    uint8_t mmu_page_size;
    uint8_t spi_flash_mode;
    uint8_t reserv3[2];
    uint32_t reserv2[18];
} esp_app_desc_t;

const esp_app_desc_t *esp_app_get_description(void);

int esp_app_get_elf_sha256(char* dst, size_t size);

#ifdef __cplusplus
}
#endif
