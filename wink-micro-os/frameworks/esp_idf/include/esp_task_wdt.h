/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_TASK_WDT_H
#define ESP_TASK_WDT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t timeout_ms;
    uint32_t idle_core_mask;
    bool trigger_panic;
} esp_task_wdt_config_t;

esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *config);
esp_err_t esp_task_wdt_deinit(void);
esp_err_t esp_task_wdt_add(TaskHandle_t handle);
esp_err_t esp_task_wdt_reset(void);
esp_err_t esp_task_wdt_delete(TaskHandle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* ESP_TASK_WDT_H */
