/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_SLEEP_INTERNAL_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_SLEEP_INTERNAL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_SLEEP_INTERNAL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_SLEEP_INTERNAL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "hal/uart_types.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_SLEEP_RTC_USE_RC_FAST_MODE = 0,
    ESP_SLEEP_DIG_USE_RC_FAST_MODE = 1,
    ESP_SLEEP_USE_ADC_TSEN_MONITOR_MODE = 2,
    ESP_SLEEP_ULTRA_LOW_MODE = 3,
    ESP_SLEEP_RTC_FAST_USE_XTAL_MODE = 4,
    ESP_SLEEP_DIG_USE_XTAL_MODE = 5,
    ESP_SLEEP_LP_USE_XTAL_MODE = 6,
    ESP_SLEEP_LP_USE_RC_FAST_MODE = 7,
    ESP_SLEEP_VBAT_POWER_DEEPSLEEP_MODE = 8,
    ESP_SLEEP_ANALOG_LOW_POWER_MODE = 9,
    ESP_SLEEP_MODE_MAX = 10,
} esp_sleep_sub_mode_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
esp_err_t esp_sleep_acquire_lp_use_xtal(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_acquire_lp_use_xtal out of Core 8 scope.");
#else
esp_err_t esp_sleep_acquire_lp_use_xtal(void);
#endif

#if defined(__WINK_SIM__)
void esp_sleep_isolate_digital_gpio(bool do_backup) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_isolate_digital_gpio out of Core 8 scope.");
#else
void esp_sleep_isolate_digital_gpio(bool do_backup);
#endif

#if defined(__WINK_SIM__)
void esp_sleep_isolate_mspi_gpio(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_isolate_mspi_gpio out of Core 8 scope.");
#else
void esp_sleep_isolate_mspi_gpio(void);
#endif

#if defined(__WINK_SIM__)
void esp_sleep_overhead_out_time_refresh(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_overhead_out_time_refresh out of Core 8 scope.");
#else
void esp_sleep_overhead_out_time_refresh(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_release_lp_use_xtal(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_release_lp_use_xtal out of Core 8 scope.");
#else
esp_err_t esp_sleep_release_lp_use_xtal(void);
#endif

#if defined(__WINK_SIM__)
void esp_sleep_restore_isolated_digital_gpio(void) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_restore_isolated_digital_gpio out of Core 8 scope.");
#else
void esp_sleep_restore_isolated_digital_gpio(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_sub_mode_config(esp_sleep_sub_mode_t mode, bool activate) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_sub_mode_config out of Core 8 scope.");
#else
esp_err_t esp_sleep_sub_mode_config(esp_sleep_sub_mode_t mode, bool activate);
#endif

#if defined(__WINK_SIM__)
int32_t* esp_sleep_sub_mode_dump_config(FILE *stream) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_sub_mode_dump_config out of Core 8 scope.");
#else
int32_t* esp_sleep_sub_mode_dump_config(FILE *stream);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_sub_mode_force_disable(esp_sleep_sub_mode_t mode) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_sub_mode_force_disable out of Core 8 scope.");
#else
esp_err_t esp_sleep_sub_mode_force_disable(esp_sleep_sub_mode_t mode);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_SLEEP_INTERNAL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_SLEEP_INTERNAL_H */
