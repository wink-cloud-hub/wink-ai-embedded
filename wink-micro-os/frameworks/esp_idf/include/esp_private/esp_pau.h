/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_PAU_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_PAU_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_PAU_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_PAU_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
bool pau_regdma_enable_aon_link_entry(bool enable) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_enable_aon_link_entry out of Core 8 scope.");
#else
bool pau_regdma_enable_aon_link_entry(bool enable);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_set_entry_link_addr(pau_regdma_link_addr_t *link_entries) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_set_entry_link_addr out of Core 8 scope.");
#else
void pau_regdma_set_entry_link_addr(pau_regdma_link_addr_t *link_entries);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_set_extra_link_addr(void *link_addr) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_set_extra_link_addr out of Core 8 scope.");
#else
void pau_regdma_set_extra_link_addr(void *link_addr);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_set_modem_link_addr(void *link_addr) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_set_modem_link_addr out of Core 8 scope.");
#else
void pau_regdma_set_modem_link_addr(void *link_addr);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_set_system_link_addr(void *link_addr) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_set_system_link_addr out of Core 8 scope.");
#else
void pau_regdma_set_system_link_addr(void *link_addr);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_trigger_extra_link_backup(void) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_trigger_extra_link_backup out of Core 8 scope.");
#else
void pau_regdma_trigger_extra_link_backup(void);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_trigger_extra_link_restore(void) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_trigger_extra_link_restore out of Core 8 scope.");
#else
void pau_regdma_trigger_extra_link_restore(void);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_trigger_modem_link_backup(void) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_trigger_modem_link_backup out of Core 8 scope.");
#else
void pau_regdma_trigger_modem_link_backup(void);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_trigger_modem_link_restore(void) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_trigger_modem_link_restore out of Core 8 scope.");
#else
void pau_regdma_trigger_modem_link_restore(void);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_trigger_system_link_backup(void) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_trigger_system_link_backup out of Core 8 scope.");
#else
void pau_regdma_trigger_system_link_backup(void);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_trigger_system_link_restore(void) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_trigger_system_link_restore out of Core 8 scope.");
#else
void pau_regdma_trigger_system_link_restore(void);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_trigger_wifimac_link_backup(void) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_trigger_wifimac_link_backup out of Core 8 scope.");
#else
void pau_regdma_trigger_wifimac_link_backup(void);
#endif

#if defined(__WINK_SIM__)
void pau_regdma_trigger_wifimac_link_restore(void) WINK_SLA_ERROR("Wink SLA Violation: pau_regdma_trigger_wifimac_link_restore out of Core 8 scope.");
#else
void pau_regdma_trigger_wifimac_link_restore(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_PAU_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_PAU_H */
