/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef FP_UNWIND_H
#define FP_UNWIND_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_FP_UNWIND_H__
#define __WINK_HARVESTED_ESP_PRIVATE_FP_UNWIND_H__
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



#if defined(__WINK_SIM__)
void esp_fp_print_backtrace(const void *frame_or) WINK_SLA_ERROR("Wink SLA Violation: esp_fp_print_backtrace out of Core 8 scope.");
#else
void esp_fp_print_backtrace(const void *frame_or);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_FP_UNWIND_H__ */
#endif /* FP_UNWIND_H */
