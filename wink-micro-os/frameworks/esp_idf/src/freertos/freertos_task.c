/* SPDX-License-Identifier: LGPL-3.0-only */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos_sync.h"
#include "wink_sim_scheduler.h"
#include "pal_osal.h"
#include "pal_log.h"
#include "esp_log.h"

static esp_tcb_t s_tcb[FREERTOS_MAX_TASKS];

esp_tcb_t* esp_freertos_resolve_handle(TaskHandle_t h) {
    if (h == NULL) {
        uint32_t slot = sim_scheduler_current_id();
        return (slot < FREERTOS_MAX_TASKS && s_tcb[slot].used) ? &s_tcb[slot] : NULL;
    }
    uint32_t val = (uint32_t)(uintptr_t)h;
    uint32_t slot = val & 0xFFu;
    uint16_t gen = (uint16_t)(val >> 8);
    if (slot >= FREERTOS_MAX_TASKS) {
        return NULL;
    }
    esp_tcb_t* t = &s_tcb[slot];
    if (!t->used || t->gen != gen || t->sim_id != slot) {
        return NULL;
    }
    return t;
}

int32_t esp_freertos_get_task_prio(uint32_t sim_id) {
    if (sim_id < FREERTOS_MAX_TASKS && s_tcb[sim_id].used) {
        return s_tcb[sim_id].prio;
    }
    return 0;
}

void esp_freertos_register_task_slot(uint32_t slot, int32_t prio, const char* name) {
    if (slot >= FREERTOS_MAX_TASKS) return;
    s_tcb[slot].used = true;
    if (s_tcb[slot].gen == 0) s_tcb[slot].gen = 1;
    s_tcb[slot].sim_id = slot;
    s_tcb[slot].prio = prio;
    strncpy(s_tcb[slot].name, name ? name : "task", sizeof(s_tcb[slot].name) - 1);
    s_tcb[slot].name[sizeof(s_tcb[slot].name) - 1] = '\0';
}

void esp_freertos_task_pool_reset(void) {
    for (uint32_t i = 0; i < FREERTOS_MAX_TASKS; ++i) {
        if (s_tcb[i].used) {
            s_tcb[i].gen++;
            if (s_tcb[i].gen == 0) s_tcb[i].gen = 1;
            s_tcb[i].used = false;
        }
        s_tcb[i].sim_id = 0;
        s_tcb[i].prio = 0;
        s_tcb[i].name[0] = '\0';
    }
}

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t pxTaskCode,
                                    const char * const pcName,
                                    const uint32_t usStackDepth,
                                    void * const pvParameters,
                                    UBaseType_t uxPriority,
                                    TaskHandle_t * const pxCreatedTask,
                                    const BaseType_t xCoreID) {
    if (pxTaskCode == NULL) {
        return pdFAIL;
    }

    if (xCoreID != 0 && xCoreID != tskNO_AFFINITY) {
        ESP_LOGW("FREERTOS", "Core ID %d not supported; clamped to 0", (int)xCoreID);
    }

    uint32_t prio = (uint32_t)uxPriority;
    if (prio > 24u) {
        prio = 24u;
    }

    uint32_t slot = UINT32_MAX;
    wink_status_t st = sim_scheduler_register(
        pxTaskCode,
        pvParameters,
        pcName ? pcName : "task",
        (int32_t)prio,
        0,
        usStackDepth,
        &slot
    );

    if (st != WINK_OK || slot >= FREERTOS_MAX_TASKS) {
        return pdFAIL;
    }

    s_tcb[slot].used = true;
    if (s_tcb[slot].gen == 0) {
        s_tcb[slot].gen = 1;
    }
    s_tcb[slot].sim_id = slot;
    s_tcb[slot].prio = (int32_t)prio;
    strncpy(s_tcb[slot].name, pcName ? pcName : "task", sizeof(s_tcb[slot].name) - 1);
    s_tcb[slot].name[sizeof(s_tcb[slot].name) - 1] = '\0';

    if (pxCreatedTask != NULL) {
        uint32_t handle_val = ((uint32_t)s_tcb[slot].gen << 8) | slot;
        *pxCreatedTask = (TaskHandle_t)(uintptr_t)handle_val;
    }

    return pdPASS;
}

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                       const char * const pcName,
                       const uint32_t usStackDepth,
                       void * const pvParameters,
                       UBaseType_t uxPriority,
                       TaskHandle_t * const pxCreatedTask) {
    return xTaskCreatePinnedToCore(pxTaskCode, pcName, usStackDepth,
                                   pvParameters, uxPriority, pxCreatedTask,
                                   tskNO_AFFINITY);
}

