/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
int esp_clk_apb_freq(void) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_apb_freq out of Core 8 scope.");
#else
int esp_clk_apb_freq(void);
#endif

#if defined(__WINK_SIM__)
int esp_clk_cpu_freq(void) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_cpu_freq out of Core 8 scope.");
#else
int esp_clk_cpu_freq(void);
#endif

#if defined(__WINK_SIM__)
void esp_clk_private_lock(void) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_private_lock out of Core 8 scope.");
#else
void esp_clk_private_lock(void);
#endif

#if defined(__WINK_SIM__)
void esp_clk_private_unlock(void) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_private_unlock out of Core 8 scope.");
#else
void esp_clk_private_unlock(void);
#endif

#if defined(__WINK_SIM__)
uint64_t esp_clk_rtc_time(void) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_rtc_time out of Core 8 scope.");
#else
uint64_t esp_clk_rtc_time(void);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_clk_slowclk_cal_get(void) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_slowclk_cal_get out of Core 8 scope.");
#else
uint32_t esp_clk_slowclk_cal_get(void);
#endif

#if defined(__WINK_SIM__)
void esp_clk_slowclk_cal_set(uint32_t value) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_slowclk_cal_set out of Core 8 scope.");
#else
void esp_clk_slowclk_cal_set(uint32_t value);
#endif

#if defined(__WINK_SIM__)
int esp_clk_xtal_freq(void) WINK_SLA_ERROR("Wink SLA Violation: esp_clk_xtal_freq out of Core 8 scope.");
#else
int esp_clk_xtal_freq(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_CLK_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_CLK_H */
