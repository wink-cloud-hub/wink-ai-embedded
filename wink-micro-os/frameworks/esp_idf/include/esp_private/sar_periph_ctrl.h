/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SAR_PERIPH_CTRL_H
#define WINK_H_GUARD_ESP_PRIVATE_SAR_PERIPH_CTRL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SAR_PERIPH_CTRL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SAR_PERIPH_CTRL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void adc_reset_lock_acquire(void) WINK_SLA_ERROR("Wink SLA Violation: adc_reset_lock_acquire out of Core 8 scope.");
#else
void adc_reset_lock_acquire(void);
#endif

#if defined(__WINK_SIM__)
void adc_reset_lock_release(void) WINK_SLA_ERROR("Wink SLA Violation: adc_reset_lock_release out of Core 8 scope.");
#else
void adc_reset_lock_release(void);
#endif

#if defined(__WINK_SIM__)
void regi2c_saradc_disable(void) WINK_SLA_ERROR("Wink SLA Violation: regi2c_saradc_disable out of Core 8 scope.");
#else
void regi2c_saradc_disable(void);
#endif

#if defined(__WINK_SIM__)
void regi2c_saradc_enable(void) WINK_SLA_ERROR("Wink SLA Violation: regi2c_saradc_enable out of Core 8 scope.");
#else
void regi2c_saradc_enable(void);
#endif

#if defined(__WINK_SIM__)
void sar_periph_ctrl_adc_continuous_power_acquire(void) WINK_SLA_ERROR("Wink SLA Violation: sar_periph_ctrl_adc_continuous_power_acquire out of Core 8 scope.");
#else
void sar_periph_ctrl_adc_continuous_power_acquire(void);
#endif

#if defined(__WINK_SIM__)
void sar_periph_ctrl_adc_continuous_power_release(void) WINK_SLA_ERROR("Wink SLA Violation: sar_periph_ctrl_adc_continuous_power_release out of Core 8 scope.");
#else
void sar_periph_ctrl_adc_continuous_power_release(void);
#endif

#if defined(__WINK_SIM__)
void sar_periph_ctrl_adc_oneshot_power_acquire(void) WINK_SLA_ERROR("Wink SLA Violation: sar_periph_ctrl_adc_oneshot_power_acquire out of Core 8 scope.");
#else
void sar_periph_ctrl_adc_oneshot_power_acquire(void);
#endif

#if defined(__WINK_SIM__)
void sar_periph_ctrl_adc_oneshot_power_release(void) WINK_SLA_ERROR("Wink SLA Violation: sar_periph_ctrl_adc_oneshot_power_release out of Core 8 scope.");
#else
void sar_periph_ctrl_adc_oneshot_power_release(void);
#endif

#if defined(__WINK_SIM__)
void sar_periph_ctrl_adc_reset(void) WINK_SLA_ERROR("Wink SLA Violation: sar_periph_ctrl_adc_reset out of Core 8 scope.");
#else
void sar_periph_ctrl_adc_reset(void);
#endif

#if defined(__WINK_SIM__)
void sar_periph_ctrl_init(void) WINK_SLA_ERROR("Wink SLA Violation: sar_periph_ctrl_init out of Core 8 scope.");
#else
void sar_periph_ctrl_init(void);
#endif

#if defined(__WINK_SIM__)
void sar_periph_ctrl_power_disable(void) WINK_SLA_ERROR("Wink SLA Violation: sar_periph_ctrl_power_disable out of Core 8 scope.");
#else
void sar_periph_ctrl_power_disable(void);
#endif

#if defined(__WINK_SIM__)
void sar_periph_ctrl_power_enable(void) WINK_SLA_ERROR("Wink SLA Violation: sar_periph_ctrl_power_enable out of Core 8 scope.");
#else
void sar_periph_ctrl_power_enable(void);
#endif

#if defined(__WINK_SIM__)
void sar_periph_ctrl_pwdet_power_acquire(void) WINK_SLA_ERROR("Wink SLA Violation: sar_periph_ctrl_pwdet_power_acquire out of Core 8 scope.");
#else
void sar_periph_ctrl_pwdet_power_acquire(void);
#endif

#if defined(__WINK_SIM__)
void sar_periph_ctrl_pwdet_power_release(void) WINK_SLA_ERROR("Wink SLA Violation: sar_periph_ctrl_pwdet_power_release out of Core 8 scope.");
#else
void sar_periph_ctrl_pwdet_power_release(void);
#endif

#if defined(__WINK_SIM__)
int16_t temp_sensor_get_raw_value(bool *range_changed) WINK_SLA_ERROR("Wink SLA Violation: temp_sensor_get_raw_value out of Core 8 scope.");
#else
int16_t temp_sensor_get_raw_value(bool *range_changed);
#endif

#if defined(__WINK_SIM__)
void temperature_sensor_power_acquire(void) WINK_SLA_ERROR("Wink SLA Violation: temperature_sensor_power_acquire out of Core 8 scope.");
#else
void temperature_sensor_power_acquire(void);
#endif

#if defined(__WINK_SIM__)
void temperature_sensor_power_release(void) WINK_SLA_ERROR("Wink SLA Violation: temperature_sensor_power_release out of Core 8 scope.");
#else
void temperature_sensor_power_release(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SAR_PERIPH_CTRL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SAR_PERIPH_CTRL_H */
