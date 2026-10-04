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

#ifndef WINK_ESP_EVENT_USER_LOOPS_MAX
#  define WINK_ESP_EVENT_USER_LOOPS_MAX 8
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

struct esp_event_loop_instance {
    bool is_created;
    bool in_sys_evt;
    TaskHandle_t task;
    SemaphoreHandle_t q_sem;
    uint32_t queue_capacity;
    esp_event_item_t queue[WINK_ESP_EVENT_QUEUE_CAPACITY];
    uint32_t q_head;
    uint32_t q_tail;
    uint32_t q_count;
    esp_event_slot_t handlers[ESP_EVENT_HANDLER_MAX];
    uint32_t handler_sequence;
};

static struct esp_event_loop_instance s_default_loop;
static struct esp_event_loop_instance s_user_loops[WINK_ESP_EVENT_USER_LOOPS_MAX];

static inline bool base_matches(esp_event_base_t slot_base, esp_event_base_t target_base) {
    if (slot_base == ESP_EVENT_ANY_BASE) return true;
    if (slot_base == target_base) return true;
    if (slot_base && target_base && strcmp(slot_base, target_base) == 0) return true;
    return false;
}

static esp_err_t esp_event_loop_run_internal(struct esp_event_loop_instance *loop) {
    if (!loop || !loop->is_created) {
        return ESP_ERR_INVALID_STATE;
    }
    if (loop->q_count == 0) {
        return ESP_OK;
    }

    esp_event_item_t item = loop->queue[loop->q_head];
    loop->queue[loop->q_head].in_use = false;
    uint32_t cap = loop->queue_capacity ? loop->queue_capacity : WINK_ESP_EVENT_QUEUE_CAPACITY;
    loop->q_head = (loop->q_head + 1) % cap;
    loop->q_count--;

    /* 快照派发：防止回调内 unregister/register 导致遍历数组被篡改 */
    esp_event_slot_t snapshot[ESP_EVENT_HANDLER_MAX];
    memcpy(snapshot, loop->handlers, sizeof(snapshot));

    for (int i = 0; i < ESP_EVENT_HANDLER_MAX; i++) {
        esp_event_slot_t *s = &snapshot[i];
        if (!s->used) continue;
        /* 检查原始池中是否已被前序回调注销或被复用换代 */
        if (!loop->handlers[i].used || loop->handlers[i].token != s->token) continue;

        bool bm = base_matches(s->base, item.base);
        bool im = (s->event_id == ESP_EVENT_ANY_ID) || (s->event_id == item.event_id);
        if (bm && im) {
            void *payload = (item.data_size > 0) ? item.data : NULL;
            s->handler(s->arg, item.base, item.event_id, payload);
        }
    }
    return ESP_OK;
}

static void event_loop_task_entry(void *arg) {
    struct esp_event_loop_instance *loop = (struct esp_event_loop_instance *)arg;
    pal_log_i("ESP_EVENT", "loop task started: loop=%p, sem=%p", (void*)loop, (void*)(loop ? loop->q_sem : NULL));
    while (loop && loop->is_created) {
        if (!loop->q_sem) break;
        BaseType_t res = xSemaphoreTake(loop->q_sem, portMAX_DELAY);
        if (!loop || !loop->is_created) break;
        if (res == pdPASS) {
            loop->in_sys_evt = true;
            (void)esp_event_loop_run_internal(loop);
            loop->in_sys_evt = false;
        }
    }
    if (loop) {
        loop->task = NULL;
    }
    vTaskDelete(NULL);
}

void esp_event_loop_sim_reset(void) {
    /* Reset default loop */
    if (s_default_loop.is_created) {
        (void)esp_event_loop_delete(&s_default_loop);
    }
    memset(&s_default_loop, 0, sizeof(s_default_loop));

    /* Reset user loops */
    for (int i = 0; i < WINK_ESP_EVENT_USER_LOOPS_MAX; i++) {
        if (s_user_loops[i].is_created) {
            (void)esp_event_loop_delete(&s_user_loops[i]);
        }
        memset(&s_user_loops[i], 0, sizeof(s_user_loops[i]));
    }
}

