/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_MEMPROT_INTERNAL_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_MEMPROT_INTERNAL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_MEMPROT_INTERNAL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_MEMPROT_INTERNAL_H__
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
esp_err_t esp_mprot_get_default_main_split_addr(const esp_mprot_mem_t mem_type, void **def_split_addr) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_get_default_main_split_addr out of Core 8 scope.");
#else
esp_err_t esp_mprot_get_default_main_split_addr(const esp_mprot_mem_t mem_type, void **def_split_addr);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_get_monitor_en(const esp_mprot_mem_t mem_type, bool* enabled, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_get_monitor_en out of Core 8 scope.");
#else
esp_err_t esp_mprot_get_monitor_en(const esp_mprot_mem_t mem_type, bool* enabled, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_get_monitor_lock(const esp_mprot_mem_t mem_type, bool *locked, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_get_monitor_lock out of Core 8 scope.");
#else
esp_err_t esp_mprot_get_monitor_lock(const esp_mprot_mem_t mem_type, bool *locked, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_get_pms_area(const esp_mprot_pms_area_t area_type, uint32_t *flags, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_get_pms_area out of Core 8 scope.");
#else
esp_err_t esp_mprot_get_pms_area(const esp_mprot_pms_area_t area_type, uint32_t *flags, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_get_pms_lock(const esp_mprot_mem_t mem_type, bool *locked, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_get_pms_lock out of Core 8 scope.");
#else
esp_err_t esp_mprot_get_pms_lock(const esp_mprot_mem_t mem_type, bool *locked, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_get_split_addr(const esp_mprot_mem_t mem_type, const esp_mprot_split_addr_t line_type, void **line_addr, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_get_split_addr out of Core 8 scope.");
#else
esp_err_t esp_mprot_get_split_addr(const esp_mprot_mem_t mem_type, const esp_mprot_split_addr_t line_type, void **line_addr, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_get_split_addr_lock(const esp_mprot_mem_t mem_type, bool *locked, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_get_split_addr_lock out of Core 8 scope.");
#else
esp_err_t esp_mprot_get_split_addr_lock(const esp_mprot_mem_t mem_type, bool *locked, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_ll_err_to_esp_err(const memprot_hal_err_t err) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_ll_err_to_esp_err out of Core 8 scope.");
#else
esp_err_t esp_mprot_ll_err_to_esp_err(const memprot_hal_err_t err);
#endif

#if defined(__WINK_SIM__)
esp_mprot_pms_world_t esp_mprot_ll_world_to_hl_world(const memprot_hal_world_t world) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_ll_world_to_hl_world out of Core 8 scope.");
#else
esp_mprot_pms_world_t esp_mprot_ll_world_to_hl_world(const memprot_hal_world_t world);
#endif

#if defined(__WINK_SIM__)
const char * esp_mprot_oper_type_to_str(const uint32_t oper_type) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_oper_type_to_str out of Core 8 scope.");
#else
const char * esp_mprot_oper_type_to_str(const uint32_t oper_type);
#endif

#if defined(__WINK_SIM__)
const char * esp_mprot_pms_world_to_str(const esp_mprot_pms_world_t world_type) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_pms_world_to_str out of Core 8 scope.");
#else
const char * esp_mprot_pms_world_to_str(const esp_mprot_pms_world_t world_type);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_set_monitor_en(const esp_mprot_mem_t mem_type, const bool enable, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_set_monitor_en out of Core 8 scope.");
#else
esp_err_t esp_mprot_set_monitor_en(const esp_mprot_mem_t mem_type, const bool enable, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_set_monitor_lock(const esp_mprot_mem_t mem_type, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_set_monitor_lock out of Core 8 scope.");
#else
esp_err_t esp_mprot_set_monitor_lock(const esp_mprot_mem_t mem_type, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_set_pms_area(const esp_mprot_pms_area_t area_type, const uint32_t flags, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_set_pms_area out of Core 8 scope.");
#else
esp_err_t esp_mprot_set_pms_area(const esp_mprot_pms_area_t area_type, const uint32_t flags, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_set_pms_lock(const esp_mprot_mem_t mem_type, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_set_pms_lock out of Core 8 scope.");
#else
esp_err_t esp_mprot_set_pms_lock(const esp_mprot_mem_t mem_type, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_set_split_addr(const esp_mprot_mem_t mem_type, const esp_mprot_split_addr_t line_type, const void *line_addr, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_set_split_addr out of Core 8 scope.");
#else
esp_err_t esp_mprot_set_split_addr(const esp_mprot_mem_t mem_type, const esp_mprot_split_addr_t line_type, const void *line_addr, const int core);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_mprot_set_split_addr_lock(const esp_mprot_mem_t mem_type, const int core) WINK_SLA_ERROR("Wink SLA Violation: esp_mprot_set_split_addr_lock out of Core 8 scope.");
#else
esp_err_t esp_mprot_set_split_addr_lock(const esp_mprot_mem_t mem_type, const int core);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_MEMPROT_INTERNAL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_MEMPROT_INTERNAL_H */
