/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_RTC_IO_H
#define WINK_H_GUARD_DRIVER_RTC_IO_H
#ifndef __WINK_HARVESTED_DRIVER_RTC_IO_H__
#define __WINK_HARVESTED_DRIVER_RTC_IO_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "hal/gpio_types.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef RTC_GPIO_IS_VALID_GPIO
#define RTC_GPIO_IS_VALID_GPIO(gpio_num) rtc_gpio_is_valid_gpio(gpio_num)
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */

esp_err_t rtc_gpio_deinit(gpio_num_t gpio_num);
esp_err_t rtc_gpio_force_hold_dis_all(void);
esp_err_t rtc_gpio_force_hold_en_all(void);
esp_err_t rtc_gpio_get_drive_capability(gpio_num_t gpio_num, gpio_drive_cap_t* strength);
uint32_t rtc_gpio_get_level(gpio_num_t gpio_num);
esp_err_t rtc_gpio_hold_dis(gpio_num_t gpio_num);
esp_err_t rtc_gpio_hold_en(gpio_num_t gpio_num);
esp_err_t rtc_gpio_init(gpio_num_t gpio_num);
esp_err_t rtc_gpio_iomux_func_sel(gpio_num_t gpio_num, int func);
esp_err_t rtc_gpio_iomux_input(gpio_num_t gpio_num, int func, uint32_t signal_idx);
esp_err_t rtc_gpio_iomux_output(gpio_num_t gpio_num, int func);
bool rtc_gpio_is_valid_gpio(gpio_num_t gpio_num);
esp_err_t rtc_gpio_isolate(gpio_num_t gpio_num);
esp_err_t rtc_gpio_pulldown_dis(gpio_num_t gpio_num);
esp_err_t rtc_gpio_pulldown_en(gpio_num_t gpio_num);
esp_err_t rtc_gpio_pullup_dis(gpio_num_t gpio_num);
esp_err_t rtc_gpio_pullup_en(gpio_num_t gpio_num);
esp_err_t rtc_gpio_set_direction(gpio_num_t gpio_num, rtc_gpio_mode_t mode);
esp_err_t rtc_gpio_set_direction_in_sleep(gpio_num_t gpio_num, rtc_gpio_mode_t mode);
esp_err_t rtc_gpio_set_drive_capability(gpio_num_t gpio_num, gpio_drive_cap_t strength);
esp_err_t rtc_gpio_set_level(gpio_num_t gpio_num, uint32_t level);
esp_err_t rtc_gpio_wakeup_disable(gpio_num_t gpio_num);
esp_err_t rtc_gpio_wakeup_enable(gpio_num_t gpio_num, gpio_int_type_t intr_type);


#if defined(__WINK_SIM__)
int rtc_io_number_get(gpio_num_t gpio_num) WINK_SLA_ERROR("Wink SLA Violation: rtc_io_number_get out of Core 8 scope.");
#else
int rtc_io_number_get(gpio_num_t gpio_num);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_RTC_IO_H__ */
#endif /* WINK_H_GUARD_DRIVER_RTC_IO_H */
