/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "esp_task_wdt";

#define TWDT_MAX_TASKS 16
#define TWDT_MAX_USERS 16

struct esp_task_wdt_user_handle_s {
    char name[32];
    bool in_use;
    TickType_t last_reset_tick;
};

typedef struct {
    TaskHandle_t handle;
    bool in_use;
    TickType_t last_reset_tick;
} twdt_task_entry_t;

typedef struct {
    bool initialized;
    esp_task_wdt_config_t config;
    twdt_task_entry_t tasks[TWDT_MAX_TASKS];
    struct esp_task_wdt_user_handle_s users[TWDT_MAX_USERS];
} twdt_ctx_t;

static twdt_ctx_t s_twdt_ctx;

esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *config) {
    if (s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(&s_twdt_ctx, 0, sizeof(s_twdt_ctx));
    s_twdt_ctx.config = *config;
    s_twdt_ctx.initialized = true;
    ESP_LOGD(TAG, "TWDT initialized with timeout=%u ms", (unsigned)config->timeout_ms);
    return ESP_OK;
}

esp_err_t esp_task_wdt_deinit(void) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s_twdt_ctx.initialized = false;
    ESP_LOGD(TAG, "TWDT deinitialized");
    return ESP_OK;
}

esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    s_twdt_ctx.config = *config;
    return ESP_OK;
}

esp_err_t esp_task_wdt_add(TaskHandle_t task_handle) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (task_handle == NULL) {
        task_handle = xTaskGetCurrentTaskHandle();
    }
    if (task_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int free_idx = -1;
    for (int i = 0; i < TWDT_MAX_TASKS; i++) {
        if (s_twdt_ctx.tasks[i].in_use) {
            if (s_twdt_ctx.tasks[i].handle == task_handle) {
                return ESP_ERR_INVALID_ARG; // Already subscribed
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    if (free_idx < 0) {
        return ESP_ERR_NO_MEM;
    }

    s_twdt_ctx.tasks[free_idx].in_use = true;
    s_twdt_ctx.tasks[free_idx].handle = task_handle;
    s_twdt_ctx.tasks[free_idx].last_reset_tick = xTaskGetTickCount();
    return ESP_OK;
}

esp_err_t esp_task_wdt_reset(void) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    if (cur == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    for (int i = 0; i < TWDT_MAX_TASKS; i++) {
        if (s_twdt_ctx.tasks[i].in_use && s_twdt_ctx.tasks[i].handle == cur) {
            s_twdt_ctx.tasks[i].last_reset_tick = xTaskGetTickCount();
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_task_wdt_status(TaskHandle_t task_handle) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (task_handle == NULL) {
        task_handle = xTaskGetCurrentTaskHandle();
    }
    if (task_handle == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    for (int i = 0; i < TWDT_MAX_TASKS; i++) {
        if (s_twdt_ctx.tasks[i].in_use && s_twdt_ctx.tasks[i].handle == task_handle) {
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_task_wdt_delete(TaskHandle_t task_handle) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (task_handle == NULL) {
        task_handle = xTaskGetCurrentTaskHandle();
    }
    if (task_handle == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    for (int i = 0; i < TWDT_MAX_TASKS; i++) {
        if (s_twdt_ctx.tasks[i].in_use && s_twdt_ctx.tasks[i].handle == task_handle) {
            s_twdt_ctx.tasks[i].in_use = false;
            s_twdt_ctx.tasks[i].handle = NULL;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_task_wdt_add_user(const char *user_name, esp_task_wdt_user_handle_t *user_handle_ret) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!user_name || !user_handle_ret) {
        return ESP_ERR_INVALID_ARG;
    }

    int free_idx = -1;
    for (int i = 0; i < TWDT_MAX_USERS; i++) {
        if (s_twdt_ctx.users[i].in_use) {
            if (strcmp(s_twdt_ctx.users[i].name, user_name) == 0) {
                return ESP_ERR_INVALID_ARG; // Already exists
            }
        } else if (free_idx < 0) {
            free_idx = i;
        }
    }

    if (free_idx < 0) {
        return ESP_ERR_NO_MEM;
    }

    s_twdt_ctx.users[free_idx].in_use = true;
    strncpy(s_twdt_ctx.users[free_idx].name, user_name, sizeof(s_twdt_ctx.users[free_idx].name) - 1);
    s_twdt_ctx.users[free_idx].name[sizeof(s_twdt_ctx.users[free_idx].name) - 1] = '\0';
    s_twdt_ctx.users[free_idx].last_reset_tick = xTaskGetTickCount();

    *user_handle_ret = &s_twdt_ctx.users[free_idx];
    return ESP_OK;
}

esp_err_t esp_task_wdt_reset_user(esp_task_wdt_user_handle_t user_handle) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!user_handle || !user_handle->in_use) {
        return ESP_ERR_NOT_FOUND;
    }
    user_handle->last_reset_tick = xTaskGetTickCount();
    return ESP_OK;
}

esp_err_t esp_task_wdt_delete_user(esp_task_wdt_user_handle_t user_handle) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!user_handle || !user_handle->in_use) {
        return ESP_ERR_NOT_FOUND;
    }
    user_handle->in_use = false;
    return ESP_OK;
}

void esp_task_wdt_isr_user_handler(void) {
    // Stub for user ISR hook
}

esp_err_t esp_task_wdt_print_triggered_tasks(task_wdt_msg_handler msg_handler, void *opaque, int *cpus_fail) {
    (void)msg_handler;
    (void)opaque;
    if (cpus_fail) {
        *cpus_fail = 0;
    }
    return ESP_OK;
}
