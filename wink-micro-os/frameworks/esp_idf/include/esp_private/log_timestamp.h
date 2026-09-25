/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_LOG_TIMESTAMP_H
#define WINK_H_GUARD_ESP_PRIVATE_LOG_TIMESTAMP_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_LOG_TIMESTAMP_H__
#define __WINK_HARVESTED_ESP_PRIVATE_LOG_TIMESTAMP_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

uint64_t esp_log_timestamp64(bool constrained_env);
char* esp_log_timestamp_str(bool constrained_env, uint64_t timestamp_ms, char* buffer);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_LOG_TIMESTAMP_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_LOG_TIMESTAMP_H */
