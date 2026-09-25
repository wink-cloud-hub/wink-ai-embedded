/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_UTILS_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_UTILS_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_UTILS_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_UTILS_H__
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
void esp_clk_utils_mspi_speed_mode_sync_after_cpu_freq_switching(uint32_t target_cpu_src_freq, uint32_t target_cpu_freq) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_utils_mspi_speed_mode_sync_after_cpu_freq_switching out of Core 8 scope.");
#else
void esp_clk_utils_mspi_speed_mode_sync_after_cpu_freq_switching(uint32_t target_cpu_src_freq, uint32_t target_cpu_freq);
#endif

#if defined(__WINK_SIM__)
void esp_clk_utils_mspi_speed_mode_sync_before_cpu_freq_switching(uint32_t target_cpu_src_freq, uint32_t target_cpu_freq) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_utils_mspi_speed_mode_sync_before_cpu_freq_switching out of Core 8 scope.");
#else
void esp_clk_utils_mspi_speed_mode_sync_before_cpu_freq_switching(uint32_t target_cpu_src_freq, uint32_t target_cpu_freq);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_UTILS_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_UTILS_H */
