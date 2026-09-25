/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef __ESP_FREERTOS_HOOKS_H__
#define __ESP_FREERTOS_HOOKS_H__
#ifndef __WINK_HARVESTED_ESP_FREERTOS_HOOKS_H__
#define __WINK_HARVESTED_ESP_FREERTOS_HOOKS_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "esp_err.h"
#include "freertos/portmacro.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef bool (*esp_freertos_idle_cb_t)(void);
typedef void (*esp_freertos_tick_cb_t)(void);



#if defined(__WINK_SIM__)
void esp_deregister_freertos_idle_hook(esp_freertos_idle_cb_t old_idle_cb) WINK_SLA_ERROR("Wink SLA Violation: esp_deregister_freertos_idle_hook out of Core 8 scope.");
#else
void esp_deregister_freertos_idle_hook(esp_freertos_idle_cb_t old_idle_cb);
#endif

#if defined(__WINK_SIM__)
void esp_deregister_freertos_idle_hook_for_cpu(esp_freertos_idle_cb_t old_idle_cb, UBaseType_t cpuid) WINK_SLA_ERROR("Wink SLA Violation: esp_deregister_freertos_idle_hook_for_cpu out of Core 8 scope.");
#else
void esp_deregister_freertos_idle_hook_for_cpu(esp_freertos_idle_cb_t old_idle_cb, UBaseType_t cpuid);
#endif

#if defined(__WINK_SIM__)
void esp_deregister_freertos_tick_hook(esp_freertos_tick_cb_t old_tick_cb) WINK_SLA_ERROR("Wink SLA Violation: esp_deregister_freertos_tick_hook out of Core 8 scope.");
#else
void esp_deregister_freertos_tick_hook(esp_freertos_tick_cb_t old_tick_cb);
#endif

#if defined(__WINK_SIM__)
void esp_deregister_freertos_tick_hook_for_cpu(esp_freertos_tick_cb_t old_tick_cb, UBaseType_t cpuid) WINK_SLA_ERROR("Wink SLA Violation: esp_deregister_freertos_tick_hook_for_cpu out of Core 8 scope.");
#else
void esp_deregister_freertos_tick_hook_for_cpu(esp_freertos_tick_cb_t old_tick_cb, UBaseType_t cpuid);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_register_freertos_idle_hook(esp_freertos_idle_cb_t new_idle_cb) WINK_SLA_ERROR("Wink SLA Violation: esp_register_freertos_idle_hook out of Core 8 scope.");
#else
esp_err_t esp_register_freertos_idle_hook(esp_freertos_idle_cb_t new_idle_cb);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_register_freertos_idle_hook_for_cpu(esp_freertos_idle_cb_t new_idle_cb, UBaseType_t cpuid) WINK_SLA_ERROR("Wink SLA Violation: esp_register_freertos_idle_hook_for_cpu out of Core 8 scope.");
#else
esp_err_t esp_register_freertos_idle_hook_for_cpu(esp_freertos_idle_cb_t new_idle_cb, UBaseType_t cpuid);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_register_freertos_tick_hook(esp_freertos_tick_cb_t new_tick_cb) WINK_SLA_ERROR("Wink SLA Violation: esp_register_freertos_tick_hook out of Core 8 scope.");
#else
esp_err_t esp_register_freertos_tick_hook(esp_freertos_tick_cb_t new_tick_cb);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_register_freertos_tick_hook_for_cpu(esp_freertos_tick_cb_t new_tick_cb, UBaseType_t cpuid) WINK_SLA_ERROR("Wink SLA Violation: esp_register_freertos_tick_hook_for_cpu out of Core 8 scope.");
#else
esp_err_t esp_register_freertos_tick_hook_for_cpu(esp_freertos_tick_cb_t new_tick_cb, UBaseType_t cpuid);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_FREERTOS_HOOKS_H__ */
#endif /* __ESP_FREERTOS_HOOKS_H__ */
