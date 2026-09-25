/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SYSTEM_INTERNAL_H
#define WINK_H_GUARD_ESP_PRIVATE_SYSTEM_INTERNAL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SYSTEM_INTERNAL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SYSTEM_INTERNAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

esp_reset_reason_t esp_reset_reason_get_hint(void);
void esp_reset_reason_set_hint(esp_reset_reason_t hint);
void esp_restart_noos(void);
void esp_restart_noos_dig(void);


#if defined(__WINK_SIM__)
int64_t esp_system_get_time(void) WINK_SLA_ERROR("Wink SLA Violation: esp_system_get_time out of Core 8 scope.");
#else
int64_t esp_system_get_time(void);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_system_get_time_resolution(void) WINK_SLA_ERROR("Wink SLA Violation: esp_system_get_time_resolution out of Core 8 scope.");
#else
uint32_t esp_system_get_time_resolution(void);
#endif

#if defined(__WINK_SIM__)
void esp_system_reset_modules_on_exit(void) WINK_SLA_ERROR("Wink SLA Violation: esp_system_reset_modules_on_exit out of Core 8 scope.");
#else
void esp_system_reset_modules_on_exit(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SYSTEM_INTERNAL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SYSTEM_INTERNAL_H */
