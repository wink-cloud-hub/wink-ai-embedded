/* SPDX-License-Identifier: LGPL-3.0-only */
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos_sync.h"
#include "pal_osal.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "esp_task_wdt";

#define TWDT_MAX_TASKS (16u)
#define TWDT_MAX_USERS (16u)
#define TWDT_NAME_BYTES (32u)
#define TWDT_MESSAGE_BYTES (128u)
#define TWDT_US_PER_MS (1000u)
/* The cooperative facade currently schedules CPU 0. */
#define TWDT_SIM_CPU_MASK (1u)
#define TWDT_USER_SEQUENCE_SHIFT (11u)
#define TWDT_USER_MAX_SEQUENCE (UINT32_MAX >> TWDT_USER_SEQUENCE_SHIFT)

struct esp_task_wdt_user_handle_s {
    char name[TWDT_NAME_BYTES];
    bool in_use;
    bool has_reset;
    uint32_t token;
};

typedef struct {
    TaskHandle_t handle;
    bool in_use;
    bool has_reset;
} twdt_task_entry_t;

typedef struct {
    bool initialized;
    esp_task_wdt_config_t config;
    twdt_task_entry_t tasks[TWDT_MAX_TASKS];
    struct esp_task_wdt_user_handle_s users[TWDT_MAX_USERS];
    uint64_t deadline_us;
    uint32_t work_token;
} twdt_ctx_t;

static twdt_ctx_t s_twdt_ctx;
/* Reserved kind-zero opaque tokens cannot alias other SDK handle kinds.
 * Never reset this sequence: slot reuse and reset must reject ABA. */
static uint32_t s_user_sequence;
extern void esp_sim_request_task_wdt_reset(void);

static void twdt_timeout(void *arg, uint32_t token);

static struct esp_task_wdt_user_handle_s *twdt_resolve_user(
    esp_task_wdt_user_handle_t handle) {
    uintptr_t raw = (uintptr_t)handle;
    if (raw == 0 || raw > UINT32_MAX) return NULL;
    for (uint32_t i = 0; i < TWDT_MAX_USERS; ++i) {
        if (s_twdt_ctx.users[i].in_use && s_twdt_ctx.users[i].token == raw) {
            return &s_twdt_ctx.users[i];
        }
    }
    return NULL;
}

static bool twdt_has_entries(void) {
    for (uint32_t i = 0; i < TWDT_MAX_TASKS; ++i) {
        if (s_twdt_ctx.tasks[i].in_use) return true;
    }
    for (uint32_t i = 0; i < TWDT_MAX_USERS; ++i) {
        if (s_twdt_ctx.users[i].in_use) return true;
    }
    return false;
}

static bool twdt_all_reset(void) {
    for (uint32_t i = 0; i < TWDT_MAX_TASKS; ++i) {
        if (s_twdt_ctx.tasks[i].in_use &&
            !s_twdt_ctx.tasks[i].has_reset) return false;
    }
    for (uint32_t i = 0; i < TWDT_MAX_USERS; ++i) {
        if (s_twdt_ctx.users[i].in_use &&
            !s_twdt_ctx.users[i].has_reset) return false;
    }
    return true;
}

static void twdt_stop(void) {
    if (s_twdt_ctx.work_token != 0) {
        (void)esp_freertos_timer_cancel_work_item(s_twdt_ctx.work_token);
        s_twdt_ctx.work_token = 0;
    }
}