void vTaskDelete(TaskHandle_t xTaskToDelete) {
    esp_tcb_t* t = esp_freertos_resolve_handle(xTaskToDelete);
    if (t == NULL) {
        return;
    }

    uint32_t slot = t->sim_id;
    uint32_t cur = sim_scheduler_current_id();

    t->used = false;
    t->gen++;
    if (t->gen == 0) {
        t->gen = 1;
    }

    sim_scheduler_mark_zombie(slot);

    if (slot == cur) {
        sim_scheduler_yield_context();
        for (;;) {}
    }
}

void vTaskSuspend(TaskHandle_t xTaskToSuspend) {
    esp_tcb_t* t = esp_freertos_resolve_handle(xTaskToSuspend);
    if (t == NULL) {
        return;
    }

    uint32_t slot = t->sim_id;
    uint32_t cur = sim_scheduler_current_id();
    uint32_t res_id = FREERTOS_MAKE_RES_ID(FREERTOS_TAG_SUSPEND, slot);

    sim_scheduler_block(slot, res_id, pal_os_get_us(), 0ULL);
    if (slot == cur) {
        sim_scheduler_yield_context();
    }
}

void vTaskResume(TaskHandle_t xTaskToResume) {
    esp_tcb_t* t = esp_freertos_resolve_handle(xTaskToResume);
    if (t == NULL) {
        return;
    }

    sim_scheduler_resume(t->sim_id);
}

void vTaskDelay(const TickType_t xTicksToDelay) {
    if (xTicksToDelay == 0) {
        sim_scheduler_yield_context();
        return;
    }

    uint32_t self = sim_scheduler_current_id();
    if (self == SIM_SCHED_NO_READY) {
        return;
    }

    uint64_t dur_us = (uint64_t)xTicksToDelay * (uint64_t)(portTICK_PERIOD_MS * 1000ULL);
    sim_scheduler_yield_timed(self, pal_os_get_us(), dur_us);
    sim_scheduler_yield_context();
}

void vTaskDelayUntil(TickType_t * const pxPreviousWakeTime, const TickType_t xTimeIncrement) {
    if (pxPreviousWakeTime == NULL) {
        return;
    }

    TickType_t now = xTaskGetTickCount();
    TickType_t target = *pxPreviousWakeTime + xTimeIncrement;
    *pxPreviousWakeTime = target;

    if ((int32_t)(target - now) > 0) {
        vTaskDelay(target - now);
    } else {
        *pxPreviousWakeTime = now;
    }
}

TickType_t xTaskGetTickCount(void) {
    return (TickType_t)(pal_os_get_us() / ((uint64_t)portTICK_PERIOD_MS * 1000ULL));
}

TickType_t xTaskGetTickCountFromISR(void) {
    return xTaskGetTickCount();
}

eTaskState eTaskGetState(TaskHandle_t xTask) {
    esp_tcb_t* t = esp_freertos_resolve_handle(xTask);
    if (t == NULL) {
        return eDeleted;
    }

    const sim_task_t* st = sim_scheduler_get(t->sim_id);
    if (st == NULL) {
        return eDeleted;
    }

    switch (st->state) {
        case SIM_TASK_STATE_READY:
            return (st->id == sim_scheduler_current_id()) ? eRunning : eReady;
        case SIM_TASK_STATE_WAITING:
        case SIM_TASK_STATE_BLOCKED:
            if (st->blocked_on != 0 &&
                FREERTOS_RES_TAG(st->blocked_on) == FREERTOS_TAG_SUSPEND &&
                st->wakeup_us == 0) {
                return eSuspended;
            }
            return eBlocked;
        case SIM_TASK_STATE_ZOMBIE:
        case SIM_TASK_STATE_TERMINATED:
        case SIM_TASK_STATE_INVALID:
        default:
            return eDeleted;
    }
}

