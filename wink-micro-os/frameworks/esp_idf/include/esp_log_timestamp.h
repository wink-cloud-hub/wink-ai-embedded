/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_LOG_TIMESTAMP_H
#define WINK_H_GUARD_ESP_LOG_TIMESTAMP_H
#ifndef __WINK_HARVESTED_ESP_LOG_TIMESTAMP_H__
#define __WINK_HARVESTED_ESP_LOG_TIMESTAMP_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_log_config.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef ESP_LOG_SUPPORT_TIMESTAMP
#define ESP_LOG_SUPPORT_TIMESTAMP (0)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

uint32_t esp_log_early_timestamp(void);
char* esp_log_system_timestamp(void);
uint32_t esp_log_timestamp(void);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_LOG_TIMESTAMP_H__ */
#endif /* WINK_H_GUARD_ESP_LOG_TIMESTAMP_H */
