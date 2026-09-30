/* SPDX-License-Identifier: LGPL-3.0-only */
/* src/core/esp_event.c */
#include "esp_event.h"
#include "esp_log.h"
#include "pal_log.h"
#include "sdkconfig_base.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#ifndef WINK_ESP_EVENT_QUEUE_CAPACITY
#  define WINK_ESP_EVENT_QUEUE_CAPACITY 32
#endif

#ifndef WINK_ESP_EVENT_MAX_PAYLOAD
#  define WINK_ESP_EVENT_MAX_PAYLOAD 1024
#endif

#ifndef ESP_EVENT_HANDLER_MAX
#  define ESP_EVENT_HANDLER_MAX 16
#endif

typedef struct {
    bool in_use;
    esp_event_base_t base;
    int32_t event_id;
    size_t data_size;
    uint8_t data[WINK_ESP_EVENT_MAX_PAYLOAD];
} esp_event_item_t;

typedef struct {
    bool used;
    uint32_t token;
    esp_event_base_t base;
    int32_t event_id;
    esp_event_handler_t handler;
    void *arg;
} esp_event_slot_t;

static esp_event_slot_t s_handlers[ESP_EVENT_HANDLER_MAX];
static uint32_t s_handler_sequence = 1;
static bool s_loop_created = false;
static TaskHandle_t s_sys_evt_task = NULL;
static SemaphoreHandle_t s_q_sem = NULL;
static bool s_in_sys_evt = false;

static esp_event_item_t s_queue[WINK_ESP_EVENT_QUEUE_CAPACITY];
static uint32_t s_q_head = 0;
static uint32_t s_q_tail = 0;
static uint32_t s_q_count = 0;

static inline bool base_matches(esp_event_base_t slot_base, esp_event_base_t target_base) {
    if (slot_base == ESP_EVENT_ANY_BASE) return true;
    if (slot_base == target_base) return true;
    if (slot_base && target_base && strcmp(slot_base, target_base) == 0) return true;
    return false;
}

void esp_event_loop_sim_reset(void) {
    s_loop_created = false;
    if (s_sys_evt_task) {
        TaskHandle_t t = s_sys_evt_task;
        s_sys_evt_task = NULL;
        vTaskDelete(t);
    }
    if (s_q_sem) {
        SemaphoreHandle_t sem = s_q_sem;
        s_q_sem = NULL;
        vSemaphoreDelete(sem);
    }
    memset(s_handlers, 0, sizeof(s_handlers));
    memset(s_queue, 0, sizeof(s_queue));
    s_q_head = 0;
    s_q_tail = 0;
    s_q_count = 0;
    s_in_sys_evt = false;
}

esp_err_t esp_event_loop_run_step(void) {
    if (!s_loop_created) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_q_count == 0) {
        return ESP_OK;
    }

    if (s_q_sem && !s_in_sys_evt) {
        (void)xSemaphoreTake(s_q_sem, 0);
    }

    esp_event_item_t item = s_queue[s_q_head];
    s_queue[s_q_head].in_use = false;
    s_q_head = (s_q_head + 1) % WINK_ESP_EVENT_QUEUE_CAPACITY;
    s_q_count--;

    /* 快照派发：防止回调内 unregister/register 导致遍历数组被篡改 */
    esp_event_slot_t snapshot[ESP_EVENT_HANDLER_MAX];
    memcpy(snapshot, s_handlers, sizeof(s_handlers));

    for (int i = 0; i < ESP_EVENT_HANDLER_MAX; i++) {
        esp_event_slot_t *s = &snapshot[i];
        if (!s->used) continue;
        /* 检查原始池中是否已被前序回调注销或被复用换代 */
        if (!s_handlers[i].used || s_handlers[i].token != s->token) continue;

        bool bm = base_matches(s->base, item.base);
        bool im = (s->event_id == ESP_EVENT_ANY_ID) || (s->event_id == item.event_id);
        if (bm && im) {
            void *payload = (item.data_size > 0) ? item.data : NULL;
            s->handler(s->arg, item.base, item.event_id, payload);
        }
    }
    return ESP_OK;
}

int esp_event_loop_run_all_pending(void) {
    if (!s_loop_created) {
        return 0;
    }
    int processed = 0;
    while (s_q_count > 0) {
        if (esp_event_loop_run_step() != ESP_OK) {
            break;
        }
        processed++;
    }
    return processed;
}

static void sys_evt_task(void *arg) {
    (void)arg;
    pal_log_i("SYS_EVT", "task started, s_loop_created=%d s_q_sem=%p", (int)s_loop_created, (void*)s_q_sem);
    while (s_loop_created) {
        if (!s_q_sem) {
            break;
        }
        pal_log_i("SYS_EVT", "before xSemaphoreTake");
        BaseType_t res = xSemaphoreTake(s_q_sem, portMAX_DELAY);
        pal_log_i("SYS_EVT", "after xSemaphoreTake res=%d", (int)res);
        if (!s_loop_created) {
            break;
        }
        if (res == pdPASS) {
            s_in_sys_evt = true;
            (void)esp_event_loop_run_step();
            s_in_sys_evt = false;
        }
    }
    vTaskDelete(NULL);
}

