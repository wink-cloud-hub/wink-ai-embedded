/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_RTC_IO_LL_H
#define WINK_H_GUARD_HAL_RTC_IO_LL_H
#ifndef __WINK_HARVESTED_HAL_RTC_IO_LL_H__
#define __WINK_HARVESTED_HAL_RTC_IO_LL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdlib.h>

#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef RTCIO_LL_PIN_FUNC
#define RTCIO_LL_PIN_FUNC 0
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    RTCIO_LL_FUNC_RTC = 0x0,
    RTCIO_LL_FUNC_DIGITAL = 0x1,
} rtcio_ll_func_t;
typedef enum {
    RTCIO_LL_OUTPUT_NORMAL = 0,
    RTCIO_LL_OUTPUT_OD = 0x1,
} rtcio_ll_out_mode_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void rtcio_ll_clear_interrupt_status(void) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_clear_interrupt_status out of Core 8 scope.");
#else
void rtcio_ll_clear_interrupt_status(void);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_disable_input_in_sleep(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_disable_input_in_sleep out of Core 8 scope.");
#else
void rtcio_ll_disable_input_in_sleep(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_disable_output_in_sleep(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_disable_output_in_sleep out of Core 8 scope.");
#else
void rtcio_ll_disable_output_in_sleep(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_disable_sleep_setting(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_disable_sleep_setting out of Core 8 scope.");
#else
void rtcio_ll_disable_sleep_setting(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_enable_input_in_sleep(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_enable_input_in_sleep out of Core 8 scope.");
#else
void rtcio_ll_enable_input_in_sleep(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_enable_output_in_sleep(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_enable_output_in_sleep out of Core 8 scope.");
#else
void rtcio_ll_enable_output_in_sleep(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_enable_sleep_setting(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_enable_sleep_setting out of Core 8 scope.");
#else
void rtcio_ll_enable_sleep_setting(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_ext0_set_wakeup_pin(int rtcio_num, int level) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_ext0_set_wakeup_pin out of Core 8 scope.");
#else
void rtcio_ll_ext0_set_wakeup_pin(int rtcio_num, int level);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_force_hold_all(void) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_force_hold_all out of Core 8 scope.");
#else
void rtcio_ll_force_hold_all(void);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_force_hold_disable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_force_hold_disable out of Core 8 scope.");
#else
void rtcio_ll_force_hold_disable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_force_hold_enable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_force_hold_enable out of Core 8 scope.");
#else
void rtcio_ll_force_hold_enable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_force_unhold_all(void) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_force_unhold_all out of Core 8 scope.");
#else
void rtcio_ll_force_unhold_all(void);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_function_select(int rtcio_num, rtcio_ll_func_t func) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_function_select out of Core 8 scope.");
#else
void rtcio_ll_function_select(int rtcio_num, rtcio_ll_func_t func);
#endif

#if defined(__WINK_SIM__)
uint32_t rtcio_ll_get_drive_capability(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_get_drive_capability out of Core 8 scope.");
#else
uint32_t rtcio_ll_get_drive_capability(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
uint32_t rtcio_ll_get_interrupt_status(void) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_get_interrupt_status out of Core 8 scope.");
#else
uint32_t rtcio_ll_get_interrupt_status(void);
#endif

#if defined(__WINK_SIM__)
uint32_t rtcio_ll_get_level(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_get_level out of Core 8 scope.");
#else
uint32_t rtcio_ll_get_level(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_input_disable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_input_disable out of Core 8 scope.");
#else
void rtcio_ll_input_disable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_input_enable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_input_enable out of Core 8 scope.");
#else
void rtcio_ll_input_enable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_intr_enable(int rtcio_num, gpio_int_type_t type) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_intr_enable out of Core 8 scope.");
#else
void rtcio_ll_intr_enable(int rtcio_num, gpio_int_type_t type);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_iomux_func_sel(int rtcio_num, int func) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_iomux_func_sel out of Core 8 scope.");
#else
void rtcio_ll_iomux_func_sel(int rtcio_num, int func);
#endif

#if defined(__WINK_SIM__)
bool rtcio_ll_is_pulldown_enabled(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_is_pulldown_enabled out of Core 8 scope.");
#else
bool rtcio_ll_is_pulldown_enabled(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
bool rtcio_ll_is_pullup_enabled(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_is_pullup_enabled out of Core 8 scope.");
#else
bool rtcio_ll_is_pullup_enabled(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_output_disable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_output_disable out of Core 8 scope.");
#else
void rtcio_ll_output_disable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_output_enable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_output_enable out of Core 8 scope.");
#else
void rtcio_ll_output_enable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_output_mode_set(int rtcio_num, rtcio_ll_out_mode_t mode) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_output_mode_set out of Core 8 scope.");
#else
void rtcio_ll_output_mode_set(int rtcio_num, rtcio_ll_out_mode_t mode);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_pulldown_disable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_pulldown_disable out of Core 8 scope.");
#else
void rtcio_ll_pulldown_disable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_pulldown_enable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_pulldown_enable out of Core 8 scope.");
#else
void rtcio_ll_pulldown_enable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_pullup_disable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_pullup_disable out of Core 8 scope.");
#else
void rtcio_ll_pullup_disable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_pullup_enable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_pullup_enable out of Core 8 scope.");
#else
void rtcio_ll_pullup_enable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_set_drive_capability(int rtcio_num, uint32_t strength) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_set_drive_capability out of Core 8 scope.");
#else
void rtcio_ll_set_drive_capability(int rtcio_num, uint32_t strength);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_set_level(int rtcio_num, uint32_t level) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_set_level out of Core 8 scope.");
#else
void rtcio_ll_set_level(int rtcio_num, uint32_t level);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_wakeup_disable(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_wakeup_disable out of Core 8 scope.");
#else
void rtcio_ll_wakeup_disable(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_ll_wakeup_enable(int rtcio_num, gpio_int_type_t type) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_wakeup_enable out of Core 8 scope.");
#else
void rtcio_ll_wakeup_enable(int rtcio_num, gpio_int_type_t type);
#endif

#if defined(__WINK_SIM__)
bool rtcio_ll_wakeup_is_enabled(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_ll_wakeup_is_enabled out of Core 8 scope.");
#else
bool rtcio_ll_wakeup_is_enabled(int rtcio_num);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_RTC_IO_LL_H__ */
#endif /* WINK_H_GUARD_HAL_RTC_IO_LL_H */
