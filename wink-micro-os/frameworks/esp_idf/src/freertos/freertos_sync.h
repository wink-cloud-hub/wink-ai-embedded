/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef FREERTOS_SYNC_H
#define FREERTOS_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wink_sim_scheduler.h"
#include "pal_osal.h"
#include "pal_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FREERTOS_MAX_TASKS
#  ifdef CONFIG_FREERTOS_MAX_TASKS
#    define FREERTOS_MAX_TASKS CONFIG_FREERTOS_MAX_TASKS
#  else
#    define FREERTOS_MAX_TASKS WINK_SIM_MAX_TASKS
#  endif
#endif

#define FREERTOS_TAG_QUEUE    0x01u
#define FREERTOS_TAG_MUTEX    0x02u
#define FREERTOS_TAG_SEM      0x03u
#define FREERTOS_TAG_EVENT    0x04u
#define FREERTOS_TAG_GPTIMER  0x05u
#define FREERTOS_TAG_SUSPEND  0x06u
#define FREERTOS_TAG_TIMER    0x07u
#define FREERTOS_TAG_TASK_NOTIFY 0x08u

#define FREERTOS_MAKE_RES_ID(tag, idx) (((uint32_t)(tag) << 24) | ((uint32_t)(idx) & 0x00FFFFFFu))
#define FREERTOS_RES_TAG(res_id)       ((uint8_t)((res_id) >> 24))
#define FREERTOS_RES_INDEX(res_id)     ((uint32_t)((res_id) & 0x00FFFFFFu))

typedef struct {
    bool     used;
    uint16_t gen;
    uint32_t token;
    uint32_t sim_id;
    int32_t  prio;
    uint32_t notify_val;
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
void       esp_freertos_timers_sim_reset(void);
void       esp_freertos_pools_reset(void);
void       esp_freertos_register_task_slot(uint32_t slot, int32_t prio, const char* name);
uint32_t   esp_freertos_get_active_timer_count(void);
uint32_t   esp_freertos_get_timer_daemon_sim_id(void);

typedef void (*esp_timer_work_fn_t)(void *arg, uint32_t token);
BaseType_t esp_freertos_timer_post_work_item(esp_timer_work_fn_t fn, void *arg, uint32_t *out_token, TickType_t delay_ticks);
BaseType_t esp_freertos_timer_cancel_work_item(uint32_t token);

/* ── Phase 2: spinlock critical-depth tracking (ISSUE-02) ──────────────────
 * Implemented in freertos_spinlock.c. Internal arrays not exposed here.
 * esp_freertos_get_critical_depth() returns the nesting depth for task_id;
 * esp_freertos_assert_not_in_critical() hard-asserts the caller is NOT inside
 * a portMUX_TYPE critical section (called from all blocking/yield primitives). */
uint32_t   esp_freertos_get_critical_depth(uint32_t task_id);
void       esp_freertos_assert_not_in_critical(const char *api_name);
void       esp_freertos_spinlock_reset(void);

/* ── Phase 2: per-fiber busy-wait spin accounting (ISSUE-06) ───────────────
 * esp_sim_spin_wait_account() increments the current fiber's spin counter;
 * when the threshold is hit it auto-yields + advances virtual time.
 * esp_sim_spin_wait_reset() clears the counter on any voluntary yield. */
void       esp_sim_spin_wait_account(void);
void       esp_sim_spin_wait_reset(uint32_t task_id);

/* ── Phase 3: ISR execution guard & yield request (ISSUE-04) ──────────────── */
void       esp_freertos_request_isr_yield(void);
bool       esp_freertos_is_isr_yield_requested(void);
void       esp_freertos_clear_isr_yield_requested(void);

/**
 * @brief Fail-loud gate to verify an API is NOT called from ISR context.
 *        Called from all blocking FreeRTOS primitives (delay, sem take, queue recv).
 */
static inline void esp_freertos_assert_not_in_isr(const char *api_name) {
    if (pal_os_in_isr()) {
        pal_log_e("FREERTOS", "FATAL: Illegal blocking call %s invoked from ISR context!",
                  api_name ? api_name : "<unknown>");
        assert(!pal_os_in_isr() && "Illegal blocking call invoked from ISR context");
        abort();
    }
}

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_SYNC_H */
