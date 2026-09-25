/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SLEEP_CACHE_H
#define WINK_H_GUARD_ESP_PRIVATE_SLEEP_CACHE_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SLEEP_CACHE_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SLEEP_CACHE_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "sdkconfig.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void sleep_cache_resume(void) WINK_SLA_ERROR("Wink SLA Violation: sleep_cache_resume out of Core 8 scope.");
#else
void sleep_cache_resume(void);
#endif

#if defined(__WINK_SIM__)
void sleep_cache_suspend(void) WINK_SLA_ERROR("Wink SLA Violation: sleep_cache_suspend out of Core 8 scope.");
#else
void sleep_cache_suspend(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SLEEP_CACHE_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SLEEP_CACHE_H */
