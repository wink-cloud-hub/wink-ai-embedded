/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_LOG_LEVEL_H
#define WINK_H_GUARD_ESP_LOG_LEVEL_H
#ifndef __WINK_HARVESTED_ESP_LOG_LEVEL_H__
#define __WINK_HARVESTED_ESP_LOG_LEVEL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_assert.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_LOG_ENABLED
#define ESP_LOG_ENABLED(configs) (LOG_LOCAL_LEVEL >= ESP_LOG_GET_LEVEL(configs))
#endif
#ifndef ESP_LOG_GET_LEVEL
#define ESP_LOG_GET_LEVEL(config) ((config) & ESP_LOG_LEVEL_MASK)
#endif
#ifndef ESP_LOG_LEVEL_LEN
#define ESP_LOG_LEVEL_LEN (3)
#endif
#ifndef ESP_LOG_LEVEL_MASK
#define ESP_LOG_LEVEL_MASK ((1 << ESP_LOG_LEVEL_LEN) - 1)
#endif
#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL CONFIG_LOG_MAXIMUM_LEVEL
#endif
#ifndef _ESP_LOG_EARLY_ENABLED
#define _ESP_LOG_EARLY_ENABLED(log_level) (ESP_LOG_ENABLED(log_level) && esp_log_get_default_level() >= ESP_LOG_GET_LEVEL(log_level))
#endif
#ifndef _ESP_LOG_ENABLED
#define _ESP_LOG_ENABLED(log_level) ESP_LOG_ENABLED(log_level)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_LOG_NONE = 0,
    ESP_LOG_ERROR = 1,
    ESP_LOG_WARN = 2,
    ESP_LOG_INFO = 3,
    ESP_LOG_DEBUG = 4,
    ESP_LOG_VERBOSE = 5,
    ESP_LOG_MAX = 6,
} esp_log_level_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

esp_log_level_t esp_log_get_default_level(void);
esp_log_level_t esp_log_level_get(const char* tag);
void esp_log_level_set(const char* tag, esp_log_level_t level);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_LOG_LEVEL_H__ */
#endif /* WINK_H_GUARD_ESP_LOG_LEVEL_H */
