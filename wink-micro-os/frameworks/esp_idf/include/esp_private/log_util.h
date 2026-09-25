/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_LOG_UTIL_H
#define WINK_H_GUARD_ESP_PRIVATE_LOG_UTIL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_LOG_UTIL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_LOG_UTIL_H__
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
typedef bool (*esp_log_cache_enabled_t)(void);

int esp_log_util_cvt(unsigned long long val, long radix, int pad, const char *digits, char *buf);
int esp_log_util_cvt_dec(unsigned long long val, int pad, char *buf);
int esp_log_util_cvt_hex(unsigned long long val, int pad, char *buf);
bool esp_log_util_is_constrained(void);
void esp_log_util_set_cache_enabled_cb(esp_log_cache_enabled_t func);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_LOG_UTIL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_LOG_UTIL_H */
