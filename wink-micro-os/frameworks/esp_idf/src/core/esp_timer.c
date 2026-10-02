/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TAG "ESP_TIMER"

#ifndef ESP_TIMER_POOL_SIZE
#define ESP_TIMER_POOL_SIZE 16
#endif

typedef struct esp_timer {
    bool used;
    TimerHandle_t os_timer;
    esp_timer_cb_t callback;
    void *arg;
    char name[16];
    bool periodic;
} esp_timer_obj_t;

static esp_timer_obj_t s_timers[ESP_TIMER_POOL_SIZE];

static void prv_esp_timer_cb(TimerHandle_t xTimer) {
    esp_timer_obj_t *t = (esp_timer_obj_t *)pvTimerGetTimerID(xTimer);
    if (!t || !t->used || !t->callback) {
        return;
    }
    t->callback(t->arg);
}

esp_err_t esp_timer_create(const esp_timer_create_args_t* create_args, esp_timer_handle_t* out_handle) {
    if (!create_args || !create_args->callback || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    int free_slot = -1;
    for (int i = 0; i < ESP_TIMER_POOL_SIZE; i++) {
        if (!s_timers[i].used) {
            free_slot = i;
            break;
        }
    }
    if (free_slot < 0) {
        ESP_LOGE(TAG, "esp_timer pool exhausted (max=%d)", ESP_TIMER_POOL_SIZE);
        return ESP_ERR_NO_MEM;
    }

    esp_timer_obj_t *t = &s_timers[free_slot];
    memset(t, 0, sizeof(*t));

    const char *name = create_args->name ? create_args->name : "esp_timer";
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';
    t->callback = create_args->callback;
    t->arg = create_args->arg;
    t->periodic = false;

    /* Create underlying FreeRTOS timer with 1 tick default, one-shot initially */
    t->os_timer = xTimerCreate(t->name, 1, pdFALSE, (void *)t, prv_esp_timer_cb);
    if (!t->os_timer) {
        return ESP_ERR_NO_MEM;
    }

    t->used = true;
    *out_handle = (esp_timer_handle_t)t;
    return ESP_OK;
}

static inline esp_timer_obj_t *resolve_esp_timer(esp_timer_handle_t timer) {
    if (!timer) {
        return NULL;
    }
    esp_timer_obj_t *t = (esp_timer_obj_t *)timer;
    if (t < &s_timers[0] || t >= &s_timers[ESP_TIMER_POOL_SIZE] || !t->used) {
        return NULL;
    }
    return t;
}

esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t || timeout_us == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    TickType_t ticks = (TickType_t)(timeout_us / (portTICK_PERIOD_MS * 1000ULL));
    if (ticks == 0) {
        ticks = 1;
    }

    t->periodic = false;
    vTimerSetReloadMode(t->os_timer, pdFALSE);
    if (xTimerChangePeriod(t->os_timer, ticks, 0) != pdPASS) {
        return ESP_FAIL;
    }
    if (xTimerStart(t->os_timer, 0) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t || period_us == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    TickType_t ticks = (TickType_t)(period_us / (portTICK_PERIOD_MS * 1000ULL));
    if (ticks == 0) {
        ticks = 1;
    }

    t->periodic = true;
    vTimerSetReloadMode(t->os_timer, pdTRUE);
    if (xTimerChangePeriod(t->os_timer, ticks, 0) != pdPASS) {
        return ESP_FAIL;
    }
    if (xTimerStart(t->os_timer, 0) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_timer_stop(esp_timer_handle_t timer) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xTimerStop(t->os_timer, 0) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_timer_delete(esp_timer_handle_t timer) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)xTimerDelete(t->os_timer, 0);
    memset(t, 0, sizeof(*t));
    return ESP_OK;
}

void esp_timer_sim_reset(void) {
    for (int i = 0; i < ESP_TIMER_POOL_SIZE; i++) {
        if (s_timers[i].used) {
            if (s_timers[i].os_timer) {
                (void)xTimerDelete(s_timers[i].os_timer, 0);
            }
            memset(&s_timers[i], 0, sizeof(s_timers[i]));
        }
    }
}
