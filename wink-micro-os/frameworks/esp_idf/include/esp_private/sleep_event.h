/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_SLEEP_EVENT_H
#define WINK_H_GUARD_ESP_PRIVATE_SLEEP_EVENT_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_SLEEP_EVENT_H__
#define __WINK_HARVESTED_ESP_PRIVATE_SLEEP_EVENT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    SLEEP_EVENT_HW_EXIT_SLEEP = 0,
    SLEEP_EVENT_SW_CLK_READY = 1,
    SLEEP_EVENT_SW_EXIT_SLEEP = 2,
    SLEEP_EVENT_SW_GOTO_SLEEP = 3,
    SLEEP_EVENT_HW_TIME_START = 4,
    SLEEP_EVENT_HW_GOTO_SLEEP = 5,
    SLEEP_EVENT_SW_CPU_TO_MEM_START = 6,
    SLEEP_EVENT_SW_CPU_TO_MEM_END = 7,
    SLEEP_EVENT_HW_PLL_EN_START = 8,
    SLEEP_EVENT_HW_PLL_EN_STOP = 9,
    SLEEP_EVENT_CB_INDEX_NUM = 10,
} esp_sleep_event_cb_index_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef esp_err_t (*esp_sleep_event_cb_t)(void *user_arg, void *ext_arg);
struct _esp_sleep_event_cb_config_t {
    esp_sleep_event_cb_t cb;
    void * user_arg;
    uint32_t prior;
    struct _esp_sleep_event_cb_config_t * next;
};
typedef struct _esp_sleep_event_cb_config_t esp_sleep_event_cb_config_t;
struct _esp_sleep_event_cbs_config_t {
    esp_sleep_event_cb_config_t * sleep_event_cb_config[SLEEP_EVENT_CB_INDEX_NUM];
};
typedef struct _esp_sleep_event_cbs_config_t esp_sleep_event_cbs_config_t;



#if defined(__WINK_SIM__)
void esp_sleep_execute_event_callbacks(esp_sleep_event_cb_index_t event_id, void *ext_arg) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_execute_event_callbacks out of Core 8 scope.");
#else
void esp_sleep_execute_event_callbacks(esp_sleep_event_cb_index_t event_id, void *ext_arg);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_register_event_callback(esp_sleep_event_cb_index_t event_id, const esp_sleep_event_cb_config_t *event_cb_conf) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_register_event_callback out of Core 8 scope.");
#else
esp_err_t esp_sleep_register_event_callback(esp_sleep_event_cb_index_t event_id, const esp_sleep_event_cb_config_t *event_cb_conf);
#endif

#if defined(__WINK_SIM__)
esp_err_t esp_sleep_unregister_event_callback(esp_sleep_event_cb_index_t event_id, esp_sleep_event_cb_t cb) WINK_SLA_ERROR("Wink SLA Violation: esp_sleep_unregister_event_callback out of Core 8 scope.");
#else
esp_err_t esp_sleep_unregister_event_callback(esp_sleep_event_cb_index_t event_id, esp_sleep_event_cb_t cb);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_SLEEP_EVENT_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_SLEEP_EVENT_H */
