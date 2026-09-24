/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef FREERTOS_SYNC_H
#define FREERTOS_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wink_sim_scheduler.h"
#include "pal_osal.h"
#include "pal_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FREERTOS_MAX_TASKS WINK_SIM_MAX_TASKS /* 8 */

#define FREERTOS_TAG_QUEUE    0x01u
#define FREERTOS_TAG_MUTEX    0x02u
#define FREERTOS_TAG_SEM      0x03u
#define FREERTOS_TAG_EVENT    0x04u
#define FREERTOS_TAG_GPTIMER  0x05u
#define FREERTOS_TAG_SUSPEND  0x06u
#define FREERTOS_TAG_TIMER    0x07u

#define FREERTOS_MAKE_RES_ID(tag, idx) (((uint32_t)(tag) << 24) | ((uint32_t)(idx) & 0x00FFFFFFu))
#define FREERTOS_RES_TAG(res_id)       ((uint8_t)((res_id) >> 24))
#define FREERTOS_RES_INDEX(res_id)     ((uint32_t)((res_id) & 0x00FFFFFFu))

typedef struct {
    bool     used;
    uint16_t gen;
    uint32_t sim_id;
    int32_t  prio;
    char     name[16];
} esp_tcb_t;

/**
 * @brief Block calling task fiber on resource_id with ticks timeout
 * @return true if woken up normally, false if timed out
 */
static inline bool sync_block(uint32_t resource_id, TickType_t xTicksToWait) {
    uint32_t self = sim_scheduler_current_id();
    if (self == SIM_SCHED_NO_READY) {
        return false;
    }
    if (xTicksToWait == 0) {
        return false;
    }
    uint64_t timeout_us = (xTicksToWait == portMAX_DELAY)
                              ? 0ULL
                              : ((uint64_t)xTicksToWait * (uint64_t)(portTICK_PERIOD_MS * 1000ULL));
    sim_scheduler_block(self, resource_id, pal_os_get_us(), timeout_us);
    sim_scheduler_yield_context();
    const sim_task_t* t = sim_scheduler_get(self);
    return (t != NULL && !t->timeout_fired);
}

esp_tcb_t* esp_freertos_resolve_handle(TaskHandle_t h);
int32_t    esp_freertos_get_task_prio(uint32_t sim_id);
void       esp_freertos_task_pool_reset(void);
void       esp_freertos_queue_pool_reset(void);
void       esp_freertos_sem_pool_reset(void);
void       esp_freertos_event_pool_reset(void);
void       esp_freertos_pools_reset(void);
void       esp_freertos_register_task_slot(uint32_t slot, int32_t prio, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_SYNC_H */
