/* SPDX-License-Identifier: LGPL-3.0-only */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos_sync.h"
#include "wink_sim_scheduler.h"
#include "pal_log.h"

#define FREERTOS_MAX_SEMAPHORES 16

typedef enum {
    SEM_TYPE_MUTEX = 0,
    SEM_TYPE_BINARY,
    SEM_TYPE_COUNTING,
} sem_type_t;

typedef struct {
    bool       used;
    sem_type_t type;
    uint32_t   count;
    uint32_t   max_count;
    uint32_t   owner_task_id;
    uint32_t   waiters[WINK_SIM_MAX_TASKS];
    int32_t    waiter_prio[WINK_SIM_MAX_TASKS];
    uint8_t    waiter_count;
} esp_sem_t;

_Static_assert(sizeof(esp_sem_t) <= 128, "esp_sem_t size budget exceeded");

static esp_sem_t s_sems[FREERTOS_MAX_SEMAPHORES];

static inline esp_sem_t* resolve_sem(SemaphoreHandle_t s) {
    if (s == NULL) return NULL;
    esp_sem_t* candidate = (esp_sem_t*)s;
    if (candidate < &s_sems[0] || candidate >= &s_sems[FREERTOS_MAX_SEMAPHORES]) {
        return NULL;
    }
    if (!candidate->used) return NULL;
    return candidate;
}

static uint32_t sem_waiter_pop_highest_prio(esp_sem_t* s) {
    if (s->waiter_count == 0) return SIM_SCHED_NO_READY;
    uint8_t best_idx = 0;
    int32_t best_prio = s->waiter_prio[0];
    for (uint8_t i = 1; i < s->waiter_count; ++i) {
        if (s->waiter_prio[i] > best_prio) {
            best_prio = s->waiter_prio[i];
            best_idx = i;
        }
    }
    uint32_t res = s->waiters[best_idx];
    for (uint8_t j = best_idx; j + 1 < s->waiter_count; ++j) {
        s->waiters[j] = s->waiters[j + 1];
        s->waiter_prio[j] = s->waiter_prio[j + 1];
    }
    s->waiter_count--;
    return res;
}

static void sem_waiter_add(esp_sem_t* s, uint32_t sim_id, int32_t prio) {
    for (uint8_t i = 0; i < s->waiter_count; ++i) {
        if (s->waiters[i] == sim_id) return;
    }
    if (s->waiter_count < WINK_SIM_MAX_TASKS) {
        s->waiters[s->waiter_count] = sim_id;
        s->waiter_prio[s->waiter_count] = prio;
        s->waiter_count++;
    }
}

static void sem_waiter_remove(esp_sem_t* s, uint32_t sim_id) {
    for (uint8_t i = 0; i < s->waiter_count; ++i) {
        if (s->waiters[i] == sim_id) {
            for (uint8_t j = i; j + 1 < s->waiter_count; ++j) {
                s->waiters[j] = s->waiters[j + 1];
                s->waiter_prio[j] = s->waiter_prio[j + 1];
            }
            s->waiter_count--;
            return;
        }
    }
}

void esp_freertos_sem_pool_reset(void) {
    memset(s_sems, 0, sizeof(s_sems));
}

SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    for (uint32_t i = 0; i < FREERTOS_MAX_SEMAPHORES; ++i) {
        if (!s_sems[i].used) {
            memset(&s_sems[i], 0, sizeof(esp_sem_t));
            s_sems[i].used = true;
            s_sems[i].type = SEM_TYPE_MUTEX;
            s_sems[i].count = 1;
            s_sems[i].max_count = 1;
            s_sems[i].owner_task_id = SIM_SCHED_NO_READY;
            return (SemaphoreHandle_t)&s_sems[i];
        }
    }
    pal_log_w("FREERTOS", "No free semaphore slot for mutex");
    return NULL;
}

SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    for (uint32_t i = 0; i < FREERTOS_MAX_SEMAPHORES; ++i) {
        if (!s_sems[i].used) {
            memset(&s_sems[i], 0, sizeof(esp_sem_t));
            s_sems[i].used = true;
            s_sems[i].type = SEM_TYPE_BINARY;
            s_sems[i].count = 0;
            s_sems[i].max_count = 1;
            s_sems[i].owner_task_id = SIM_SCHED_NO_READY;
            return (SemaphoreHandle_t)&s_sems[i];
        }
    }
    pal_log_w("FREERTOS", "No free semaphore slot for binary semaphore");
    return NULL;
}

