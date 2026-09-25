/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_LOG_LEVEL_H
#define WINK_H_GUARD_ESP_PRIVATE_LOG_LEVEL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_LOG_LEVEL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_LOG_LEVEL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

bool esp_log_is_tag_loggable(esp_log_level_t level, const char *tag);
esp_log_level_t esp_log_level_get_timeout(const char *tag);
void esp_log_set_default_level(esp_log_level_t level);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_LOG_LEVEL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_LOG_LEVEL_H */
