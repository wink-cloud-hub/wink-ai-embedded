/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "hal/pal_uart.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

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
    bool armed;
    bool pending_periodic_reload;
    uint64_t period_us;
    uint64_t alarm_us;
} esp_timer_obj_t;

static esp_timer_obj_t s_timers[ESP_TIMER_POOL_SIZE];

static void prv_esp_timer_cb(TimerHandle_t xTimer) {
    esp_timer_obj_t *t = (esp_timer_obj_t *)pvTimerGetTimerID(xTimer);
    if (!t || !t->used || !t->callback) {
        return;
    }

    if (t->periodic) {
        t->alarm_us += t->period_us;
        if (t->pending_periodic_reload) {
            t->pending_periodic_reload = false;
            vTimerSetReloadMode(t->os_timer, pdTRUE);
            TickType_t ticks = (TickType_t)(t->period_us / (portTICK_PERIOD_MS * 1000ULL));
            if (ticks == 0) {
                ticks = 1;
            }
            xTimerChangePeriod(t->os_timer, ticks, 0);
            xTimerStart(t->os_timer, 0);
        }
    } else {
        t->armed = false;
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
    t->armed = false;
    t->pending_periodic_reload = false;
    t->period_us = 0;
    t->alarm_us = 0;

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
    if (t->armed) {
        return ESP_ERR_INVALID_STATE;
    }

    TickType_t ticks = (TickType_t)(timeout_us / (portTICK_PERIOD_MS * 1000ULL));
    if (ticks == 0) {
        ticks = 1;
    }

    int64_t now = esp_timer_get_time();
    t->periodic = false;
    t->period_us = 0;
    t->alarm_us = (uint64_t)now + timeout_us;
    t->pending_periodic_reload = false;

    vTimerSetReloadMode(t->os_timer, pdFALSE);
    if (xTimerChangePeriod(t->os_timer, ticks, 0) != pdPASS) {
        return ESP_FAIL;
    }
    if (xTimerStart(t->os_timer, 0) != pdPASS) {
        return ESP_FAIL;
    }
    t->armed = true;
    return ESP_OK;
}

esp_err_t esp_timer_start_once_at(esp_timer_handle_t timer, uint64_t alarm_us) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t) {
        return ESP_ERR_INVALID_ARG;
    }
    if (t->armed) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t now = esp_timer_get_time();
    if ((int64_t)alarm_us < now) {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t delay_us = alarm_us - (uint64_t)now;
    TickType_t ticks = (TickType_t)(delay_us / (portTICK_PERIOD_MS * 1000ULL));
    if (ticks == 0) {
        ticks = 1;
    }

    t->periodic = false;
    t->period_us = 0;
    t->alarm_us = alarm_us;
    t->pending_periodic_reload = false;

    vTimerSetReloadMode(t->os_timer, pdFALSE);
    if (xTimerChangePeriod(t->os_timer, ticks, 0) != pdPASS) {
        return ESP_FAIL;
    }
    if (xTimerStart(t->os_timer, 0) != pdPASS) {
        return ESP_FAIL;
    }
    t->armed = true;
    return ESP_OK;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t || period_us == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (t->armed) {
        return ESP_ERR_INVALID_STATE;
    }

    TickType_t ticks = (TickType_t)(period_us / (portTICK_PERIOD_MS * 1000ULL));
    if (ticks == 0) {
        ticks = 1;
    }

    int64_t now = esp_timer_get_time();
    t->periodic = true;
    t->period_us = period_us;
    t->alarm_us = (uint64_t)now + period_us;
    t->pending_periodic_reload = false;

    vTimerSetReloadMode(t->os_timer, pdTRUE);
    if (xTimerChangePeriod(t->os_timer, ticks, 0) != pdPASS) {
        return ESP_FAIL;
    }
    if (xTimerStart(t->os_timer, 0) != pdPASS) {
        return ESP_FAIL;
    }
    t->armed = true;
    return ESP_OK;
}

esp_err_t esp_timer_start_periodic_at(esp_timer_handle_t timer, uint64_t period_us, uint64_t first_alarm_us) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t || period_us == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (t->armed) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t now = esp_timer_get_time();
    if ((int64_t)first_alarm_us < now) {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t delay_us = first_alarm_us - (uint64_t)now;
    t->periodic = true;
    t->period_us = period_us;
    t->alarm_us = first_alarm_us;

    if (delay_us == period_us || delay_us == 0) {
        t->pending_periodic_reload = false;
        vTimerSetReloadMode(t->os_timer, pdTRUE);
        TickType_t ticks = (TickType_t)(period_us / (portTICK_PERIOD_MS * 1000ULL));
        if (ticks == 0) {
            ticks = 1;
        }
        if (xTimerChangePeriod(t->os_timer, ticks, 0) != pdPASS) {
            return ESP_FAIL;
        }
    } else {
        t->pending_periodic_reload = true;
        vTimerSetReloadMode(t->os_timer, pdFALSE);
        TickType_t ticks = (TickType_t)(delay_us / (portTICK_PERIOD_MS * 1000ULL));
        if (ticks == 0) {
            ticks = 1;
        }
        if (xTimerChangePeriod(t->os_timer, ticks, 0) != pdPASS) {
            return ESP_FAIL;
        }
    }

    if (xTimerStart(t->os_timer, 0) != pdPASS) {
        return ESP_FAIL;
    }
    t->armed = true;
    return ESP_OK;
}

