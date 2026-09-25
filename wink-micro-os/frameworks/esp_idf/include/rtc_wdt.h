/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_RTC_WDT_H
#define WINK_H_GUARD_RTC_WDT_H
#ifndef __WINK_HARVESTED_RTC_WDT_H__
#define __WINK_HARVESTED_RTC_WDT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    RTC_WDT_STAGE0 = 0,
    RTC_WDT_STAGE1 = 1,
    RTC_WDT_STAGE2 = 2,
    RTC_WDT_STAGE3 = 3,
} rtc_wdt_stage_t;
typedef enum {
    RTC_WDT_STAGE_ACTION_OFF = RTC_WDT_STG_SEL_OFF,
    RTC_WDT_STAGE_ACTION_INTERRUPT = RTC_WDT_STG_SEL_INT,
    RTC_WDT_STAGE_ACTION_RESET_CPU = RTC_WDT_STG_SEL_RESET_CPU,
    RTC_WDT_STAGE_ACTION_RESET_SYSTEM = RTC_WDT_STG_SEL_RESET_SYSTEM,
    RTC_WDT_STAGE_ACTION_RESET_RTC = RTC_WDT_STG_SEL_RESET_RTC,
} rtc_wdt_stage_action_t;
typedef enum {
    RTC_WDT_SYS_RESET_SIG = 0,
    RTC_WDT_CPU_RESET_SIG = 1,
} rtc_wdt_reset_sig_t;
typedef enum {
    RTC_WDT_LENGTH_100ns = 0,
    RTC_WDT_LENGTH_200ns = 1,
    RTC_WDT_LENGTH_300ns = 2,
    RTC_WDT_LENGTH_400ns = 3,
    RTC_WDT_LENGTH_500ns = 4,
    RTC_WDT_LENGTH_800ns = 5,
    RTC_WDT_LENGTH_1_6us = 6,
    RTC_WDT_LENGTH_3_2us = 7,
} rtc_wdt_length_sig_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void rtc_wdt_disable(void) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_disable out of Core 8 scope.");
#else
void rtc_wdt_disable(void);
#endif

#if defined(__WINK_SIM__)
void rtc_wdt_enable(void) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_enable out of Core 8 scope.");
#else
void rtc_wdt_enable(void);
#endif

#if defined(__WINK_SIM__)
void rtc_wdt_feed(void) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_feed out of Core 8 scope.");
#else
void rtc_wdt_feed(void);
#endif

#if defined(__WINK_SIM__)
void rtc_wdt_flashboot_mode_enable(void) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_flashboot_mode_enable out of Core 8 scope.");
#else
void rtc_wdt_flashboot_mode_enable(void);
#endif

#if defined(__WINK_SIM__)
bool rtc_wdt_get_protect_status(void) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_get_protect_status out of Core 8 scope.");
#else
bool rtc_wdt_get_protect_status(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t rtc_wdt_get_timeout(rtc_wdt_stage_t stage, unsigned int* timeout_ms) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_get_timeout out of Core 8 scope.");
#else
esp_err_t rtc_wdt_get_timeout(rtc_wdt_stage_t stage, unsigned int* timeout_ms);
#endif

#if defined(__WINK_SIM__)
bool rtc_wdt_is_on(void) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_is_on out of Core 8 scope.");
#else
bool rtc_wdt_is_on(void);
#endif

#if defined(__WINK_SIM__)
void rtc_wdt_protect_off(void) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_protect_off out of Core 8 scope.");
#else
void rtc_wdt_protect_off(void);
#endif

#if defined(__WINK_SIM__)
void rtc_wdt_protect_on(void) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_protect_on out of Core 8 scope.");
#else
void rtc_wdt_protect_on(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t rtc_wdt_set_length_of_reset_signal(rtc_wdt_reset_sig_t reset_src, rtc_wdt_length_sig_t reset_signal_length) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_set_length_of_reset_signal out of Core 8 scope.");
#else
esp_err_t rtc_wdt_set_length_of_reset_signal(rtc_wdt_reset_sig_t reset_src, rtc_wdt_length_sig_t reset_signal_length);
#endif

#if defined(__WINK_SIM__)
esp_err_t rtc_wdt_set_stage(rtc_wdt_stage_t stage, rtc_wdt_stage_action_t stage_sel) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_set_stage out of Core 8 scope.");
#else
esp_err_t rtc_wdt_set_stage(rtc_wdt_stage_t stage, rtc_wdt_stage_action_t stage_sel);
#endif

#if defined(__WINK_SIM__)
esp_err_t rtc_wdt_set_time(rtc_wdt_stage_t stage, unsigned int timeout_ms) WINK_SLA_ERROR("Wink SLA Violation: rtc_wdt_set_time out of Core 8 scope.");
#else
esp_err_t rtc_wdt_set_time(rtc_wdt_stage_t stage, unsigned int timeout_ms);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_RTC_WDT_H__ */
#endif /* WINK_H_GUARD_RTC_WDT_H */