UBaseType_t uxTaskGetNumberOfTasks(void) {
    return (UBaseType_t)sim_scheduler_task_count();
}

UBaseType_t uxTaskPriorityGet(const TaskHandle_t xTask) {
    esp_tcb_t* t = esp_freertos_resolve_handle(xTask);
    return t ? (UBaseType_t)t->prio : 0u;
}

void vTaskPrioritySet(TaskHandle_t xTask, UBaseType_t uxNewPriority) {
    esp_tcb_t* t = esp_freertos_resolve_handle(xTask);
    if (t != NULL) {
        t->prio = (int32_t)(uxNewPriority > 24u ? 24u : uxNewPriority);
    }
}

UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask) {
    (void)xTask;
    return UINT32_MAX;
}

BaseType_t xPortGetCoreID(void) {
    return 0;
}

BaseType_t xTaskGetSchedulerState(void) {
    return taskSCHEDULER_RUNNING;
}

void vTaskStartScheduler(void) {
    pal_log_w("FREERTOS", "vTaskStartScheduler is a no-op under cooperative simulator");
}

void vPortEnterCritical(void) {}
void vPortExitCritical(void) {}

void vTaskList(char * pcWriteBuffer) {
    if (pcWriteBuffer == NULL) return;
    int offset = 0;
    offset += snprintf(pcWriteBuffer + offset, 256 - offset, "Name          State  Prio  Id\n");
    for (uint32_t i = 0; i < FREERTOS_MAX_TASKS; ++i) {
        if (s_tcb[i].used) {
            const sim_task_t* st = sim_scheduler_get(i);
            const char* state_str = "Unknown";
            if (st) {
                switch (st->state) {
                    case SIM_TASK_STATE_READY: state_str = "Ready"; break;
                    case SIM_TASK_STATE_WAITING: state_str = "Waiting"; break;
                    case SIM_TASK_STATE_BLOCKED: state_str = "Blocked"; break;
                    case SIM_TASK_STATE_ZOMBIE: state_str = "Zombie"; break;
                    default: break;
                }
            }
            offset += snprintf(pcWriteBuffer + offset, 256 - offset,
                               "%-12s  %-7s  %-4d  %-2u\n",
                               s_tcb[i].name, state_str, (int)s_tcb[i].prio, i);
            if (offset >= 240) break;
        }
    }
}

UBaseType_t uxTaskGetSystemState(TaskStatus_t * const pxTaskStatusArray,
                                 const UBaseType_t uxArraySize,
                                 uint32_t * const pulTotalRunTime) {
    if (pxTaskStatusArray == NULL || uxArraySize == 0) {
        return 0;
    }
    UBaseType_t count = 0;
    for (uint32_t i = 0; i < FREERTOS_MAX_TASKS && count < uxArraySize; ++i) {
        if (s_tcb[i].used) {
            TaskStatus_t* s = &pxTaskStatusArray[count];
            s->xHandle = (TaskHandle_t)(uintptr_t)(((uint32_t)s_tcb[i].gen << 8) | i);
            s->pcTaskName = s_tcb[i].name;
            s->xTaskNumber = i;
            s->eCurrentState = eTaskGetState(s->xHandle);
            s->uxCurrentPriority = (UBaseType_t)s_tcb[i].prio;
            s->uxBasePriority = s->uxCurrentPriority;
            s->ulRunTimeCounter = 0;
            s->usStackHighWaterMark = UINT32_MAX;
            count++;
        }
    }
    if (pulTotalRunTime != NULL) {
        *pulTotalRunTime = (uint32_t)(pal_os_get_us() / 1000ULL);
    }
    return count;
}
