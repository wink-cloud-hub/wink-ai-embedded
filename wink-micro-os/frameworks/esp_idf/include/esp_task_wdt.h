/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_TASK_WDT_H
#define WINK_H_GUARD_ESP_TASK_WDT_H
#ifndef __WINK_HARVESTED_ESP_TASK_WDT_H__
#define __WINK_HARVESTED_ESP_TASK_WDT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
    uint32_t timeout_ms;
    uint32_t idle_core_mask;
    bool trigger_panic;
} esp_task_wdt_config_t;
typedef struct esp_task_wdt_user_handle_s * esp_task_wdt_user_handle_t;
typedef void (*task_wdt_msg_handler)(void *opaque, const char *msg);



#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_add(TaskHandle_t task_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_add out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_add(TaskHandle_t task_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_add_user(const char *user_name, esp_task_wdt_user_handle_t *user_handle_ret) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_add_user out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_add_user(const char *user_name, esp_task_wdt_user_handle_t *user_handle_ret);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_deinit(void) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_deinit out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_deinit(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_delete(TaskHandle_t task_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_delete out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_delete(TaskHandle_t task_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_delete_user(esp_task_wdt_user_handle_t user_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_delete_user out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_delete_user(esp_task_wdt_user_handle_t user_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *config) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_init out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *config);
#endif

#if defined(__WINK_SIM__)
void esp_task_wdt_isr_user_handler(void) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_isr_user_handler out of Core 8 scope.");
#else
void esp_task_wdt_isr_user_handler(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_print_triggered_tasks(task_wdt_msg_handler msg_handler, void *opaque, int *cpus_fail) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_print_triggered_tasks out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_print_triggered_tasks(task_wdt_msg_handler msg_handler, void *opaque, int *cpus_fail);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_reconfigure out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_reset(void) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_reset out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_reset(void);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_reset_user(esp_task_wdt_user_handle_t user_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_reset_user out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_reset_user(esp_task_wdt_user_handle_t user_handle);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_task_wdt_status(TaskHandle_t task_handle) WINK_SLA_ERROR("Wink SLA Violation: esp_task_wdt_status out of Core 8 scope.");
#else
esp_err_t esp_task_wdt_status(TaskHandle_t task_handle);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_TASK_WDT_H__ */
#endif /* WINK_H_GUARD_ESP_TASK_WDT_H */
