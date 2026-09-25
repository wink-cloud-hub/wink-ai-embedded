/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_SYSTICK_ETM_H
#define WINK_H_GUARD_ESP_SYSTICK_ETM_H
#ifndef __WINK_HARVESTED_ESP_SYSTICK_ETM_H__
#define __WINK_HARVESTED_ESP_SYSTICK_ETM_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_err.h"
#include "esp_etm.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
esp_err_t esp_systick_new_etm_alarm_event(int core_id, esp_etm_event_handle_t *out_event) WINK_SLA_ERROR("Wink SLA Violation: esp_systick_new_etm_alarm_event out of Core 8 scope.");
#else
esp_err_t esp_systick_new_etm_alarm_event(int core_id, esp_etm_event_handle_t *out_event);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_SYSTICK_ETM_H__ */
#endif /* WINK_H_GUARD_ESP_SYSTICK_ETM_H */
