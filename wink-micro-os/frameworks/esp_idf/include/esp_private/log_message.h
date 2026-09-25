/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_LOG_MESSAGE_H
#define WINK_H_GUARD_ESP_PRIVATE_LOG_MESSAGE_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_LOG_MESSAGE_H__
#define __WINK_HARVESTED_ESP_PRIVATE_LOG_MESSAGE_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdarg.h>

#include "esp_log_config.h"
#include "esp_log_level.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    esp_log_config_t config;
    const char * tag;
    const char * format;
    uint64_t timestamp;
    const char * arg_types;
    va_list args;
} esp_log_msg_t;



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_LOG_MESSAGE_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_LOG_MESSAGE_H */
