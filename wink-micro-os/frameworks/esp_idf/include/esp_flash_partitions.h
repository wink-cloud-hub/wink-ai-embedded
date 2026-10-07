/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PARTITION_MAGIC     0x50AA
#define ESP_PARTITION_MAGIC_MD5 0xEBEB

#define PART_TYPE_APP 0x00
#define PART_SUBTYPE_FACTORY  0x00
#define PART_SUBTYPE_OTA_FLAG 0x10
#define PART_SUBTYPE_OTA_MASK 0x0f
#define PART_SUBTYPE_TEST     0x20

#define PART_TYPE_DATA 0x01
#define PART_SUBTYPE_DATA_OTA 0x00
#define PART_SUBTYPE_DATA_RF  0x01
#define PART_SUBTYPE_DATA_WIFI 0x02
#define PART_SUBTYPE_DATA_NVS_KEYS 0x04

#define PART_TYPE_BOOTLOADER 0x02
#define PART_SUBTYPE_BOOTLOADER_PRIMARY 0x00
#define PART_SUBTYPE_BOOTLOADER_OTA 0x01
#define PART_SUBTYPE_BOOTLOADER_RECOVERY 0x02

#define PART_TYPE_PARTITION_TABLE 0x03
#define PART_SUBTYPE_PARTITION_TABLE_PRIMARY 0x00
#define PART_SUBTYPE_PARTITION_TABLE_OTA 0x01

#ifndef CONFIG_BOOTLOADER_OFFSET_IN_FLASH
#define CONFIG_BOOTLOADER_OFFSET_IN_FLASH 0x1000
#endif

#ifndef CONFIG_PARTITION_TABLE_OFFSET
#define CONFIG_PARTITION_TABLE_OFFSET 0x8000
#endif

#define ESP_BOOTLOADER_DIGEST_OFFSET        0x0
#define ESP_BOOTLOADER_OFFSET               CONFIG_BOOTLOADER_OFFSET_IN_FLASH
#define ESP_PRIMARY_BOOTLOADER_OFFSET       CONFIG_BOOTLOADER_OFFSET_IN_FLASH
#define ESP_PARTITION_TABLE_OFFSET          CONFIG_PARTITION_TABLE_OFFSET
#define ESP_PRIMARY_PARTITION_TABLE_OFFSET  CONFIG_PARTITION_TABLE_OFFSET
#define ESP_PARTITION_TABLE_SIZE            (0x1000)
#define ESP_BOOTLOADER_SIZE                 (ESP_PARTITION_TABLE_OFFSET - ESP_BOOTLOADER_OFFSET)

typedef enum {
    ESP_OTA_IMG_NEW             = 0x0U,
    ESP_OTA_IMG_PENDING_VERIFY  = 0x1U,
    ESP_OTA_IMG_VALID           = 0x2U,
    ESP_OTA_IMG_INVALID         = 0x3U,
    ESP_OTA_IMG_ABORTED         = 0x4U,
    ESP_OTA_IMG_UNDEFINED       = 0xFFFFFFFFU,
} esp_ota_img_states_t;

typedef struct {
    uint32_t ota_seq;
    uint8_t  seq_label[20];
    uint32_t ota_state;
    uint32_t crc;
} esp_ota_select_entry_t;

typedef struct {
    uint32_t offset;
    uint32_t size;
} esp_partition_pos_t;

typedef struct {
    uint16_t magic;
    uint8_t  type;
    uint8_t  subtype;
    esp_partition_pos_t pos;
    uint8_t  label[16];
    uint32_t flags;
} esp_partition_info_t;

#ifdef __cplusplus
}
#endif
