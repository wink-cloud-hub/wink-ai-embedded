/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef ESP_TIMER_H
#define ESP_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_timer* esp_timer_handle_t;
typedef void (*esp_timer_cb_t)(void* arg);

typedef enum {
    ESP_TIMER_TASK,
    ESP_TIMER_ISR,
    ESP_TIMER_MAX,
} esp_timer_dispatch_t;

typedef struct {
    esp_timer_cb_t callback;
    void *arg;
    esp_timer_dispatch_t dispatch_method;
    const char *name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

#include <stdio.h>

int64_t esp_timer_get_time(void);

esp_err_t esp_timer_create(const esp_timer_create_args_t* create_args, esp_timer_handle_t* out_handle);
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_start_once_at(esp_timer_handle_t timer, uint64_t alarm_us);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us);
esp_err_t esp_timer_start_periodic_at(esp_timer_handle_t timer, uint64_t period_us, uint64_t first_alarm_us);
esp_err_t esp_timer_restart(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_restart_at(esp_timer_handle_t timer, uint64_t period_us, uint64_t first_alarm_us);
esp_err_t esp_timer_stop(esp_timer_handle_t timer);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);
esp_err_t esp_timer_dump(FILE* stream);
bool esp_timer_is_active(esp_timer_handle_t timer);

#ifdef __cplusplus
}
#endif

#endif /* ESP_TIMER_H */
