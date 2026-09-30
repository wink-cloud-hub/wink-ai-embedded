/* SPDX-License-Identifier: LGPL-3.0-only */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos_sync.h"
#include "esp_log.h"
#include "pal_log.h"
#include "pal_osal.h"
#include "../core/esp_sim_handle.h"
#include "wink_sim_scheduler.h"

#define TAG "FREERTOS_TIMER"

#ifndef FREERTOS_MAX_TIMERS
#  ifdef CONFIG_FREERTOS_MAX_TIMERS
#    define FREERTOS_MAX_TIMERS CONFIG_FREERTOS_MAX_TIMERS
#  else
#    define FREERTOS_MAX_TIMERS 10
#  endif
#endif

_Static_assert(FREERTOS_MAX_TIMERS <= 64, "FREERTOS_MAX_TIMERS must not exceed 64 (handle encoding limit)");

#ifndef FREERTOS_TIMER_QUEUE_LENGTH
#  ifdef CONFIG_FREERTOS_TIMER_QUEUE_LENGTH
#    define FREERTOS_TIMER_QUEUE_LENGTH CONFIG_FREERTOS_TIMER_QUEUE_LENGTH
#  else
#    define FREERTOS_TIMER_QUEUE_LENGTH 8
#  endif
#endif

typedef enum {
    TIMER_CMD_START = 1,
    TIMER_CMD_STOP,
    TIMER_CMD_CHANGE_PERIOD,
    TIMER_CMD_DELETE,
    TIMER_CMD_RESET,
    TIMER_CMD_POST_WORK_ITEM,
} timer_cmd_type_t;

typedef struct {
    timer_cmd_type_t    type;
    uint32_t            slot_id;
    uint32_t            generation;
    TickType_t          period_ticks;
    TickType_t          delay_ticks;
    esp_timer_work_fn_t work_fn;
    void               *work_arg;
} timer_cmd_msg_t;

typedef struct {
    bool                     used;
    bool                     active;
    bool                     auto_reload;
    bool                     is_work_item;
    uint32_t                 slot_id;
    uint32_t                 token;          /* handle token issued by esp_sim_handle_issue */
    uint32_t                 generation;     /* global monotonic generation */
    char                     name[16];
    TickType_t               period_ticks;
    TickType_t               expiry_ticks;   /* absolute tick count */
    void                    *timer_id;
    TimerCallbackFunction_t  callback;
    esp_timer_work_fn_t      work_fn;
    void                    *work_arg;
} esp_sim_timer_t;

static esp_sim_timer_t s_timers[FREERTOS_MAX_TIMERS];
static uint32_t s_global_timer_gen = 1000u;

/* Command Queue (Ring Buffer) */
static timer_cmd_msg_t s_cmd_queue[FREERTOS_TIMER_QUEUE_LENGTH];
static uint32_t s_cmd_read_idx = 0;
static uint32_t s_cmd_write_idx = 0;
static uint32_t s_cmd_count = 0;
static uint32_t s_cmd_tx_waiter = SIM_SCHED_NO_READY;

/* Daemon Task state */
static bool s_daemon_initialized = false;
static bool s_daemon_running = false;
static bool s_daemon_waiting = false;
static uint32_t s_daemon_sim_id = SIM_SCHED_NO_READY;

static void sys_timer_daemon_task(void *arg);
static void prvExecuteCommand(const timer_cmd_msg_t *msg);
static void prvProcessExpiredTimers(void);

static void prvEnsureDaemonStarted(void) {
    if (s_daemon_initialized) {
        return;
    }
    s_daemon_initialized = true;
    s_daemon_running = true;
    s_cmd_read_idx = 0;
    s_cmd_write_idx = 0;
    s_cmd_count = 0;
    s_cmd_tx_waiter = SIM_SCHED_NO_READY;
    s_daemon_waiting = false;

    wink_status_t st = sim_scheduler_register(
        sys_timer_daemon_task,
        NULL,
        "sys_timer",
        1,          /* Priority 1 (above idle) */
        0,          /* Core 0 */
        32768,      /* Stack depth */
        &s_daemon_sim_id
    );
    if (st == WINK_OK) {
        esp_freertos_register_task_slot(s_daemon_sim_id, 1, "sys_timer");
    } else {
        ESP_LOGE(TAG, "Failed to register sys_timer daemon task (status=%d)", st);
        s_daemon_initialized = false;
        s_daemon_running = false;
        s_daemon_sim_id = SIM_SCHED_NO_READY;
    }
}

