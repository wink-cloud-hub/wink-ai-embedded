/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_HAL_RTC_IO_HAL_H
#define WINK_H_GUARD_HAL_RTC_IO_HAL_H
#ifndef __WINK_HARVESTED_HAL_RTC_IO_HAL_H__
#define __WINK_HARVESTED_HAL_RTC_IO_HAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <esp_err.h>

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef gpio_hal_clear_hp_periph_pd_sleep_edge_wakeup_latch
#define gpio_hal_clear_hp_periph_pd_sleep_edge_wakeup_latch(hal, gpio_num) rtcio_ll_clear_edge_wakeup_latch(rtc_io_num_map[gpio_num])
#endif
#ifndef gpio_hal_wakeup_disable_on_hp_periph_powerdown_sleep
#define gpio_hal_wakeup_disable_on_hp_periph_powerdown_sleep(hal, gpio_num) rtcio_hal_wakeup_disable(rtc_io_num_map[gpio_num])
#endif
#ifndef gpio_hal_wakeup_enable_on_hp_periph_powerdown_sleep
#define gpio_hal_wakeup_enable_on_hp_periph_powerdown_sleep(hal, gpio_num, intr_type) rtcio_hal_wakeup_enable(rtc_io_num_map[gpio_num], intr_type)
#endif
#ifndef gpio_hal_wakeup_is_enabled_on_hp_periph_powerdown_sleep
#define gpio_hal_wakeup_is_enabled_on_hp_periph_powerdown_sleep(hal, gpio_num) rtcio_hal_wakeup_is_enabled(rtc_io_num_map[gpio_num])
#endif
#ifndef rtc_hal_gpio_clear_wakeup_status
#define rtc_hal_gpio_clear_wakeup_status() rtcio_hal_clear_interrupt_status()
#endif
#ifndef rtc_hal_gpio_get_wakeup_status
#define rtc_hal_gpio_get_wakeup_status() rtcio_hal_get_interrupt_status()
#endif
#ifndef rtcio_hal_clear_interrupt_status
#define rtcio_hal_clear_interrupt_status() rtcio_ll_clear_interrupt_status()
#endif
#ifndef rtcio_hal_enable_io_clock
#define rtcio_hal_enable_io_clock(enable) rtcio_ll_enable_io_clock(enable)
#endif
#ifndef rtcio_hal_ext0_set_wakeup_pin
#define rtcio_hal_ext0_set_wakeup_pin(rtcio_num, level) rtcio_ll_ext0_set_wakeup_pin(rtcio_num, level)
#endif
#ifndef rtcio_hal_function_select
#define rtcio_hal_function_select(rtcio_num, func) rtcio_ll_function_select(rtcio_num, func)
#endif
#ifndef rtcio_hal_get_drive_capability
#define rtcio_hal_get_drive_capability(rtcio_num) rtcio_ll_get_drive_capability(rtcio_num)
#endif
#ifndef rtcio_hal_get_interrupt_status
#define rtcio_hal_get_interrupt_status() rtcio_ll_get_interrupt_status()
#endif
#ifndef rtcio_hal_get_level
#define rtcio_hal_get_level(rtcio_num) rtcio_ll_get_level(rtcio_num)
#endif
#ifndef rtcio_hal_hold_all
#define rtcio_hal_hold_all() rtcio_ll_force_hold_all()
#endif
#ifndef rtcio_hal_hold_disable
#define rtcio_hal_hold_disable(rtcio_num) rtcio_ll_force_hold_disable(rtcio_num)
#endif
#ifndef rtcio_hal_hold_enable
#define rtcio_hal_hold_enable(rtcio_num) rtcio_ll_force_hold_enable(rtcio_num)
#endif
#ifndef rtcio_hal_input_disable
#define rtcio_hal_input_disable(rtcio_num) rtcio_ll_input_disable(rtcio_num)
#endif
#ifndef rtcio_hal_input_enable
#define rtcio_hal_input_enable(rtcio_num) rtcio_ll_input_enable(rtcio_num)
#endif
#ifndef rtcio_hal_iomux_func_sel
#define rtcio_hal_iomux_func_sel(rtcio_num, func) rtcio_ll_iomux_func_sel(rtcio_num, func)
#endif
#ifndef rtcio_hal_is_pulldown_enabled
#define rtcio_hal_is_pulldown_enabled(rtcio_num) rtcio_ll_is_pulldown_enabled(rtcio_num)
#endif
#ifndef rtcio_hal_is_pullup_enabled
#define rtcio_hal_is_pullup_enabled(rtcio_num) rtcio_ll_is_pullup_enabled(rtcio_num)
#endif
#ifndef rtcio_hal_output_disable
#define rtcio_hal_output_disable(rtcio_num) rtcio_ll_output_disable(rtcio_num)
#endif
#ifndef rtcio_hal_output_enable
#define rtcio_hal_output_enable(rtcio_num) rtcio_ll_output_enable(rtcio_num)
#endif
#ifndef rtcio_hal_pulldown_disable
#define rtcio_hal_pulldown_disable(rtcio_num) rtcio_ll_pulldown_disable(rtcio_num)
#endif
#ifndef rtcio_hal_pulldown_enable
#define rtcio_hal_pulldown_enable(rtcio_num) rtcio_ll_pulldown_enable(rtcio_num)
#endif
#ifndef rtcio_hal_pullup_disable
#define rtcio_hal_pullup_disable(rtcio_num) rtcio_ll_pullup_disable(rtcio_num)
#endif
#ifndef rtcio_hal_pullup_enable
#define rtcio_hal_pullup_enable(rtcio_num) rtcio_ll_pullup_enable(rtcio_num)
#endif
#ifndef rtcio_hal_set_drive_capability
#define rtcio_hal_set_drive_capability(rtcio_num, strength) rtcio_ll_set_drive_capability(rtcio_num, strength)
#endif
#ifndef rtcio_hal_set_level
#define rtcio_hal_set_level(rtcio_num, level) rtcio_ll_set_level(rtcio_num, level)
#endif
#ifndef rtcio_hal_unhold_all
#define rtcio_hal_unhold_all() rtcio_ll_force_unhold_all()
#endif
#ifndef rtcio_hal_wakeup_disable
#define rtcio_hal_wakeup_disable(rtcio_num) rtcio_ll_wakeup_disable(rtcio_num)
#endif
#ifndef rtcio_hal_wakeup_enable
#define rtcio_hal_wakeup_enable(rtcio_num, type) rtcio_ll_wakeup_enable(rtcio_num, type)
#endif
#ifndef rtcio_hal_wakeup_is_enabled
#define rtcio_hal_wakeup_is_enabled(rtcio_num) rtcio_ll_wakeup_is_enabled(rtcio_num)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void rtcio_hal_iomux_input(int rtcio_num, int func, uint32_t signal_idx) WINK_SLA_ERROR("Wink SLA Violation: rtcio_hal_iomux_input out of Core 8 scope.");
#else
void rtcio_hal_iomux_input(int rtcio_num, int func, uint32_t signal_idx);
#endif

