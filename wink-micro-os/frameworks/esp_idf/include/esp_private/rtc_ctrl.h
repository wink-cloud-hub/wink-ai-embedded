/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_RTC_CTRL_H
#define WINK_H_GUARD_ESP_PRIVATE_RTC_CTRL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_RTC_CTRL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_RTC_CTRL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_err.h"
#include "esp_intr_alloc.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef RTC_INTR_FLAG_IRAM
#define RTC_INTR_FLAG_IRAM (BIT(0))
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
esp_err_t rtc_isr_deregister(intr_handler_t handler, void* handler_arg) WINK_SLA_ERROR("Wink SLA Violation: rtc_isr_deregister out of Core 8 scope.");
#else
esp_err_t rtc_isr_deregister(intr_handler_t handler, void* handler_arg);
#endif

#if defined(__WINK_SIM__)
void rtc_isr_noniram_disable(uint32_t cpu) WINK_SLA_ERROR("Wink SLA Violation: rtc_isr_noniram_disable out of Core 8 scope.");
#else
void rtc_isr_noniram_disable(uint32_t cpu);
#endif

#if defined(__WINK_SIM__)
void rtc_isr_noniram_enable(uint32_t cpu) WINK_SLA_ERROR("Wink SLA Violation: rtc_isr_noniram_enable out of Core 8 scope.");
#else
void rtc_isr_noniram_enable(uint32_t cpu);
#endif

#if defined(__WINK_SIM__)
esp_err_t rtc_isr_register(intr_handler_t handler, void* handler_arg,
                            uint32_t rtc_intr_mask, uint32_t flags) WINK_SLA_ERROR("Wink SLA Violation: rtc_isr_register out of Core 8 scope.");
#else
esp_err_t rtc_isr_register(intr_handler_t handler, void* handler_arg,
                            uint32_t rtc_intr_mask, uint32_t flags);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_RTC_CTRL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_RTC_CTRL_H */