static inline esp_sim_timer_t* resolve_timer(TimerHandle_t xTimer) {
    if (xTimer == NULL) {
        return NULL;
    }
    uint32_t slot;
    if (!esp_sim_handle_decode(xTimer, ESP_SIM_HANDLE_TIMER, FREERTOS_MAX_TIMERS, &slot)) {
        return NULL;
    }
    esp_sim_timer_t *t = &s_timers[slot];
    if (!t->used || t->token != (uint32_t)(uintptr_t)xTimer) {
        return NULL;
    }
    return t;
}

static BaseType_t prvReceiveTimerCommand(timer_cmd_msg_t *msg, TickType_t wait_ticks) {
    if (s_cmd_count > 0) {
        *msg = s_cmd_queue[s_cmd_read_idx];
        s_cmd_read_idx = (s_cmd_read_idx + 1) % FREERTOS_TIMER_QUEUE_LENGTH;
        s_cmd_count--;
        if (s_cmd_tx_waiter != SIM_SCHED_NO_READY) {
            uint32_t waiter = s_cmd_tx_waiter;
            s_cmd_tx_waiter = SIM_SCHED_NO_READY;
            sim_scheduler_resume(waiter);
        }
        return pdPASS;
    }

    if (wait_ticks == 0) {
        return pdFAIL;
    }

    s_daemon_waiting = true;
    (void)sync_block(FREERTOS_MAKE_RES_ID(FREERTOS_TAG_TIMER, 0), wait_ticks);
    s_daemon_waiting = false;

    if (!s_daemon_running) {
        return pdFAIL;
    }

    if (s_cmd_count > 0) {
        *msg = s_cmd_queue[s_cmd_read_idx];
        s_cmd_read_idx = (s_cmd_read_idx + 1) % FREERTOS_TIMER_QUEUE_LENGTH;
        s_cmd_count--;
        if (s_cmd_tx_waiter != SIM_SCHED_NO_READY) {
            uint32_t waiter = s_cmd_tx_waiter;
            s_cmd_tx_waiter = SIM_SCHED_NO_READY;
            sim_scheduler_resume(waiter);
        }
        return pdPASS;
    }

    return pdFAIL;
}

static BaseType_t prvSendTimerCommand(const timer_cmd_msg_t *msg, TickType_t xTicksToWait) {
    esp_freertos_assert_not_in_critical("xTimerCommand");
    prvEnsureDaemonStarted();

    /* If called from within the Daemon itself (e.g., callback), execute immediately to avoid deadlock */
    if (sim_scheduler_current_id() == s_daemon_sim_id) {
        prvExecuteCommand(msg);
        return pdPASS;
    }

    TickType_t remaining = xTicksToWait;
    uint32_t self = sim_scheduler_current_id();

    while (s_cmd_count >= FREERTOS_TIMER_QUEUE_LENGTH) {
        if (remaining == 0 || self == SIM_SCHED_NO_READY) {
            return pdFAIL;
        }
        s_cmd_tx_waiter = self;
        uint64_t before_us = pal_os_get_us();
        bool ok = sync_block(FREERTOS_MAKE_RES_ID(FREERTOS_TAG_TIMER, 1), remaining);
        if (s_cmd_tx_waiter == self) {
            s_cmd_tx_waiter = SIM_SCHED_NO_READY;
        }
        if (!ok && s_cmd_count >= FREERTOS_TIMER_QUEUE_LENGTH) {
            return pdFAIL;
        }
        if (remaining != portMAX_DELAY) {
            uint64_t elapsed_us = pal_os_get_us() - before_us;
            TickType_t elapsed_ticks = (TickType_t)(elapsed_us / (portTICK_PERIOD_MS * 1000ULL));
            remaining = (elapsed_ticks >= remaining) ? 0 : (remaining - elapsed_ticks);
        }
    }

    s_cmd_queue[s_cmd_write_idx] = *msg;
    s_cmd_write_idx = (s_cmd_write_idx + 1) % FREERTOS_TIMER_QUEUE_LENGTH;
    s_cmd_count++;

    if (s_daemon_waiting && s_daemon_sim_id != SIM_SCHED_NO_READY) {
        sim_scheduler_resume(s_daemon_sim_id);
    }

    return pdPASS;
}

