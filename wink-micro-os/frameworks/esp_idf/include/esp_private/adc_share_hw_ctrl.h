/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ADC_SHARE_HW_CTRL_H
#define WINK_H_GUARD_ESP_PRIVATE_ADC_SHARE_HW_CTRL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ADC_SHARE_HW_CTRL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ADC_SHARE_HW_CTRL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_err.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void adc2_cal_include(void) WINK_SLA_ERROR("Wink SLA Violation: adc2_cal_include out of Core 8 scope.");
#else
void adc2_cal_include(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t adc2_wifi_acquire(void) WINK_SLA_ERROR("Wink SLA Violation: adc2_wifi_acquire out of Core 8 scope.");
#else
esp_err_t adc2_wifi_acquire(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t adc2_wifi_release(void) WINK_SLA_ERROR("Wink SLA Violation: adc2_wifi_release out of Core 8 scope.");
#else
esp_err_t adc2_wifi_release(void);
#endif

#if defined(__WINK_SIM__)
void adc_apb_periph_claim(void) WINK_SLA_ERROR("Wink SLA Violation: adc_apb_periph_claim out of Core 8 scope.");
#else
void adc_apb_periph_claim(void);
#endif

#if defined(__WINK_SIM__)
void adc_apb_periph_free(void) WINK_SLA_ERROR("Wink SLA Violation: adc_apb_periph_free out of Core 8 scope.");
#else
void adc_apb_periph_free(void);
#endif

#if defined(__WINK_SIM__)
void adc_calc_hw_calibration_code(adc_unit_t adc_n, adc_atten_t atten) WINK_SLA_ERROR("Wink SLA Violation: adc_calc_hw_calibration_code out of Core 8 scope.");
#else
void adc_calc_hw_calibration_code(adc_unit_t adc_n, adc_atten_t atten);
#endif

#if defined(__WINK_SIM__)
int adc_get_hw_calibration_chan_compens(adc_unit_t adc_n, adc_channel_t chan, adc_atten_t atten) WINK_SLA_ERROR("Wink SLA Violation: adc_get_hw_calibration_chan_compens out of Core 8 scope.");
#else
int adc_get_hw_calibration_chan_compens(adc_unit_t adc_n, adc_channel_t chan, adc_atten_t atten);
#endif

#if defined(__WINK_SIM__)
void adc_load_hw_calibration_chan_compens(adc_unit_t adc_n, adc_channel_t chan, adc_atten_t atten) WINK_SLA_ERROR("Wink SLA Violation: adc_load_hw_calibration_chan_compens out of Core 8 scope.");
#else
void adc_load_hw_calibration_chan_compens(adc_unit_t adc_n, adc_channel_t chan, adc_atten_t atten);
#endif

#if defined(__WINK_SIM__)
esp_err_t adc_lock_acquire(adc_unit_t adc_unit) WINK_SLA_ERROR("Wink SLA Violation: adc_lock_acquire out of Core 8 scope.");
#else
esp_err_t adc_lock_acquire(adc_unit_t adc_unit);
#endif

#if defined(__WINK_SIM__)
esp_err_t adc_lock_release(adc_unit_t adc_unit) WINK_SLA_ERROR("Wink SLA Violation: adc_lock_release out of Core 8 scope.");
#else
esp_err_t adc_lock_release(adc_unit_t adc_unit);
#endif

#if defined(__WINK_SIM__)
esp_err_t adc_lock_try_acquire(adc_unit_t adc_unit) WINK_SLA_ERROR("Wink SLA Violation: adc_lock_try_acquire out of Core 8 scope.");
#else
esp_err_t adc_lock_try_acquire(adc_unit_t adc_unit);
#endif

#if defined(__WINK_SIM__)
void adc_set_hw_calibration_code(adc_unit_t adc_n, adc_atten_t atten) WINK_SLA_ERROR("Wink SLA Violation: adc_set_hw_calibration_code out of Core 8 scope.");
#else
void adc_set_hw_calibration_code(adc_unit_t adc_n, adc_atten_t atten);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ADC_SHARE_HW_CTRL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ADC_SHARE_HW_CTRL_H */
