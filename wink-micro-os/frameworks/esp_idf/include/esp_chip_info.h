/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_CHIP_INFO_H
#define ESP_CHIP_INFO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CHIP_ESP32 = 1,
    CHIP_ESP32S2 = 2,
    CHIP_ESP32S3 = 9,
    CHIP_ESP32C3 = 5,
    CHIP_ESP32C2 = 12,
    CHIP_ESP32C6 = 13,
    CHIP_ESP32H2 = 16,
    CHIP_POSIX_LINUX = 999,
} esp_chip_model_t;

typedef struct {
    esp_chip_model_t model;
    uint32_t features;
    uint16_t revision;
    uint8_t cores;
} esp_chip_info_t;

#define CHIP_FEATURE_EMB_FLASH      (1 << 0)
#define CHIP_FEATURE_WIFI_BGN       (1 << 1)
#define CHIP_FEATURE_BLE            (1 << 4)
#define CHIP_FEATURE_BT             (1 << 5)
#define CHIP_FEATURE_IEEE802154     (1 << 6)
#define CHIP_FEATURE_EMB_PSRAM      (1 << 7)

void esp_chip_info(esp_chip_info_t *out_info);

#ifdef __cplusplus
}
#endif

#endif /* ESP_CHIP_INFO_H */