esp_err_t esp_event_loop_create(const esp_event_loop_args_t* args, esp_event_loop_handle_t* out_handle) {
    if (!args || !out_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct esp_event_loop_instance *loop = NULL;
    for (int i = 0; i < WINK_ESP_EVENT_USER_LOOPS_MAX; i++) {
        if (!s_user_loops[i].is_created) {
            loop = &s_user_loops[i];
            break;
        }
    }
    if (!loop) {
        ESP_LOGE("ESP_EVENT", "User event loop pool exhausted (max=%d)", WINK_ESP_EVENT_USER_LOOPS_MAX);
        return ESP_ERR_NO_MEM;
    }

    memset(loop, 0, sizeof(*loop));
    loop->is_created = true;
    uint32_t cap = (uint32_t)args->queue_size;
    if (cap == 0 || cap > WINK_ESP_EVENT_QUEUE_CAPACITY) {
        cap = WINK_ESP_EVENT_QUEUE_CAPACITY;
    }
    loop->queue_capacity = cap;
    loop->handler_sequence = 1;
    loop->q_sem = xSemaphoreCreateCounting(cap, 0);

    if (args->task_name != NULL) {
        uint32_t stack_size = args->task_stack_size > 0 ? args->task_stack_size : 4096;
        UBaseType_t prio = args->task_priority > 0 ? args->task_priority : 5;
        BaseType_t rc = xTaskCreate(event_loop_task_entry, args->task_name, stack_size, loop, prio, &loop->task);
        if (rc != pdPASS) {
            if (loop->q_sem) {
                vSemaphoreDelete(loop->q_sem);
                loop->q_sem = NULL;
            }
            loop->is_created = false;
            return ESP_ERR_NO_MEM;
        }
    }

    *out_handle = loop;
    return ESP_OK;
}

esp_err_t esp_event_loop_delete(esp_event_loop_handle_t loop) {
    if (!loop || !loop->is_created) {
        return ESP_ERR_INVALID_STATE;
    }
    loop->is_created = false;
    if (loop->task) {
        TaskHandle_t t = loop->task;
        loop->task = NULL;
        vTaskDelete(t);
    }
    if (loop->q_sem) {
        SemaphoreHandle_t sem = loop->q_sem;
        loop->q_sem = NULL;
        vSemaphoreDelete(sem);
    }
    memset(loop, 0, sizeof(*loop));
    return ESP_OK;
}

esp_err_t esp_event_loop_run(esp_event_loop_handle_t loop, TickType_t ticks_to_run) {
    if (!loop || !loop->is_created) {
        return ESP_ERR_INVALID_STATE;
    }

    if (loop->q_count == 0) {
        if (loop->q_sem && ticks_to_run > 0) {
            BaseType_t res = xSemaphoreTake(loop->q_sem, ticks_to_run);
            if (res != pdPASS) {
                return ESP_OK;
            }
        } else {
            return ESP_OK;
        }
    } else {
        if (loop->q_sem && !loop->in_sys_evt) {
            (void)xSemaphoreTake(loop->q_sem, 0);
        }
    }

    return esp_event_loop_run_internal(loop);
}

esp_err_t esp_event_loop_run_step(void) {
    return esp_event_loop_run(&s_default_loop, 0);
}

int esp_event_loop_run_all_pending(void) {
    if (!s_default_loop.is_created) {
        return 0;
    }
    int processed = 0;
    while (s_default_loop.q_count > 0) {
        if (esp_event_loop_run_step() != ESP_OK) {
            break;
        }
        processed++;
    }
    return processed;
}

esp_err_t esp_event_loop_create_default(void) {
    if (s_default_loop.is_created) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(&s_default_loop, 0, sizeof(s_default_loop));
    s_default_loop.is_created = true;
    s_default_loop.queue_capacity = WINK_ESP_EVENT_QUEUE_CAPACITY;
    s_default_loop.handler_sequence = 1;
    s_default_loop.q_sem = xSemaphoreCreateCounting(WINK_ESP_EVENT_QUEUE_CAPACITY, 0);

    BaseType_t rc = xTaskCreate(event_loop_task_entry, "sys_evt", 32768, &s_default_loop, 5, &s_default_loop.task);
    if (rc != pdPASS) {
        if (s_default_loop.q_sem) {
            vSemaphoreDelete(s_default_loop.q_sem);
            s_default_loop.q_sem = NULL;
        }
        s_default_loop.is_created = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t esp_event_loop_delete_default(void) {
    return esp_event_loop_delete(&s_default_loop);
}

esp_err_t esp_event_post_to(esp_event_loop_handle_t loop,
                            esp_event_base_t base,
                            int32_t event_id,
                            void* data,
                            size_t data_size,
                            TickType_t ticks_to_wait) {
    (void)ticks_to_wait;
    if (!loop || !loop->is_created) {
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

    uint32_t cap = loop->queue_capacity ? loop->queue_capacity : WINK_ESP_EVENT_QUEUE_CAPACITY;
    if (loop->q_count >= cap) {
        return ESP_ERR_TIMEOUT;
    }

    esp_event_item_t *item = &loop->queue[loop->q_tail];
    item->in_use = true;
    item->base = base;
    item->event_id = event_id;
    item->data_size = data_size;
    if (data && data_size > 0) {
        memcpy(item->data, data, data_size);
    }

    loop->q_tail = (loop->q_tail + 1) % cap;
    loop->q_count++;

    if (loop->q_sem) {
        xSemaphoreGive(loop->q_sem);
    }

    return ESP_OK;
}

esp_err_t esp_event_post(esp_event_base_t base,
                         int32_t event_id,
                         void* data,
                         size_t data_size,
                         TickType_t ticks_to_wait) {
    return esp_event_post_to(&s_default_loop, base, event_id, data, data_size, ticks_to_wait);
}

esp_err_t esp_event_handler_instance_register_with(esp_event_loop_handle_t loop,
                                                  esp_event_base_t base,
                                                  int32_t event_id,
                                                  esp_event_handler_t handler,
                                                  void* arg,
                                                  esp_event_handler_instance_t* instance) {
    if (!loop || !loop->is_created || !handler) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < ESP_EVENT_HANDLER_MAX; i++) {
        esp_event_slot_t *s = &loop->handlers[i];
        if (!s->used) {
            uint32_t token = (loop->handler_sequence++ << 8) | ((uint32_t)i & 0xFFu);
            if (loop->handler_sequence == 0) loop->handler_sequence = 1;
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

    ESP_LOGE("ESP_EVENT", "Handler pool full (max=%d)", ESP_EVENT_HANDLER_MAX);
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_event_handler_instance_unregister_with(esp_event_loop_handle_t loop,
                                                    esp_event_base_t base,
                                                    int32_t event_id,
                                                    esp_event_handler_instance_t instance) {
    if (!loop || !loop->is_created || !instance) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t token = (uint32_t)(uintptr_t)instance;
    uint32_t slot = token & 0xFFu;
    if (slot >= ESP_EVENT_HANDLER_MAX) {
        return ESP_ERR_NOT_FOUND;
    }
    esp_event_slot_t *s = &loop->handlers[slot];
    if (s->used && s->token == token) {
        if ((base == ESP_EVENT_ANY_BASE || base_matches(s->base, base)) &&
            (event_id == ESP_EVENT_ANY_ID || s->event_id == event_id)) {
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
    return esp_event_handler_instance_register_with(&s_default_loop, base, event_id, handler, arg, instance);
}

esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base,
                                                int32_t event_id,
                                                esp_event_handler_instance_t instance) {
    return esp_event_handler_instance_unregister_with(&s_default_loop, base, event_id, instance);
}

esp_err_t esp_event_handler_register(esp_event_base_t event_base,
                                     int32_t event_id,
                                     esp_event_handler_t event_handler,
                                     void* arg) {
    return esp_event_handler_instance_register_with(&s_default_loop, event_base, event_id, event_handler, arg, NULL);
}

esp_err_t esp_event_handler_unregister(esp_event_base_t base,
                                       int32_t event_id,
                                       esp_event_handler_t handler) {
    if (!s_default_loop.is_created) {
        return ESP_ERR_INVALID_STATE;
    }
    for (int i = 0; i < ESP_EVENT_HANDLER_MAX; i++) {
        esp_event_slot_t *s = &s_default_loop.handlers[i];
        if (s->used && base_matches(s->base, base) && s->event_id == event_id && s->handler == handler) {
            memset(s, 0, sizeof(*s));
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}