static esp_err_t twdt_arm(uint64_t deadline_us) {
    const uint64_t tick_us = (uint64_t)portTICK_PERIOD_MS * TWDT_US_PER_MS;
    uint64_t now = pal_os_get_us();
    uint64_t remaining = deadline_us > now ? deadline_us - now : tick_us;
    TickType_t ticks = (TickType_t)((remaining + tick_us - 1u) / tick_us);
    uint32_t token = 0;
    /* Reserve before cancelling: failed allocation keeps the prior deadline. */
    if (esp_freertos_timer_post_work_item(twdt_timeout, NULL, &token,
                                         ticks) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    twdt_stop();
    s_twdt_ctx.work_token = token;
    s_twdt_ctx.deadline_us = deadline_us;
    return ESP_OK;
}

static esp_err_t twdt_feed(void) {
    uint64_t interval_us =
        (uint64_t)s_twdt_ctx.config.timeout_ms * TWDT_US_PER_MS;
    esp_err_t status = twdt_arm(pal_os_get_us() + interval_us);
    if (status != ESP_OK) return status;
    for (uint32_t i = 0; i < TWDT_MAX_TASKS; ++i) {
        s_twdt_ctx.tasks[i].has_reset = false;
    }
    for (uint32_t i = 0; i < TWDT_MAX_USERS; ++i) {
        s_twdt_ctx.users[i].has_reset = false;
    }
    return ESP_OK;
}

static esp_err_t twdt_update_subscriptions(void) {
    if (!twdt_has_entries()) {
        twdt_stop();
        return ESP_OK;
    }
    if (s_twdt_ctx.work_token == 0 || twdt_all_reset()) return twdt_feed();
    return ESP_OK;
}

static void twdt_timeout(void *arg, uint32_t token) {
    (void)arg;
    if (!s_twdt_ctx.initialized || token != s_twdt_ctx.work_token) return;
    s_twdt_ctx.work_token = 0;
    if (pal_os_get_us() < s_twdt_ctx.deadline_us) {
        if (twdt_arm(s_twdt_ctx.deadline_us) != ESP_OK) {
            ESP_LOGE(TAG, "TWDT timer rearm failed");
        }
        return;
    }
    (void)esp_task_wdt_print_triggered_tasks(NULL, NULL, NULL);
    /* The public hook executes with ISR restrictions, while reporting/rearming
     * remain in the timer service task. No CPU preemption is modeled here. */
    bool was_in_isr = pal_os_in_isr();
    pal_os_set_sim_isr_context(true);
    esp_task_wdt_isr_user_handler();
    pal_os_set_sim_isr_context(was_in_isr);
    if (s_twdt_ctx.config.trigger_panic) {
        ESP_LOGE(TAG, "TWDT panic -> pending watchdog reset");
        esp_sim_request_task_wdt_reset();
        return;
    }
    uint64_t interval_us =
        (uint64_t)s_twdt_ctx.config.timeout_ms * TWDT_US_PER_MS;
    if (twdt_arm(pal_os_get_us() + interval_us) != ESP_OK) {
        ESP_LOGE(TAG, "TWDT timer rearm failed");
    }
}

esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t *config) {
    if (s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!config || config->timeout_ms == 0 ||
        (config->idle_core_mask & ~TWDT_SIM_CPU_MASK) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(&s_twdt_ctx, 0, sizeof(s_twdt_ctx));
    s_twdt_ctx.config = *config;
    s_twdt_ctx.initialized = true;
    if (config->idle_core_mask != 0) {
        ESP_LOGW(TAG, "Idle-core TWDT monitoring is unavailable in "
                      "cooperative simulation");
    }
    ESP_LOGD(TAG, "TWDT initialized with timeout=%u ms",
             (unsigned)config->timeout_ms);
    return ESP_OK;
}

esp_err_t esp_task_wdt_deinit(void) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (twdt_has_entries()) return ESP_ERR_INVALID_STATE;
    twdt_stop();
    s_twdt_ctx.initialized = false;
    ESP_LOGD(TAG, "TWDT deinitialized");
    return ESP_OK;
}

esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t *config) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!config || config->timeout_ms == 0 ||
        (config->idle_core_mask & ~TWDT_SIM_CPU_MASK) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_task_wdt_config_t previous = s_twdt_ctx.config;
    s_twdt_ctx.config = *config;
    esp_err_t status = twdt_has_entries() ? twdt_feed() : ESP_OK;
    if (status != ESP_OK) s_twdt_ctx.config = previous;
    return status;
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
    if (esp_freertos_resolve_handle(task_handle) == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int free_idx = -1;
    for (uint32_t i = 0; i < TWDT_MAX_TASKS; i++) {
        if (s_twdt_ctx.tasks[i].in_use) {
            if (s_twdt_ctx.tasks[i].handle == task_handle) {
                return ESP_ERR_INVALID_ARG; // Already subscribed
            }
        } else if (free_idx < 0) {
            free_idx = (int)i;
        }
    }

    if (free_idx < 0) {
        return ESP_ERR_NO_MEM;
    }

    bool other_entries_reset = twdt_all_reset();
    s_twdt_ctx.tasks[free_idx].in_use = true;
    s_twdt_ctx.tasks[free_idx].handle = task_handle;
    s_twdt_ctx.tasks[free_idx].has_reset = false;
    esp_err_t status = other_entries_reset ? twdt_feed() :
                                            twdt_update_subscriptions();
    if (status != ESP_OK) {
        memset(&s_twdt_ctx.tasks[free_idx], 0,
               sizeof(s_twdt_ctx.tasks[free_idx]));
    }
    return status;
}