static void prvExecuteCommand(const timer_cmd_msg_t *msg) {
    if (!msg || msg->slot_id >= FREERTOS_MAX_TIMERS) {
        return;
    }
    esp_sim_timer_t *t = &s_timers[msg->slot_id];
    if (!t->used || t->generation != msg->generation) {
        /* Stale command or reused slot with different generation (Anti-ABA drop) */
        return;
    }

    TickType_t now_ticks = xTaskGetTickCount();

    switch (msg->type) {
    case TIMER_CMD_START:
    case TIMER_CMD_RESET:
        t->active = true;
        t->expiry_ticks = now_ticks + t->period_ticks;
        break;

    case TIMER_CMD_STOP:
        t->active = false;
        break;

    case TIMER_CMD_CHANGE_PERIOD:
        t->period_ticks = msg->period_ticks;
        t->active = true;
        t->expiry_ticks = now_ticks + msg->period_ticks;
        break;

    case TIMER_CMD_DELETE:
        t->used = false;
        t->active = false;
        t->token = 0;
        t->generation = ++s_global_timer_gen;
        break;

    case TIMER_CMD_POST_WORK_ITEM:
        t->active = true;
        t->expiry_ticks = now_ticks + msg->delay_ticks;
        break;

    default:
        break;
    }
}

static void prvProcessExpiredTimers(void) {
    TickType_t now_ticks = xTaskGetTickCount();

    for (uint32_t i = 0; i < FREERTOS_MAX_TIMERS; i++) {
        esp_sim_timer_t *t = &s_timers[i];
        if (t->used && t->active && t->expiry_ticks <= now_ticks) {
            uint32_t expected_gen = t->generation;
            TimerHandle_t handle = (TimerHandle_t)(uintptr_t)t->token;
            TimerCallbackFunction_t cb = t->callback;
            esp_timer_work_fn_t work_fn = t->work_fn;
            void *work_arg = t->work_arg;
            bool is_work = t->is_work_item;
            bool auto_reload = t->auto_reload;
            TickType_t period = t->period_ticks;

            if (auto_reload && !is_work) {
                t->expiry_ticks = now_ticks + period;
            } else {
                t->active = false;
                if (is_work) {
                    t->used = false;
                    t->token = 0;
                    t->generation = ++s_global_timer_gen;
                }
            }

            /* Dispatch callback without holding locks */
            if (is_work) {
                if (work_fn) {
                    work_fn(work_arg, expected_gen);
                }
            } else {
                if (cb) {
                    cb(handle);
                }
            }
        }
    }
}

static void sys_timer_daemon_task(void *arg) {
    (void)arg;
    timer_cmd_msg_t msg;

    while (s_daemon_running) {
        TickType_t now_ticks = xTaskGetTickCount();
        TickType_t min_expiry = portMAX_DELAY;

        for (uint32_t i = 0; i < FREERTOS_MAX_TIMERS; i++) {
            if (s_timers[i].used && s_timers[i].active) {
                if (s_timers[i].expiry_ticks <= now_ticks) {
                    min_expiry = now_ticks;
                    break;
                } else if (s_timers[i].expiry_ticks < min_expiry) {
                    min_expiry = s_timers[i].expiry_ticks;
                }
            }
        }

        TickType_t wait_ticks = portMAX_DELAY;
        if (min_expiry != portMAX_DELAY) {
            wait_ticks = (min_expiry > now_ticks) ? (min_expiry - now_ticks) : 0;
        }

        BaseType_t rc = prvReceiveTimerCommand(&msg, wait_ticks);
        if (!s_daemon_running) {
            break;
        }

        if (rc == pdPASS) {
            prvExecuteCommand(&msg);
            while (s_cmd_count > 0 && prvReceiveTimerCommand(&msg, 0) == pdPASS) {
                prvExecuteCommand(&msg);
            }
        }

        if (!s_daemon_running) {
            break;
        }

        prvProcessExpiredTimers();
    }

    vTaskDelete(NULL);
}

/* ── FreeRTOS Public Timer APIs ───────────────────────────────────────────── */

TimerHandle_t xTimerCreate(const char *const pcTimerName,
                           const TickType_t xTimerPeriodInTicks,
                           const UBaseType_t uxAutoReload,
                           void *const pvTimerID,
                           TimerCallbackFunction_t pxCallbackFunction) {
    if (xTimerPeriodInTicks == 0 || pxCallbackFunction == NULL) {
        return NULL;
    }

    prvEnsureDaemonStarted();

    int slot = -1;
    for (uint32_t i = 0; i < FREERTOS_MAX_TIMERS; i++) {
        if (!s_timers[i].used) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        ESP_LOGW(TAG, "No free timer slots available (max=%d)", FREERTOS_MAX_TIMERS);
        return NULL;
    }

    uint32_t token = esp_sim_handle_issue(ESP_SIM_HANDLE_TIMER, (uint32_t)slot);
    if (token == 0) {
        return NULL;
    }

    esp_sim_timer_t *t = &s_timers[slot];
    memset(t, 0, sizeof(*t));
    t->used = true;
    t->active = false;
    t->auto_reload = (uxAutoReload != pdFALSE);
    t->is_work_item = false;
    t->slot_id = (uint32_t)slot;
    t->token = token;
    t->generation = ++s_global_timer_gen;
    t->period_ticks = xTimerPeriodInTicks;
    t->timer_id = pvTimerID;
    t->callback = pxCallbackFunction;

    if (pcTimerName) {
        strncpy(t->name, pcTimerName, sizeof(t->name) - 1);
        t->name[sizeof(t->name) - 1] = '\0';
    } else {
        strncpy(t->name, "timer", sizeof(t->name) - 1);
    }

    return (TimerHandle_t)(uintptr_t)token;
}

