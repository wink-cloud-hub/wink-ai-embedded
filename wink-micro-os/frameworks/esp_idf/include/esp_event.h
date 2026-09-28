/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_event_base.h"
#include "freertos/FreeRTOS.h"

#ifndef WINK_ESP_EVENT_QUEUE_CAPACITY
#  define WINK_ESP_EVENT_QUEUE_CAPACITY 32
#endif

#ifndef WINK_ESP_EVENT_MAX_PAYLOAD
#  define WINK_ESP_EVENT_MAX_PAYLOAD 1024
#endif

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_loop_delete_default(void);

esp_err_t esp_event_handler_register(esp_event_base_t event_base,
                                     int32_t event_id,
                                     esp_event_handler_t event_handler,
                                     void* event_handler_arg);

esp_err_t esp_event_handler_unregister(esp_event_base_t event_base,
                                       int32_t event_id,
                                       esp_event_handler_t event_handler);

esp_err_t esp_event_handler_instance_register(esp_event_base_t event_base,
                                              int32_t event_id,
                                              esp_event_handler_t event_handler,
                                              void* event_handler_arg,
                                              esp_event_handler_instance_t* instance);

esp_err_t esp_event_handler_instance_unregister(esp_event_base_t event_base,
                                                int32_t event_id,
                                                esp_event_handler_instance_t instance);

esp_err_t esp_event_post(esp_event_base_t event_base,
                         int32_t event_id,
                         void* event_data,
                         size_t event_data_size,
                         TickType_t ticks_to_wait);

/* Wink simulation helpers */
void esp_event_loop_sim_reset(void);
esp_err_t esp_event_loop_run_step(void);
int esp_event_loop_run_all_pending(void);

#ifdef __cplusplus
}
#endif