esp_err_t esp_task_wdt_reset(void) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    TaskHandle_t cur = xTaskGetCurrentTaskHandle();
    if (cur == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    for (uint32_t i = 0; i < TWDT_MAX_TASKS; i++) {
        if (s_twdt_ctx.tasks[i].in_use && s_twdt_ctx.tasks[i].handle == cur) {
            s_twdt_ctx.tasks[i].has_reset = true;
            return twdt_all_reset() ? twdt_feed() : ESP_OK;
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

    for (uint32_t i = 0; i < TWDT_MAX_TASKS; i++) {
        if (s_twdt_ctx.tasks[i].in_use &&
            s_twdt_ctx.tasks[i].handle == task_handle) {
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

    for (uint32_t i = 0; i < TWDT_MAX_TASKS; i++) {
        if (s_twdt_ctx.tasks[i].in_use &&
            s_twdt_ctx.tasks[i].handle == task_handle) {
            s_twdt_ctx.tasks[i].in_use = false;
            s_twdt_ctx.tasks[i].handle = NULL;
            return twdt_update_subscriptions();
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_task_wdt_add_user(
    const char *user_name, esp_task_wdt_user_handle_t *user_handle_ret) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!user_name || !user_handle_ret ||
        strlen(user_name) >= TWDT_NAME_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }

    int free_idx = -1;
    for (uint32_t i = 0; i < TWDT_MAX_USERS; i++) {
        if (s_twdt_ctx.users[i].in_use) {
            if (strcmp(s_twdt_ctx.users[i].name, user_name) == 0) {
                return ESP_ERR_INVALID_ARG; // Already exists
            }
        } else if (free_idx < 0) {
            free_idx = (int)i;
        }
    }

    if (free_idx < 0) {
        return ESP_ERR_NO_MEM;
    }
    if (s_user_sequence >= TWDT_USER_MAX_SEQUENCE) return ESP_ERR_NO_MEM;
    bool other_entries_reset = twdt_all_reset();
    s_twdt_ctx.users[free_idx].in_use = true;
    s_twdt_ctx.users[free_idx].token =
        (++s_user_sequence << TWDT_USER_SEQUENCE_SHIFT) |
        ((uint32_t)free_idx << 1u) | 1u;
    snprintf(s_twdt_ctx.users[free_idx].name, TWDT_NAME_BYTES,
             "%s", user_name);
    s_twdt_ctx.users[free_idx].has_reset = false;
    esp_err_t status = other_entries_reset ? twdt_feed() :
                                            twdt_update_subscriptions();
    if (status != ESP_OK) {
        memset(&s_twdt_ctx.users[free_idx], 0,
               sizeof(s_twdt_ctx.users[free_idx]));
        return status;
    }
    *user_handle_ret = (esp_task_wdt_user_handle_t)(uintptr_t)
        s_twdt_ctx.users[free_idx].token;
    return ESP_OK;
}

esp_err_t esp_task_wdt_reset_user(esp_task_wdt_user_handle_t user_handle) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    struct esp_task_wdt_user_handle_s *user = twdt_resolve_user(user_handle);
    if (user == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    user->has_reset = true;
    return twdt_all_reset() ? twdt_feed() : ESP_OK;
}

esp_err_t esp_task_wdt_delete_user(esp_task_wdt_user_handle_t user_handle) {
    if (!s_twdt_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    struct esp_task_wdt_user_handle_s *user = twdt_resolve_user(user_handle);
    if (user == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    user->in_use = false;
    return twdt_update_subscriptions();
}

__attribute__((weak)) void esp_task_wdt_isr_user_handler(void) {
    /* Optional public ISR hook. */
}

esp_err_t esp_task_wdt_print_triggered_tasks(
    task_wdt_msg_handler msg_handler, void *opaque, int *cpus_fail) {
    if (!s_twdt_ctx.initialized) {
        if (cpus_fail) *cpus_fail = 0;
        return ESP_ERR_INVALID_STATE;
    }

    if (cpus_fail) *cpus_fail = 0;
    if (!twdt_has_entries()) return ESP_FAIL;
    const char *caption = "Task watchdog got triggered. The following "
                          "tasks/users did not reset the watchdog in time:";
    if (msg_handler) msg_handler(opaque, caption);
    else ESP_LOGE(TAG, "%s", caption);
    for (uint32_t i = 0; i < TWDT_MAX_TASKS; i++) {
        if (s_twdt_ctx.tasks[i].in_use && !s_twdt_ctx.tasks[i].has_reset) {
            char buf[TWDT_MESSAGE_BYTES];
            esp_tcb_t *task =
                esp_freertos_resolve_handle(s_twdt_ctx.tasks[i].handle);
            snprintf(buf, sizeof(buf), "did not reset: task '%s' (CPU 0)",
                     task ? task->name : "deleted task");
            if (msg_handler) msg_handler(opaque, buf);
            else ESP_LOGE(TAG, "%s", buf);
            if (cpus_fail) *cpus_fail |= 1;
        }
    }

    for (uint32_t i = 0; i < TWDT_MAX_USERS; i++) {
        if (s_twdt_ctx.users[i].in_use && !s_twdt_ctx.users[i].has_reset) {
            char buf[TWDT_MESSAGE_BYTES];
            snprintf(buf, sizeof(buf), "did not reset: user '%s' (CPU 0)",
                     s_twdt_ctx.users[i].name);
            if (msg_handler) msg_handler(opaque, buf);
            else ESP_LOGE(TAG, "%s", buf);
            if (cpus_fail) *cpus_fail |= 1;
        }
    }

    return ESP_OK;
}

void esp_task_wdt_sim_reset(void) {
    twdt_stop();
    memset(&s_twdt_ctx, 0, sizeof(s_twdt_ctx));
}