SemaphoreHandle_t xSemaphoreCreateCounting(const UBaseType_t uxMaxCount, const UBaseType_t uxInitialCount) {
    if (uxMaxCount == 0 || uxInitialCount > uxMaxCount) {
        return NULL;
    }
    for (uint32_t i = 0; i < FREERTOS_MAX_SEMAPHORES; ++i) {
        if (!s_sems[i].used) {
            memset(&s_sems[i], 0, sizeof(esp_sem_t));
            s_sems[i].used = true;
            s_sems[i].type = SEM_TYPE_COUNTING;
            s_sems[i].count = (uint32_t)uxInitialCount;
            s_sems[i].max_count = (uint32_t)uxMaxCount;
            s_sems[i].owner_task_id = SIM_SCHED_NO_READY;
            return (SemaphoreHandle_t)&s_sems[i];
        }
    }
    pal_log_w("FREERTOS", "No free semaphore slot for counting semaphore");
    return NULL;
}

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) {
    pal_log_w("FREERTOS", "Recursive mutex not supported in simulation shim (ADR item 10)");
    return NULL;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) {
    esp_sem_t* s = resolve_sem(xSemaphore);
    if (s == NULL) {
        return pdFALSE;
    }

    uint32_t sem_idx = (uint32_t)(s - s_sems);
    TickType_t remaining = xTicksToWait;

    while (s->count == 0) {
        if (remaining == 0) {
            return pdFALSE;
        }
        uint32_t self = sim_scheduler_current_id();
        if (self == SIM_SCHED_NO_READY) {
            return pdFALSE;
        }

        int32_t prio = esp_freertos_get_task_prio(self);
        sem_waiter_add(s, self, prio);

        uint32_t tag = (s->type == SEM_TYPE_MUTEX) ? FREERTOS_TAG_MUTEX : FREERTOS_TAG_SEM;
        uint64_t before_us = pal_os_get_us();
        bool ok = sync_block(FREERTOS_MAKE_RES_ID(tag, sem_idx), remaining);
        sem_waiter_remove(s, self);

        if (remaining != portMAX_DELAY) {
            uint64_t elapsed_us = pal_os_get_us() - before_us;
            TickType_t elapsed_ticks = (TickType_t)(elapsed_us / (portTICK_PERIOD_MS * 1000ULL));
            remaining = (elapsed_ticks >= remaining) ? 0 : (remaining - elapsed_ticks);
        }

        if (!ok && s->count == 0) {
            return pdFALSE;
        }
    }

    s->count--;
    if (s->type == SEM_TYPE_MUTEX) {
        s->owner_task_id = sim_scheduler_current_id();
    }

    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
    esp_sem_t* s = resolve_sem(xSemaphore);
    if (s == NULL) {
        return pdFALSE;
    }

    if (s->type == SEM_TYPE_MUTEX) {
        uint32_t self = sim_scheduler_current_id();
        if (s->owner_task_id != self) {
            pal_log_w("FREERTOS", "Mutex given by non-owner task (owner=%u, cur=%u)",
                      s->owner_task_id, self);
        }
        s->owner_task_id = SIM_SCHED_NO_READY;
    }

    if (s->count >= s->max_count) {
        return pdFALSE;
    }

    s->count++;

    if (s->waiter_count > 0) {
        uint32_t wake_id = sem_waiter_pop_highest_prio(s);
        if (wake_id != SIM_SCHED_NO_READY) {
            sim_scheduler_resume(wake_id);
        }
    }

    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) {
    esp_sem_t* s = resolve_sem(xSemaphore);
    if (s == NULL) return;

    for (uint8_t i = 0; i < s->waiter_count; ++i) {
        sim_scheduler_resume(s->waiters[i]);
    }
    memset(s, 0, sizeof(esp_sem_t));
}

BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t xSemaphore, BaseType_t * const pxHigherPriorityTaskWoken) {
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return xSemaphoreTake(xSemaphore, 0);
}

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore, BaseType_t * const pxHigherPriorityTaskWoken) {
    if (pxHigherPriorityTaskWoken != NULL) {
        *pxHigherPriorityTaskWoken = pdFALSE;
    }
    return xSemaphoreGive(xSemaphore);
}

UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t xSemaphore) {
    esp_sem_t* s = resolve_sem(xSemaphore);
    return s ? (UBaseType_t)s->count : 0u;
}

BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t xMutex, TickType_t xBlockTime) {
    (void)xMutex;
    (void)xBlockTime;
    pal_log_e("FREERTOS", "xSemaphoreTakeRecursive: not supported (ADR item 10, recursive mutex disabled)");
    return pdFAIL;
}

BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t xMutex) {
    (void)xMutex;
    pal_log_e("FREERTOS", "xSemaphoreGiveRecursive: not supported (ADR item 10, recursive mutex disabled)");
    return pdFAIL;
}
