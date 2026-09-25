/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_GPIO_ETM_H
#define WINK_H_GUARD_DRIVER_GPIO_ETM_H
#ifndef __WINK_HARVESTED_DRIVER_GPIO_ETM_H__
#define __WINK_HARVESTED_DRIVER_GPIO_ETM_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdint.h>

#include "esp_err.h"
#include "esp_etm.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */
#ifndef GPIO_ETM_EVENT_EDGE_TYPES
#define GPIO_ETM_EVENT_EDGE_TYPES 3
#endif
#ifndef GPIO_ETM_TASK_ACTION_TYPES
#define GPIO_ETM_TASK_ACTION_TYPES 3
#endif

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    GPIO_ETM_EVENT_EDGE_POS = 1,
    GPIO_ETM_EVENT_EDGE_NEG = 2,
    GPIO_ETM_EVENT_EDGE_ANY = 3,
} gpio_etm_event_edge_t;
typedef enum {
    GPIO_ETM_TASK_ACTION_SET = 1,
    GPIO_ETM_TASK_ACTION_CLR = 2,
    GPIO_ETM_TASK_ACTION_TOG = 3,
} gpio_etm_task_action_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct {
union {
        gpio_etm_event_edge_t edge;                                 
        gpio_etm_event_edge_t edges[GPIO_ETM_EVENT_EDGE_TYPES];     
    };
} gpio_etm_event_config_t;
typedef struct {
union {
        gpio_etm_task_action_t action;                                  
        gpio_etm_task_action_t actions[GPIO_ETM_TASK_ACTION_TYPES];     
    };
} gpio_etm_task_config_t;

esp_err_t gpio_etm_event_bind_gpio(esp_etm_event_handle_t event, int gpio_num);
esp_err_t gpio_etm_task_add_gpio(esp_etm_task_handle_t task, int gpio_num);
esp_err_t gpio_etm_task_rm_gpio(esp_etm_task_handle_t task, int gpio_num);
esp_err_t gpio_new_etm_event(const gpio_etm_event_config_t *config, esp_etm_event_handle_t *ret_event, ...);
esp_err_t gpio_new_etm_task(const gpio_etm_task_config_t *config, esp_etm_task_handle_t *ret_task, ...);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_GPIO_ETM_H__ */
#endif /* WINK_H_GUARD_DRIVER_GPIO_ETM_H */
