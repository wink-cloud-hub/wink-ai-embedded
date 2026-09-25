/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_CLK_CTRL_OS_H
#define WINK_H_GUARD_CLK_CTRL_OS_H
#ifndef __WINK_HARVESTED_CLK_CTRL_OS_H__
#define __WINK_HARVESTED_CLK_CTRL_OS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "esp_err.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void periph_rtc_apll_acquire(void) WINK_SLA_ERROR("Wink SLA Violation: periph_rtc_apll_acquire out of Core 8 scope.");
#else
void periph_rtc_apll_acquire(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t periph_rtc_apll_freq_set(uint32_t expt_freq_hz, uint32_t *real_freq_hz) WINK_SLA_ERROR("Wink SLA Violation: periph_rtc_apll_freq_set out of Core 8 scope.");
#else
esp_err_t periph_rtc_apll_freq_set(uint32_t expt_freq_hz, uint32_t *real_freq_hz);
#endif

#if defined(__WINK_SIM__)
void periph_rtc_apll_release(void) WINK_SLA_ERROR("Wink SLA Violation: periph_rtc_apll_release out of Core 8 scope.");
#else
void periph_rtc_apll_release(void);
#endif

#if defined(__WINK_SIM__)
void periph_rtc_dig_clk8m_disable(void) WINK_SLA_ERROR("Wink SLA Violation: periph_rtc_dig_clk8m_disable out of Core 8 scope.");
#else
void periph_rtc_dig_clk8m_disable(void);
#endif

#if defined(__WINK_SIM__)
bool periph_rtc_dig_clk8m_enable(void) WINK_SLA_ERROR("Wink SLA Violation: periph_rtc_dig_clk8m_enable out of Core 8 scope.");
#else
bool periph_rtc_dig_clk8m_enable(void);
#endif

#if defined(__WINK_SIM__)
uint32_t periph_rtc_dig_clk8m_get_freq(void) WINK_SLA_ERROR("Wink SLA Violation: periph_rtc_dig_clk8m_get_freq out of Core 8 scope.");
#else
uint32_t periph_rtc_dig_clk8m_get_freq(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_CLK_CTRL_OS_H__ */
#endif /* WINK_H_GUARD_CLK_CTRL_OS_H */