BaseType_t xTimerStart(TimerHandle_t xTimer, const TickType_t xTicksToWait) {
    esp_sim_timer_t *t = resolve_timer(xTimer);
    if (!t) {
        return pdFAIL;
    }

    timer_cmd_msg_t msg = {
        .type = TIMER_CMD_START,
        .slot_id = t->slot_id,
        .generation = t->generation,
        .period_ticks = t->period_ticks,
    };

    return prvSendTimerCommand(&msg, xTicksToWait);
}

BaseType_t xTimerStop(TimerHandle_t xTimer, const TickType_t xTicksToWait) {
    esp_sim_timer_t *t = resolve_timer(xTimer);
    if (!t) {
        return pdFAIL;
    }

    uint32_t prev_gen = t->generation;
    uint32_t next_gen = ++s_global_timer_gen;
    t->generation = next_gen;

    timer_cmd_msg_t msg = {
        .type = TIMER_CMD_STOP,
        .slot_id = t->slot_id,
        .generation = next_gen,
    };

    BaseType_t ret = prvSendTimerCommand(&msg, xTicksToWait);
    if (ret != pdPASS) {
        t->generation = prev_gen;
    }
    return ret;
}

BaseType_t xTimerChangePeriod(TimerHandle_t xTimer, const TickType_t xNewPeriod, const TickType_t xTicksToWait) {
    if (xNewPeriod == 0) {
        return pdFAIL;
    }
    esp_sim_timer_t *t = resolve_timer(xTimer);
    if (!t) {
        return pdFAIL;
    }

    uint32_t prev_gen = t->generation;
    uint32_t next_gen = ++s_global_timer_gen;
    t->generation = next_gen;

    timer_cmd_msg_t msg = {
        .type = TIMER_CMD_CHANGE_PERIOD,
        .slot_id = t->slot_id,
        .generation = next_gen,
        .period_ticks = xNewPeriod,
    };

    BaseType_t ret = prvSendTimerCommand(&msg, xTicksToWait);
    if (ret != pdPASS) {
        t->generation = prev_gen;
    }
    return ret;
}

BaseType_t xTimerDelete(TimerHandle_t xTimer, const TickType_t xTicksToWait) {
    esp_sim_timer_t *t = resolve_timer(xTimer);
    if (!t) {
        return pdFAIL;
    }

    uint32_t slot = t->slot_id;
    uint32_t gen = t->generation;

    timer_cmd_msg_t msg = {
        .type = TIMER_CMD_DELETE,
        .slot_id = slot,
        .generation = gen,
    };

    BaseType_t ret = prvSendTimerCommand(&msg, xTicksToWait);
    if (ret == pdPASS) {
        /* Mark unused only once command is successfully enqueued */
        t->used = false;
        t->active = false;
        t->token = 0;
        t->generation = ++s_global_timer_gen;
    }
    return ret;
}

BaseType_t xTimerReset(TimerHandle_t xTimer, const TickType_t xTicksToWait) {
    esp_sim_timer_t *t = resolve_timer(xTimer);
    if (!t) {
        return pdFAIL;
    }

    timer_cmd_msg_t msg = {
        .type = TIMER_CMD_RESET,
        .slot_id = t->slot_id,
        .generation = t->generation,
        .period_ticks = t->period_ticks,
    };

    return prvSendTimerCommand(&msg, xTicksToWait);
}

BaseType_t xTimerIsTimerActive(TimerHandle_t xTimer) {
    esp_sim_timer_t *t = resolve_timer(xTimer);
    if (!t) {
        return pdFALSE;
    }
    return t->active ? pdTRUE : pdFALSE;
}

void *pvTimerGetTimerID(const TimerHandle_t xTimer) {
    esp_sim_timer_t *t = resolve_timer(xTimer);
    if (!t) {
        return NULL;
    }
    return t->timer_id;
}

