/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_CHIP_INFO_H
#define WINK_H_GUARD_ESP_CHIP_INFO_H
#ifndef __WINK_HARVESTED_ESP_CHIP_INFO_H__
#define __WINK_HARVESTED_ESP_CHIP_INFO_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_bit_defs.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef CHIP_FEATURE_BLE
#define CHIP_FEATURE_BLE BIT(4)
#endif
#ifndef CHIP_FEATURE_BT
#define CHIP_FEATURE_BT BIT(5)
#endif
#ifndef CHIP_FEATURE_EMB_FLASH
#define CHIP_FEATURE_EMB_FLASH BIT(0)
#endif
#ifndef CHIP_FEATURE_EMB_PSRAM
#define CHIP_FEATURE_EMB_PSRAM BIT(7)
#endif
#ifndef CHIP_FEATURE_IEEE802154
#define CHIP_FEATURE_IEEE802154 BIT(6)
#endif
#ifndef CHIP_FEATURE_WIFI_BGN
#define CHIP_FEATURE_WIFI_BGN BIT(1)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    CHIP_ESP32 = 1,
    CHIP_ESP32S2 = 2,
    CHIP_ESP32S3 = 9,
    CHIP_ESP32C3 = 5,
    CHIP_ESP32C2 = 12,
    CHIP_ESP32C6 = 13,
    CHIP_ESP32H2 = 16,
    CHIP_ESP32P4 = 18,
    CHIP_ESP32C61 = 20,
    CHIP_ESP32C5 = 23,
    CHIP_ESP32H21 = 25,
    CHIP_ESP32H4 = 28,
    CHIP_ESP32S31 = 32,
    CHIP_POSIX_LINUX = 999,
} esp_chip_model_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    esp_chip_model_t model;
    uint32_t features;
    uint16_t revision;
    uint8_t cores;
} esp_chip_info_t;

void esp_chip_info(esp_chip_info_t* out_info);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_CHIP_INFO_H__ */
#endif /* WINK_H_GUARD_ESP_CHIP_INFO_H */
