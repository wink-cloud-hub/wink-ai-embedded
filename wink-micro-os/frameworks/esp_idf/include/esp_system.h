/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef __ESP_SYSTEM_H__
#define __ESP_SYSTEM_H__
#ifndef __WINK_HARVESTED_ESP_SYSTEM_H__
#define __WINK_HARVESTED_ESP_SYSTEM_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_attr.h"
#include "esp_bit_defs.h"
#include "esp_err.h"
#include "esp_idf_version.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ESP_RST_UNKNOWN = 0,
    ESP_RST_POWERON = 1,
    ESP_RST_EXT = 2,
    ESP_RST_SW = 3,
    ESP_RST_PANIC = 4,
    ESP_RST_INT_WDT = 5,
    ESP_RST_TASK_WDT = 6,
    ESP_RST_WDT = 7,
    ESP_RST_DEEPSLEEP = 8,
    ESP_RST_BROWNOUT = 9,
    ESP_RST_SDIO = 10,
    ESP_RST_USB = 11,
    ESP_RST_JTAG = 12,
    ESP_RST_EFUSE = 13,
    ESP_RST_PWR_GLITCH = 14,
    ESP_RST_CPU_LOCKUP = 15,
} esp_reset_reason_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef void (*shutdown_handler_t)(void);

uint32_t esp_get_free_heap_size(void);
esp_reset_reason_t esp_reset_reason(void);
void esp_restart(void);


#if defined(__WINK_SIM__)
uint32_t esp_get_free_internal_heap_size(void) WINK_SLA_ERROR("Wink SLA Violation: esp_get_free_internal_heap_size out of Core 8 scope.");
#else
uint32_t esp_get_free_internal_heap_size(void);
#endif

#if defined(__WINK_SIM__)
uint32_t esp_get_minimum_free_heap_size(void) WINK_SLA_ERROR("Wink SLA Violation: esp_get_minimum_free_heap_size out of Core 8 scope.");
#else
uint32_t esp_get_minimum_free_heap_size(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_register_shutdown_handler(shutdown_handler_t handle) WINK_SLA_ERROR("Wink SLA Violation: esp_register_shutdown_handler out of Core 8 scope.");
#else
esp_err_t esp_register_shutdown_handler(shutdown_handler_t handle);
#endif

#if defined(__WINK_SIM__)
void esp_system_abort(const char* details) WINK_SLA_ERROR("Wink SLA Violation: esp_system_abort out of Core 8 scope.");
#else
void esp_system_abort(const char* details);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_unregister_shutdown_handler(shutdown_handler_t handle) WINK_SLA_ERROR("Wink SLA Violation: esp_unregister_shutdown_handler out of Core 8 scope.");
#else
esp_err_t esp_unregister_shutdown_handler(shutdown_handler_t handle);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_SYSTEM_H__ */
#endif /* __ESP_SYSTEM_H__ */