esp_err_t esp_timer_restart(esp_timer_handle_t timer, uint64_t timeout_us) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t || timeout_us == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!t->armed) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t now = esp_timer_get_time();
    t->alarm_us = (uint64_t)now + timeout_us;
    if (t->periodic) {
        t->period_us = timeout_us;
    }
    t->pending_periodic_reload = false;

    vTimerSetReloadMode(t->os_timer, t->periodic ? pdTRUE : pdFALSE);
    TickType_t ticks = (TickType_t)(timeout_us / (portTICK_PERIOD_MS * 1000ULL));
    if (ticks == 0) {
        ticks = 1;
    }
    if (xTimerChangePeriod(t->os_timer, ticks, 0) != pdPASS) {
        return ESP_FAIL;
    }
    if (xTimerStart(t->os_timer, 0) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_timer_restart_at(esp_timer_handle_t timer, uint64_t period_us, uint64_t first_alarm_us) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t || period_us == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!t->armed) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t now = esp_timer_get_time();
    if ((int64_t)first_alarm_us < now) {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t delay_us = first_alarm_us - (uint64_t)now;
    t->periodic = true;
    t->period_us = period_us;
    t->alarm_us = first_alarm_us;

    (void)xTimerStop(t->os_timer, 0);

    if (delay_us == period_us || delay_us == 0) {
        t->pending_periodic_reload = false;
        vTimerSetReloadMode(t->os_timer, pdTRUE);
        TickType_t ticks = (TickType_t)(period_us / (portTICK_PERIOD_MS * 1000ULL));
        if (ticks == 0) {
            ticks = 1;
        }
        if (xTimerChangePeriod(t->os_timer, ticks, 0) != pdPASS) {
            return ESP_FAIL;
        }
    } else {
        t->pending_periodic_reload = true;
        vTimerSetReloadMode(t->os_timer, pdFALSE);
        TickType_t ticks = (TickType_t)(delay_us / (portTICK_PERIOD_MS * 1000ULL));
        if (ticks == 0) {
            ticks = 1;
        }
        if (xTimerChangePeriod(t->os_timer, ticks, 0) != pdPASS) {
            return ESP_FAIL;
        }
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
    if (!t->armed) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xTimerStop(t->os_timer, 0) != pdPASS) {
        return ESP_FAIL;
    }
    t->armed = false;
    t->pending_periodic_reload = false;
    return ESP_OK;
}

esp_err_t esp_timer_delete(esp_timer_handle_t timer) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t) {
        return ESP_ERR_INVALID_ARG;
    }
    if (t->armed) {
        return ESP_ERR_INVALID_STATE;
    }

    (void)xTimerDelete(t->os_timer, 0);
    memset(t, 0, sizeof(*t));
    return ESP_OK;
}

bool esp_timer_is_active(esp_timer_handle_t timer) {
    esp_timer_obj_t *t = resolve_esp_timer(timer);
    if (!t || !t->used) {
        return false;
    }
    return t->armed;
}

esp_err_t esp_timer_dump(FILE* stream) {
    if (!stream) {
        return ESP_ERR_INVALID_ARG;
    }

    fprintf(stream, "Timer stats:\n");
    fprintf(stream, "%-20s  %-10s  %-12s\n", "Name", "Period", "Alarm");

#if defined(__EMSCRIPTEN__)
    if (stream == stdout || stream == stderr) {
        (void)pal_uart_write(0, (const uint8_t *)"Timer stats:\n", 13);
        (void)pal_uart_write(0, (const uint8_t *)"Name                  Period      Alarm       \n", 46);
    }
#endif

    for (int i = 0; i < ESP_TIMER_POOL_SIZE; i++) {
        esp_timer_obj_t *t = &s_timers[i];
        if (t->used && t->armed) {
            char line[128];
            int len = snprintf(line, sizeof(line), "%-20s  %-10llu  %-12llu\n",
                               t->name, (unsigned long long)t->period_us, (unsigned long long)t->alarm_us);
            if (len > 0) {
                fputs(line, stream);
#if defined(__EMSCRIPTEN__)
                if (stream == stdout || stream == stderr) {
                    (void)pal_uart_write(0, (const uint8_t *)line, (uint32_t)len);
                }
#endif
            }
        }
    }
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
