/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef __ESP_CROSSCORE_INT_H
#define __ESP_CROSSCORE_INT_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_CROSSCORE_INT_H__
#define __WINK_HARVESTED_ESP_PRIVATE_CROSSCORE_INT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void esp_crosscore_int_init(void) WINK_SLA_ERROR("Wink SLA Violation: esp_crosscore_int_init out of Core 8 scope.");
#else
void esp_crosscore_int_init(void);
#endif

#if defined(__WINK_SIM__)
void esp_crosscore_int_send_freq_switch(int core_id) WINK_SLA_ERROR("Wink SLA Violation: esp_crosscore_int_send_freq_switch out of Core 8 scope.");
#else
void esp_crosscore_int_send_freq_switch(int core_id);
#endif

#if defined(__WINK_SIM__)
void esp_crosscore_int_send_gdb_call(int core_id) WINK_SLA_ERROR("Wink SLA Violation: esp_crosscore_int_send_gdb_call out of Core 8 scope.");
#else
void esp_crosscore_int_send_gdb_call(int core_id);
#endif

#if defined(__WINK_SIM__)
void esp_crosscore_int_send_print_backtrace(int core_id) WINK_SLA_ERROR("Wink SLA Violation: esp_crosscore_int_send_print_backtrace out of Core 8 scope.");
#else
void esp_crosscore_int_send_print_backtrace(int core_id);
#endif

#if defined(__WINK_SIM__)
void esp_crosscore_int_send_yield(int core_id) WINK_SLA_ERROR("Wink SLA Violation: esp_crosscore_int_send_yield out of Core 8 scope.");
#else
void esp_crosscore_int_send_yield(int core_id);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_CROSSCORE_INT_H__ */
#endif /* __ESP_CROSSCORE_INT_H */
