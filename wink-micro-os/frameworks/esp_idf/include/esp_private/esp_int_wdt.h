/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_INT_WDT_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_INT_WDT_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_INT_WDT_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_INT_WDT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef IWDT_INITIAL_TIMEOUT_S
#define IWDT_INITIAL_TIMEOUT_S (5)
#endif
#ifndef IWDT_INITIAL_TIMEOUT_US
#define IWDT_INITIAL_TIMEOUT_US (IWDT_INITIAL_TIMEOUT_S * 1000000 / IWDT_TICKS_PER_US)
#endif
#ifndef IWDT_STAGE0_TIMEOUT_US
#define IWDT_STAGE0_TIMEOUT_US (CONFIG_ESP_INT_WDT_TIMEOUT_MS * 1000 / IWDT_TICKS_PER_US)
#endif
#ifndef IWDT_STAGE1_TIMEOUT_US
#define IWDT_STAGE1_TIMEOUT_US (2 * IWDT_STAGE0_TIMEOUT_US)
#endif
#ifndef IWDT_TICKS_PER_US
#define IWDT_TICKS_PER_US (500)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void esp_int_wdt_cpu_init(void) WINK_SLA_ERROR("Wink SLA Violation: esp_int_wdt_cpu_init out of Core 8 scope.");
#else
void esp_int_wdt_cpu_init(void);
#endif

#if defined(__WINK_SIM__)
void esp_int_wdt_init(void) WINK_SLA_ERROR("Wink SLA Violation: esp_int_wdt_init out of Core 8 scope.");
#else
void esp_int_wdt_init(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_INT_WDT_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_INT_WDT_H */