BaseType_t xTimerStartFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken) {
    (void)xTimer;
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    ESP_LOGE(TAG, "xTimerStartFromISR not supported (no hardware ISR thread in simulation)");
    return pdFAIL;
}

BaseType_t xTimerStopFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken) {
    (void)xTimer;
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    ESP_LOGE(TAG, "xTimerStopFromISR not supported in simulation");
    return pdFAIL;
}

BaseType_t xTimerResetFromISR(TimerHandle_t xTimer, BaseType_t *pxHigherPriorityTaskWoken) {
    (void)xTimer;
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    ESP_LOGE(TAG, "xTimerResetFromISR not supported in simulation");
    return pdFAIL;
}

BaseType_t xTimerChangePeriodFromISR(TimerHandle_t xTimer, const TickType_t xNewPeriod, BaseType_t *pxHigherPriorityTaskWoken) {
    (void)xTimer;
    (void)xNewPeriod;
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    ESP_LOGE(TAG, "xTimerChangePeriodFromISR not supported in simulation");
    return pdFAIL;
}

/* ── Work Item Extension for Wi-Fi / MQTT ─────────────────────────────────── */

BaseType_t esp_freertos_timer_post_work_item(esp_timer_work_fn_t fn, void *arg, uint32_t *out_token, TickType_t delay_ticks) {
    if (!fn) {
        return pdFAIL;
    }
    prvEnsureDaemonStarted();

    int slot = -1;
    for (uint32_t i = 0; i < FREERTOS_MAX_TIMERS; i++) {
        if (!s_timers[i].used) {
            slot = (int)i;
            break;
        }
    }
    if (slot < 0) {
        ESP_LOGW(TAG, "No free slot for timer work item");
        return pdFAIL;
    }

    uint32_t gen = ++s_global_timer_gen;
    esp_sim_timer_t *t = &s_timers[slot];
    memset(t, 0, sizeof(*t));
    t->used = true;
    t->active = false;
    t->is_work_item = true;
    t->slot_id = (uint32_t)slot;
    t->generation = gen;
    t->token = gen;
    t->work_fn = fn;
    t->work_arg = arg;
    strncpy(t->name, "work_item", sizeof(t->name) - 1);

    if (out_token) {
        *out_token = gen;
    }

    timer_cmd_msg_t msg = {
        .type = TIMER_CMD_POST_WORK_ITEM,
        .slot_id = (uint32_t)slot,
        .generation = gen,
        .delay_ticks = delay_ticks,
        .work_fn = fn,
        .work_arg = arg,
    };

    BaseType_t ret = prvSendTimerCommand(&msg, 0);
    if (ret != pdPASS) {
        /* Rollback slot reservation on enqueue failure to prevent leaking timer slots */
        memset(t, 0, sizeof(*t));
        t->used = false;
        t->generation = ++s_global_timer_gen;
        if (out_token) {
            *out_token = 0;
        }
    }
    return ret;
}

BaseType_t esp_freertos_timer_cancel_work_item(uint32_t token) {
    if (token == 0) {
        return pdFAIL;
    }
    for (uint32_t i = 0; i < FREERTOS_MAX_TIMERS; i++) {
        esp_sim_timer_t *t = &s_timers[i];
        if (t->used && t->is_work_item && t->generation == token) {
            t->active = false;
            t->used = false;
            t->generation = ++s_global_timer_gen;
            return pdPASS;
        }
    }
    return pdFAIL;
}

/* ── Reset and Observation Hooks ─────────────────────────────────────────── */

void esp_freertos_timers_sim_reset(void) {
    s_daemon_running = false;
    if (s_daemon_waiting && s_daemon_sim_id != SIM_SCHED_NO_READY) {
        sim_scheduler_resume(s_daemon_sim_id);
    }
    s_daemon_waiting = false;
    s_daemon_sim_id = SIM_SCHED_NO_READY;
    s_daemon_initialized = false;
    s_cmd_read_idx = 0;
    s_cmd_write_idx = 0;
    s_cmd_count = 0;
    s_cmd_tx_waiter = SIM_SCHED_NO_READY;

    for (uint32_t i = 0; i < FREERTOS_MAX_TIMERS; i++) {
        s_timers[i].used = false;
        s_timers[i].active = false;
        s_timers[i].token = 0;
        s_timers[i].generation = ++s_global_timer_gen;
    }
}

uint32_t esp_freertos_get_active_timer_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < FREERTOS_MAX_TIMERS; i++) {
        if (s_timers[i].used && s_timers[i].active) {
            count++;
        }
    }
    return count;
}

uint32_t esp_freertos_get_timer_daemon_sim_id(void) {
    return s_daemon_sim_id;
}
