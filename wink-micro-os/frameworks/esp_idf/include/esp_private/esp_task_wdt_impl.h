/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ESP_TASK_WDT_IMPL_H
#define WINK_H_GUARD_ESP_PRIVATE_ESP_TASK_WDT_IMPL_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ESP_TASK_WDT_IMPL_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ESP_TASK_WDT_IMPL_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */



#if defined(__WINK_SIM__)
void esp_task_wdt_impl_timeout_triggered(twdt_ctx_t obj) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_impl_timeout_triggered out of Core 8 scope.");
#else
void esp_task_wdt_impl_timeout_triggered(twdt_ctx_t obj);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_impl_timer_allocate(const esp_task_wdt_config_t *config,
                                           twdt_isr_callback callback,
                                           twdt_ctx_t *obj) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_impl_timer_allocate out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_impl_timer_allocate(const esp_task_wdt_config_t *config,
                                           twdt_isr_callback callback,
                                           twdt_ctx_t *obj);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_impl_timer_feed(twdt_ctx_t obj) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_impl_timer_feed out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_impl_timer_feed(twdt_ctx_t obj);
#endif

#if defined(__WINK_SIM__)
void esp_task_wdt_impl_timer_free(twdt_ctx_t obj) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_impl_timer_free out of Core 8 scope.");
#else
void esp_task_wdt_impl_timer_free(twdt_ctx_t obj);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_impl_timer_reconfigure(twdt_ctx_t obj, const esp_task_wdt_config_t *config) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_impl_timer_reconfigure out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_impl_timer_reconfigure(twdt_ctx_t obj, const esp_task_wdt_config_t *config);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_impl_timer_restart(twdt_ctx_t obj) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_impl_timer_restart out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_impl_timer_restart(twdt_ctx_t obj);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_impl_timer_stop(twdt_ctx_t obj) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_impl_timer_stop out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_impl_timer_stop(twdt_ctx_t obj);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ESP_TASK_WDT_IMPL_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ESP_TASK_WDT_IMPL_H */
