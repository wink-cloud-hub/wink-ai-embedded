/* SPDX-License-Identifier: LGPL-3.0-only */
/* src/core/esp_event.c */
#include "esp_event.h"
#include "esp_log.h"
#include <string.h>

#ifndef ESP_EVENT_HANDLER_MAX
#  define ESP_EVENT_HANDLER_MAX 16
#endif

typedef struct {
    bool used;
    esp_event_base_t base;
    int32_t event_id;
    esp_event_handler_t handler;
    void *arg;
} esp_event_slot_t;

static esp_event_slot_t s_handlers[ESP_EVENT_HANDLER_MAX];
static bool s_loop_created = false;

static inline bool base_matches(esp_event_base_t slot_base, esp_event_base_t target_base) {
    if (slot_base == ESP_EVENT_ANY_BASE) return true;
    if (slot_base == target_base) return true;
    if (slot_base && target_base && strcmp(slot_base, target_base) == 0) return true;
    return false;
}

void esp_event_loop_sim_reset(void) {
    memset(s_handlers, 0, sizeof(s_handlers));
    s_loop_created = false;
}

esp_err_t esp_event_loop_create_default(void) {
    if (s_loop_created) {
        return ESP_ERR_INVALID_STATE;
    }
    s_loop_created = true;
    return ESP_OK;
}

esp_err_t esp_event_loop_delete_default(void) {
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
            s_handlers[i] = (esp_event_slot_t){
                .used = true,
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
            *s = (esp_event_slot_t){
                .used = true,
                .base = base,
                .event_id = event_id,
                .handler = handler,
                .arg = arg
            };
            if (instance) {
                *instance = (esp_event_handler_instance_t)s;
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
    esp_event_slot_t *s = (esp_event_slot_t*)instance;
    if (s >= &s_handlers[0] && s < &s_handlers[ESP_EVENT_HANDLER_MAX] && s->used) {
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
    (void)ticks_to_wait;
    (void)data_size;
    /* 快照派发：防止回调内 unregister/register 导致遍历数组被篡改 */
    esp_event_slot_t snapshot[ESP_EVENT_HANDLER_MAX];
    memcpy(snapshot, s_handlers, sizeof(s_handlers));

    for (int i = 0; i < ESP_EVENT_HANDLER_MAX; i++) {
        esp_event_slot_t *s = &snapshot[i];
        if (!s->used) continue;
        /* 检查原始池中是否已被前序回调注销 */
        if (!s_handlers[i].used) continue;

        bool bm = base_matches(s->base, base);
        bool im = (s->event_id == ESP_EVENT_ANY_ID) || (s->event_id == event_id);
        if (bm && im) {
            s->handler(s->arg, base, event_id, data);
        }
    }
    return ESP_OK;
}
