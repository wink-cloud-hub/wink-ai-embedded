/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_LOG_CONFIG_H
#define WINK_H_GUARD_ESP_LOG_CONFIG_H
#ifndef __WINK_HARVESTED_ESP_LOG_CONFIG_H__
#define __WINK_HARVESTED_ESP_LOG_CONFIG_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_assert.h"
#include "esp_log_level.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_LOG_API_CONSTRAINED_ENV_SAFE
#define ESP_LOG_API_CONSTRAINED_ENV_SAFE (0)
#endif
#ifndef ESP_LOG_COLOR_DISABLED
#define ESP_LOG_COLOR_DISABLED (0)
#endif
#ifndef ESP_LOG_CONFIGS_DEFAULT
#define ESP_LOG_CONFIGS_DEFAULT (  ((ESP_LOG_CONSTRAINED_ENV)     ? (ESP_LOG_CONFIG_CONSTRAINED_ENV)    : 0)  | ((ESP_LOG_FORMATTING_DISABLED) ? (0) : (ESP_LOG_CONFIG_REQUIRE_FORMATTING))  | ((ESP_LOG_COLOR_DISABLED)      ? (ESP_LOG_CONFIG_DIS_COLOR)          : 0)  | ((ESP_LOG_TIMESTAMP_DISABLED)  ? (ESP_LOG_CONFIG_DIS_TIMESTAMP)      : 0)  | ((ESP_LOG_MODE_BINARY_EN)      ? (ESP_LOG_CONFIG_BINARY_MODE)        : 0))
#endif
#ifndef ESP_LOG_CONFIG_BINARY_MODE
#define ESP_LOG_CONFIG_BINARY_MODE (1 << ESP_LOG_OFFSET_BINARY_MODE)
#endif
#ifndef ESP_LOG_CONFIG_CONSTRAINED_ENV
#define ESP_LOG_CONFIG_CONSTRAINED_ENV (1 << ESP_LOG_OFFSET_CONSTRAINED_ENV)
#endif
#ifndef ESP_LOG_CONFIG_DIS_COLOR
#define ESP_LOG_CONFIG_DIS_COLOR (1 << ESP_LOG_OFFSET_DIS_COLOR_OFFSET)
#endif
#ifndef ESP_LOG_CONFIG_DIS_TIMESTAMP
#define ESP_LOG_CONFIG_DIS_TIMESTAMP (1 << ESP_LOG_OFFSET_DIS_TIMESTAMP)
#endif
#ifndef ESP_LOG_CONFIG_INIT
#define ESP_LOG_CONFIG_INIT(configs) ((esp_log_config_t){.data = (configs)})
#endif
#ifndef ESP_LOG_CONFIG_LEVEL_MASK
#define ESP_LOG_CONFIG_LEVEL_MASK ((1 << ESP_LOG_LEVEL_LEN) - 1)
#endif
#ifndef ESP_LOG_CONFIG_REQUIRE_FORMATTING
#define ESP_LOG_CONFIG_REQUIRE_FORMATTING (1 << ESP_LOG_OFFSET_REQUIRE_FORMATTING)
#endif
#ifndef ESP_LOG_CONSTRAINED_ENV
#define ESP_LOG_CONSTRAINED_ENV (0)
#endif
#ifndef ESP_LOG_FORMATTING_DISABLED
#define ESP_LOG_FORMATTING_DISABLED (0)
#endif
#ifndef ESP_LOG_MODE_BINARY_EN
#define ESP_LOG_MODE_BINARY_EN (0)
#endif
#ifndef ESP_LOG_MODE_TEXT_EN
#define ESP_LOG_MODE_TEXT_EN (0)
#endif
#ifndef ESP_LOG_OFFSET_BINARY_MODE
#define ESP_LOG_OFFSET_BINARY_MODE (7)
#endif
#ifndef ESP_LOG_OFFSET_CONSTRAINED_ENV
#define ESP_LOG_OFFSET_CONSTRAINED_ENV (ESP_LOG_LEVEL_LEN)
#endif
#ifndef ESP_LOG_OFFSET_DIS_COLOR_OFFSET
#define ESP_LOG_OFFSET_DIS_COLOR_OFFSET (5)
#endif
#ifndef ESP_LOG_OFFSET_DIS_TIMESTAMP
#define ESP_LOG_OFFSET_DIS_TIMESTAMP (6)
#endif
#ifndef ESP_LOG_OFFSET_REQUIRE_FORMATTING
#define ESP_LOG_OFFSET_REQUIRE_FORMATTING (4)
#endif
#ifndef ESP_LOG_TIMESTAMP_DISABLED
#define ESP_LOG_TIMESTAMP_DISABLED (0)
#endif
#ifndef ESP_LOG_V2
#define ESP_LOG_V2 (1)
#endif
#ifndef ESP_LOG_VERSION
#define ESP_LOG_VERSION (CONFIG_LOG_VERSION)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
union {
        struct {
            esp_log_level_t log_level: ESP_LOG_LEVEL_LEN; 
            uint32_t constrained_env: 1;                  
            uint32_t require_formatting: 1;               
            uint32_t dis_color: 1;                        
            uint32_t dis_timestamp: 1;                    
            uint32_t binary_mode : 1;                     
            uint32_t reserved: 24;                        
        } opts;
        uint32_t data;                                    
    };
} esp_log_config_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_LOG_CONFIG_H__ */
#endif /* WINK_H_GUARD_ESP_LOG_CONFIG_H */
