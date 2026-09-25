/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_LOG_FORMAT_H
#define WINK_H_GUARD_ESP_LOG_FORMAT_H
#ifndef __WINK_HARVESTED_ESP_LOG_FORMAT_H__
#define __WINK_HARVESTED_ESP_LOG_FORMAT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef LOG_FORMAT
#define LOG_FORMAT(letter, format) LOG_COLOR_ ## letter #letter " (%" PRIu32 ") %s: " format LOG_RESET_COLOR "\n"
#endif
#ifndef LOG_SYSTEM_TIME_FORMAT
#define LOG_SYSTEM_TIME_FORMAT(letter, format) LOG_COLOR_ ## letter #letter " (%s) %s: " format LOG_RESET_COLOR "\n"
#endif
#ifndef _ESP_LOG_DRAM_LOG_FORMAT
#define _ESP_LOG_DRAM_LOG_FORMAT(letter, format) DRAM_STR(#letter " %s: " format "\n")
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_LOG_FORMAT_H__ */
#endif /* WINK_H_GUARD_ESP_LOG_FORMAT_H */
