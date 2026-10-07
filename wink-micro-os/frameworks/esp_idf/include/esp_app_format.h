/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_CHIP_ID_ESP32   = 0x0000,
    ESP_CHIP_ID_ESP32S2 = 0x0002,
    ESP_CHIP_ID_ESP32C3 = 0x0005,
    ESP_CHIP_ID_ESP32S3 = 0x0009,
    ESP_CHIP_ID_ESP32C2 = 0x000C,
    ESP_CHIP_ID_ESP32C6 = 0x000D,
    ESP_CHIP_ID_ESP32H2 = 0x0010,
    ESP_CHIP_ID_INVALID = 0xFFFF
} __attribute__((packed)) esp_chip_id_t;

typedef enum {
    ESP_IMAGE_SPI_MODE_QIO,
    ESP_IMAGE_SPI_MODE_QOUT,
    ESP_IMAGE_SPI_MODE_DIO,
    ESP_IMAGE_SPI_MODE_DOUT,
    ESP_IMAGE_SPI_MODE_FAST_READ,
    ESP_IMAGE_SPI_MODE_SLOW_READ
} esp_image_spi_mode_t;

typedef enum {
    ESP_IMAGE_SPI_SPEED_DIV_2,
    ESP_IMAGE_SPI_SPEED_DIV_3,
    ESP_IMAGE_SPI_SPEED_DIV_4,
    ESP_IMAGE_SPI_SPEED_DIV_1 = 0xF
} esp_image_spi_freq_t;

typedef enum {
    ESP_IMAGE_FLASH_SIZE_1MB = 0,
    ESP_IMAGE_FLASH_SIZE_2MB,
    ESP_IMAGE_FLASH_SIZE_4MB,
    ESP_IMAGE_FLASH_SIZE_8MB,
    ESP_IMAGE_FLASH_SIZE_16MB,
} esp_image_flash_size_t;

typedef struct {
    uint8_t magic;
    uint8_t segment_count;
    uint8_t spi_mode;
    uint8_t spi_speed: 4;
    uint8_t spi_size: 4;
    uint32_t entry_addr;
    uint8_t wp_pin;
    uint8_t spi_pin_drv[3];
    esp_chip_id_t chip_id;
    uint8_t min_chip_rev;
    uint16_t min_chip_rev_full;
    uint16_t max_chip_rev_full;
    uint8_t reserved[4];
    uint8_t hash_appended;
} __attribute__((packed)) esp_image_header_t;

#ifdef __cplusplus
}
#endif
