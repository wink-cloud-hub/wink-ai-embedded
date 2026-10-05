// SPDX-License-Identifier: LGPL-3.0-only
/**
 * @file wink_sim_scheduler.h
 * @brief Single-virtual-core cooperative task scheduler for simulation targets (ADR-0014).
 */
#ifndef WINK_SIM_SCHEDULER_H
#define WINK_SIM_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include "wink_status.h"
#include "sim_ctx.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINK_SIM_MAX_TASKS
#define WINK_SIM_MAX_TASKS 16
#endif
#define WINK_SIM_TASK_WCET_THRESHOLD_US (5000u)

#if defined(__EMSCRIPTEN__)
    #define WINK_SIM_STACK_MIN     (16u * 1024u)
    #define WINK_SIM_ASYNCIFY_MIN  (64u * 1024u)
#elif defined(_WIN32)
    #define WINK_SIM_STACK_MIN     (32u * 1024u)
    #define WINK_SIM_ASYNCIFY_MIN  0u
#else
    #define WINK_SIM_STACK_MIN     (32u * 1024u)
    #define WINK_SIM_ASYNCIFY_MIN  0u
#endif

typedef enum {
    WINK_SIM_WAKE_NONE = 0,
    WINK_SIM_WAKE_IRQ = 1,
    WINK_SIM_WAKE_SYNC_RES = 2,
    WINK_SIM_WAKE_TIMEOUT = 3,
    WINK_SIM_WAKE_DIRECT = 4,
} wink_sim_wake_reason_t;

typedef enum {
    WINK_SIM_TRACE_EVENT_NONE = 0,
    WINK_SIM_TRACE_EVENT_IRQ_DISPATCH = 1,
    WINK_SIM_TRACE_EVENT_TASK_WOKEN = 2,
    WINK_SIM_TRACE_EVENT_TASK_SWITCH_IN = 3,
    WINK_SIM_TRACE_EVENT_TASK_BLOCK = 4,
    WINK_SIM_TRACE_EVENT_TASK_YIELD = 5,
} wink_sim_trace_event_type_t;

#ifndef WINK_SIM_TRACE_CAPACITY
#define WINK_SIM_TRACE_CAPACITY 128
#endif

typedef struct {
    uint64_t virtual_time_us;
    uint32_t sequence;
    uint32_t task_id;
    uint32_t task_slot;
    uint32_t resource_id;
    uint8_t  wake_reason;       /**< wink_sim_wake_reason_t */
    uint8_t  event_type;        /**< wink_sim_trace_event_type_t */
    uint16_t reserved;
    char     task_name[16];
    uint32_t pad;
} wink_sim_trace_entry_t;
_Static_assert(sizeof(wink_sim_trace_entry_t) == 48, "wink_sim_trace_entry_t must be 48 bytes");

typedef enum {
    SIM_TASK_STATE_INVALID = 0,
    SIM_TASK_STATE_READY,       /**< Runnable */
    SIM_TASK_STATE_WAITING,     /**< Waiting for sleep_ms timer */
    SIM_TASK_STATE_BLOCKED,     /**< Blocked on resource (mutex/queue/sem); wakeup_us>0 means with timeout */
    SIM_TASK_STATE_ZOMBIE,      /**< Self-deleted, fiber not released, waiting for main loop GC */
    SIM_TASK_STATE_TERMINATED,  /**< Terminated and freed, slot reusable */
} sim_task_state_t;

typedef struct {
    void   (*func)(void*);
    void*    arg;
    int32_t  priority;
    int32_t  core_id;
    uint64_t wakeup_us;
    uint64_t runtime_us;
    uint32_t blocked_on;
    bool     timeout_fired;
    uint8_t  last_wake_reason;  /**< wink_sim_wake_reason_t */
    sim_task_state_t state;
    uint32_t id;
    char     name[16];
    sim_ctx_t* ctx;
} sim_task_t;
_Static_assert(sizeof(sim_task_t) <= 96, "sim_task_t must stay compact");

void          sim_scheduler_reset(uint32_t prng_seed);
wink_status_t sim_scheduler_register(void (*func)(void*), void* arg,
                                     const char* name, int32_t priority,
                                     int32_t core_id, uint32_t stack_depth,
                                     uint32_t* out_id);
void          sim_scheduler_mark_zombie(uint32_t task_id);
void          sim_scheduler_gc_zombies(void);

void          sim_scheduler_accumulate_runtime(uint32_t task_id, uint64_t delta_us);
uint64_t      sim_scheduler_get_runtime_us(uint32_t task_id);
void          sim_scheduler_reset_runtime(void);

struct wink_app_callbacks;
wink_status_t pal_sim_scheduler_run(const struct wink_app_callbacks* callbacks,
                                    uint32_t main_task_id, uint32_t max_ticks);

uint32_t      sim_scheduler_wakeup_by_time(uint64_t now_us);
#define SIM_SCHED_NO_READY UINT32_MAX
uint32_t      sim_scheduler_pick_next(void);

void          sim_scheduler_yield_timed(uint32_t task_id, uint64_t now_us, uint64_t duration_us);
void          sim_scheduler_block(uint32_t task_id, uint32_t resource_id,
                                uint64_t now_us, uint64_t timeout_us);
void          sim_scheduler_resume(uint32_t task_id);

uint64_t      sim_scheduler_next_wakeup_us(void);

uint32_t      sim_scheduler_task_count(void);
const sim_task_t* sim_scheduler_get(uint32_t task_id);
uint32_t      sim_scheduler_current_id(void);
void          sim_scheduler_set_current(uint32_t task_id);

sim_ctx_t*    sim_scheduler_current_ctx(void);

void          sim_scheduler_set_main_ctx(sim_ctx_t* ctx);
sim_ctx_t*    sim_scheduler_main_ctx(void);
void          sim_scheduler_yield_context(void);

void                          sim_scheduler_trace_enable(bool enable);
bool                          sim_scheduler_trace_is_enabled(void);
void                          sim_scheduler_trace_reset(void);
uint32_t                      sim_scheduler_trace_count(void);
const wink_sim_trace_entry_t* sim_scheduler_trace_get(uint32_t index);
void                          sim_scheduler_trace_record(uint64_t virtual_time_us, uint32_t task_slot,
                                                         uint32_t resource_id, wink_sim_wake_reason_t wake_reason,
                                                         wink_sim_trace_event_type_t event_type);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* WINK_SIM_SCHEDULER_H */
