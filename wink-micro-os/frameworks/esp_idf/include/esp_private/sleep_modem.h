/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SLEEP_MODEM_H
#define WINK_H_GUARD_ESP_PRIVATE_SLEEP_MODEM_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SLEEP_MODEM_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SLEEP_MODEM_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
esp_err_t esp_pm_register_inform_out_light_sleep_overhead_callback(inform_out_light_sleep_overhead_cb_t cb) WINK_SLA_ERROR("Wink SLA Violation: esp_pm_register_inform_out_light_sleep_overhead_callback out of Core 8 scope.");
#else
esp_err_t esp_pm_register_inform_out_light_sleep_overhead_callback(inform_out_light_sleep_overhead_cb_t cb);
#endif

#if defined(__WINK_SIM__)
void esp_pm_register_light_sleep_default_params_config_callback(update_light_sleep_default_params_config_cb_t cb) WINK_SLA_ERROR("Wink SLA Violation: esp_pm_register_light_sleep_default_params_config_callback out of Core 8 scope.");
#else
void esp_pm_register_light_sleep_default_params_config_callback(update_light_sleep_default_params_config_cb_t cb);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_pm_unregister_inform_out_light_sleep_overhead_callback(inform_out_light_sleep_overhead_cb_t cb) WINK_SLA_ERROR("Wink SLA Violation: esp_pm_unregister_inform_out_light_sleep_overhead_callback out of Core 8 scope.");
#else
esp_err_t esp_pm_unregister_inform_out_light_sleep_overhead_callback(inform_out_light_sleep_overhead_cb_t cb);
#endif

#if defined(__WINK_SIM__)
void esp_pm_unregister_light_sleep_default_params_config_callback(void) WINK_SLA_ERROR("Wink SLA Violation: esp_pm_unregister_light_sleep_default_params_config_callback out of Core 8 scope.");
#else
void esp_pm_unregister_light_sleep_default_params_config_callback(void);
#endif

#if defined(__WINK_SIM__)
bool modem_domain_pd_allowed(void) WINK_SLA_ERROR("Wink SLA Violation: modem_domain_pd_allowed out of Core 8 scope.");
#else
bool modem_domain_pd_allowed(void);
#endif

#if defined(__WINK_SIM__)
void periph_inform_out_light_sleep_overhead(uint32_t out_light_sleep_time) WINK_SLA_ERROR("Wink SLA Violation: periph_inform_out_light_sleep_overhead out of Core 8 scope.");
#else
void periph_inform_out_light_sleep_overhead(uint32_t out_light_sleep_time);
#endif

#if defined(__WINK_SIM__)
esp_err_t sleep_modem_configure(int max_freq_mhz, int min_freq_mhz, bool light_sleep_enable) WINK_SLA_ERROR("Wink SLA Violation: sleep_modem_configure out of Core 8 scope.");
#else
esp_err_t sleep_modem_configure(int max_freq_mhz, int min_freq_mhz, bool light_sleep_enable);
#endif

#if defined(__WINK_SIM__)
uint32_t sleep_modem_reject_triggers(void) WINK_SLA_ERROR("Wink SLA Violation: sleep_modem_reject_triggers out of Core 8 scope.");
#else
uint32_t sleep_modem_reject_triggers(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SLEEP_MODEM_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SLEEP_MODEM_H */
