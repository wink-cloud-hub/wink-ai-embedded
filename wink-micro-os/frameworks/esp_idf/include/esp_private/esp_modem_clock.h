/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_MODEM_CLOCK_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_MODEM_CLOCK_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_MODEM_CLOCK_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_MODEM_CLOCK_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void modem_clock_configure_wifi_status(bool inited) WINK_SLA_ERROR("Wink SLA Violation: modem_clock_configure_wifi_status out of Core 8 scope.");
#else
void modem_clock_configure_wifi_status(bool inited);
#endif

#if defined(__WINK_SIM__)
void modem_clock_deselect_all_module_lp_clock_source(void) WINK_SLA_ERROR("Wink SLA Violation: modem_clock_deselect_all_module_lp_clock_source out of Core 8 scope.");
#else
void modem_clock_deselect_all_module_lp_clock_source(void);
#endif

#if defined(__WINK_SIM__)
void modem_clock_deselect_lp_clock_source(shared_periph_module_t module) WINK_SLA_ERROR("Wink SLA Violation: modem_clock_deselect_lp_clock_source out of Core 8 scope.");
#else
void modem_clock_deselect_lp_clock_source(shared_periph_module_t module);
#endif

#if defined(__WINK_SIM__)
uint32_t modem_clock_module_bits_get(shared_periph_module_t module) WINK_SLA_ERROR("Wink SLA Violation: modem_clock_module_bits_get out of Core 8 scope.");
#else
uint32_t modem_clock_module_bits_get(shared_periph_module_t module);
#endif

#if defined(__WINK_SIM__)
void modem_clock_module_disable(shared_periph_module_t module) WINK_SLA_ERROR("Wink SLA Violation: modem_clock_module_disable out of Core 8 scope.");
#else
void modem_clock_module_disable(shared_periph_module_t module);
#endif

#if defined(__WINK_SIM__)
void modem_clock_module_enable(shared_periph_module_t module) WINK_SLA_ERROR("Wink SLA Violation: modem_clock_module_enable out of Core 8 scope.");
#else
void modem_clock_module_enable(shared_periph_module_t module);
#endif

#if defined(__WINK_SIM__)
void modem_clock_module_mac_reset(shared_periph_module_t module) WINK_SLA_ERROR("Wink SLA Violation: modem_clock_module_mac_reset out of Core 8 scope.");
#else
void modem_clock_module_mac_reset(shared_periph_module_t module);
#endif

#if defined(__WINK_SIM__)
void modem_clock_select_lp_clock_source(shared_periph_module_t module, modem_clock_lpclk_src_t src, uint32_t divider) WINK_SLA_ERROR("Wink SLA Violation: modem_clock_select_lp_clock_source out of Core 8 scope.");
#else
void modem_clock_select_lp_clock_source(shared_periph_module_t module, modem_clock_lpclk_src_t src, uint32_t divider);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_MODEM_CLOCK_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_MODEM_CLOCK_H */