#if defined(__WINK_SIM__)
void rtcio_hal_iomux_output(int rtcio_num, int func) WINK_SLA_ERROR("Wink SLA Violation: rtcio_hal_iomux_output out of Core 8 scope.");
#else
void rtcio_hal_iomux_output(int rtcio_num, int func);
#endif

#if defined(__WINK_SIM__)
void rtcio_hal_isolate(int rtcio_num) WINK_SLA_ERROR("Wink SLA Violation: rtcio_hal_isolate out of Core 8 scope.");
#else
void rtcio_hal_isolate(int rtcio_num);
#endif

#if defined(__WINK_SIM__)
void rtcio_hal_matrix_in(int rtcio_num, uint32_t signal_idx, bool inv) WINK_SLA_ERROR("Wink SLA Violation: rtcio_hal_matrix_in out of Core 8 scope.");
#else
void rtcio_hal_matrix_in(int rtcio_num, uint32_t signal_idx, bool inv);
#endif

#if defined(__WINK_SIM__)
void rtcio_hal_matrix_out(int rtcio_num, uint32_t signal_idx, bool out_inv, bool oen_inv) WINK_SLA_ERROR("Wink SLA Violation: rtcio_hal_matrix_out out of Core 8 scope.");
#else
void rtcio_hal_matrix_out(int rtcio_num, uint32_t signal_idx, bool out_inv, bool oen_inv);
#endif

#if defined(__WINK_SIM__)
void rtcio_hal_set_direction(int rtcio_num, rtc_gpio_mode_t mode) WINK_SLA_ERROR("Wink SLA Violation: rtcio_hal_set_direction out of Core 8 scope.");
#else
void rtcio_hal_set_direction(int rtcio_num, rtc_gpio_mode_t mode);
#endif

#if defined(__WINK_SIM__)
void rtcio_hal_set_direction_in_sleep(int rtcio_num, rtc_gpio_mode_t mode) WINK_SLA_ERROR("Wink SLA Violation: rtcio_hal_set_direction_in_sleep out of Core 8 scope.");
#else
void rtcio_hal_set_direction_in_sleep(int rtcio_num, rtc_gpio_mode_t mode);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_HAL_RTC_IO_HAL_H__ */
#endif /* WINK_H_GUARD_HAL_RTC_IO_HAL_H */