esp_err_t esp_event_loop_create_default(void) {
    if (s_loop_created) {
        return ESP_ERR_INVALID_STATE;
    }
    s_loop_created = true;
    s_q_head = 0;
    s_q_tail = 0;
    s_q_count = 0;
    memset(s_queue, 0, sizeof(s_queue));

    s_q_sem = xSemaphoreCreateCounting(WINK_ESP_EVENT_QUEUE_CAPACITY, 0);
    s_sys_evt_task = NULL;
    (void)xTaskCreate(sys_evt_task, "sys_evt", 32768, NULL, 5, &s_sys_evt_task);
    return ESP_OK;
}

esp_err_t esp_event_loop_delete_default(void) {
    if (!s_loop_created) {
        return ESP_ERR_INVALID_STATE;
    }
    s_loop_created = false;
    if (s_sys_evt_task) {
        TaskHandle_t t = s_sys_evt_task;
        s_sys_evt_task = NULL;
        vTaskDelete(t);
    }
    if (s_q_sem) {
        SemaphoreHandle_t sem = s_q_sem;
        s_q_sem = NULL;
        vSemaphoreDelete(sem);
    }
    esp_event_loop_sim_reset();
    return ESP_OK;
}

esp_err_t esp_event_handler_register(esp_event_base_t event_base,
                                     int32_t event_id,
                                     esp_event_handler_t event_handler,
                                     void* arg) {
    if (!event_handler) {
        return ESP_ERR_INVALID_ARG;
    }
    /* 幂等更新现有同三元组配置 */
    for (int i = 0; i < ESP_EVENT_HANDLER_MAX; i++) {
        esp_event_slot_t *s = &s_handlers[i];
        if (s->used && base_matches(s->base, event_base) && s->event_id == event_id
            && s->handler == event_handler) {
            s->arg = arg;
            return ESP_OK;
        }
    }
    for (int i = 0; i < ESP_EVENT_HANDLER_MAX; i++) {
        if (!s_handlers[i].used) {
            uint32_t token = (s_handler_sequence++ << 8) | ((uint32_t)i & 0xFFu);
            if (s_handler_sequence == 0) s_handler_sequence = 1;
            s_handlers[i] = (esp_event_slot_t){
                .used = true,
                .token = token,
                .base = event_base,
                .event_id = event_id,
                .handler = event_handler,
                .arg = arg
            };
            return ESP_OK;
        }
    }
    ESP_LOGE("ESP_EVENT", "Handler pool full (max=%d)", ESP_EVENT_HANDLER_MAX);
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_event_handler_unregister(esp_event_base_t base,
                                       int32_t event_id,
                                       esp_event_handler_t handler) {
    for (int i = 0; i < ESP_EVENT_HANDLER_MAX; i++) {
        esp_event_slot_t *s = &s_handlers[i];
        if (s->used && base_matches(s->base, base) && s->event_id == event_id
            && s->handler == handler) {
            memset(s, 0, sizeof(*s));
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_event_handler_instance_register(esp_event_base_t base,
                                              int32_t event_id,
                                              esp_event_handler_t handler,
                                              void* arg,
                                              esp_event_handler_instance_t* instance) {
    if (!handler) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < ESP_EVENT_HANDLER_MAX; i++) {
        esp_event_slot_t *s = &s_handlers[i];
        if (!s->used) {
            uint32_t token = (s_handler_sequence++ << 8) | ((uint32_t)i & 0xFFu);
            if (s_handler_sequence == 0) s_handler_sequence = 1;
            *s = (esp_event_slot_t){
                .used = true,
                .token = token,
                .base = base,
                .event_id = event_id,
                .handler = handler,
                .arg = arg
            };
            if (instance) {
                *instance = (esp_event_handler_instance_t)(uintptr_t)token;
            }
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base,
                                                int32_t event_id,
                                                esp_event_handler_instance_t instance) {
    if (!instance) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t token = (uint32_t)(uintptr_t)instance;
    uint32_t slot = token & 0xFFu;
    if (slot >= ESP_EVENT_HANDLER_MAX) {
        return ESP_ERR_NOT_FOUND;
    }
    esp_event_slot_t *s = &s_handlers[slot];
    if (s->used && s->token == token) {
        if ((base == ESP_EVENT_ANY_BASE || base_matches(s->base, base)) &&
            (event_id == ESP_EVENT_ANY_ID || s->event_id == event_id)) {
            memset(s, 0, sizeof(*s));
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_event_post(esp_event_base_t base,
                         int32_t event_id,
                         void* data,
                         size_t data_size,
                         TickType_t ticks_to_wait) {
    if (!s_loop_created) {
        return ESP_ERR_INVALID_STATE;
    }
    if (base == ESP_EVENT_ANY_BASE || event_id == ESP_EVENT_ANY_ID) {
        return ESP_ERR_INVALID_ARG;
    }
    if (data == NULL && data_size > 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (data_size > WINK_ESP_EVENT_MAX_PAYLOAD) {
        return ESP_ERR_NO_MEM;
    }

    if (s_q_count >= WINK_ESP_EVENT_QUEUE_CAPACITY) {
        if (ticks_to_wait == 0) {
            return ESP_ERR_TIMEOUT;
        }
        return ESP_ERR_TIMEOUT;
    }

    esp_event_item_t *item = &s_queue[s_q_tail];
    item->in_use = true;
    item->base = base;
    item->event_id = event_id;
    item->data_size = data_size;
    if (data && data_size > 0) {
        memcpy(item->data, data, data_size);
    }

    s_q_tail = (s_q_tail + 1) % WINK_ESP_EVENT_QUEUE_CAPACITY;
    s_q_count++;

    if (s_q_sem) {
        xSemaphoreGive(s_q_sem);
    }

    return ESP_OK;
}
