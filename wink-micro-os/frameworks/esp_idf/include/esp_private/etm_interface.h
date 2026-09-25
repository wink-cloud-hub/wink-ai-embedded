/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_ESP_PRIVATE_ETM_INTERFACE_H
#define WINK_H_GUARD_ESP_PRIVATE_ETM_INTERFACE_H
#ifndef __WINK_HARVESTED_ESP_PRIVATE_ETM_INTERFACE_H__
#define __WINK_HARVESTED_ESP_PRIVATE_ETM_INTERFACE_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */
typedef enum {
    ETM_TRIG_PERIPH_GPIO = 0,
    ETM_TRIG_PERIPH_GDMA = 1,
    ETM_TRIG_PERIPH_GPTIMER = 2,
    ETM_TRIG_PERIPH_SYSTIMER = 3,
    ETM_TRIG_PERIPH_MCPWM = 4,
    ETM_TRIG_PERIPH_ANA_CMPR = 5,
    ETM_TRIG_PERIPH_TSENS = 6,
    ETM_TRIG_PERIPH_I2S = 7,
    ETM_TRIG_PERIPH_LP_CORE = 8,
    ETM_TRIG_PERIPH_MODEM = 9,
    ETM_TRIG_PERIPH_LEDC = 10,
} etm_trigger_peripheral_t;

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct esp_etm_event_t esp_etm_event_t;
typedef struct esp_etm_task_t esp_etm_task_t;
struct esp_etm_event_t {
    uint32_t event_id;
    etm_trigger_peripheral_t trig_periph;
    esp_err_t (*del)(esp_etm_event_t *event);
};
struct esp_etm_task_t {
    uint32_t task_id;
    etm_trigger_peripheral_t trig_periph;
    esp_err_t (*del)(esp_etm_task_t *task);
};



#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_ESP_PRIVATE_ETM_INTERFACE_H__ */
#endif /* WINK_H_GUARD_ESP_PRIVATE_ETM_INTERFACE_H */
