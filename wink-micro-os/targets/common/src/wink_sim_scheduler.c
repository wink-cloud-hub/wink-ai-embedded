// SPDX-License-Identifier: LGPL-3.0-only
#include "wink_sim_scheduler.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#if defined(_MSC_VER)
#  pragma warning(disable: 4996)
#  define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef WINK_SIM_SCHED_TRACE
#define WINK_SIM_SCHED_TRACE 0
#endif

#if WINK_SIM_SCHED_TRACE
#define SCHED_TRACE(fmt, ...) fprintf(stderr, "[SCHED] " fmt "\n", ##__VA_ARGS__)
#else
#define SCHED_TRACE(fmt, ...) ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak)) uint64_t pal_os_get_us(void) { return 0; }
__attribute__((weak)) bool pal_os_in_isr(void) { return false; }
#elif defined(_MSC_VER)
static uint64_t _default_pal_os_get_us(void) { return 0; }
static bool _default_pal_os_in_isr(void) { return false; }
#pragma comment(linker, "/alternatename:pal_os_get_us=_default_pal_os_get_us")
#pragma comment(linker, "/alternatename:pal_os_in_isr=_default_pal_os_in_isr")
#else
extern uint64_t pal_os_get_us(void);
extern bool pal_os_in_isr(void);
#endif

static sim_task_t s_tasks[WINK_SIM_MAX_TASKS];
static uint32_t s_task_id_counter = 0;
static uint32_t s_current_task_id = SIM_SCHED_NO_READY;
static uint32_t s_prng_state = 42;
static uint32_t s_last_scheduled_task_id = SIM_SCHED_NO_READY;

static wink_sim_trace_entry_t s_sim_trace_buffer[WINK_SIM_TRACE_CAPACITY];
static uint32_t s_sim_trace_count = 0;
static uint32_t s_sim_trace_sequence = 0;
static bool s_sim_trace_enabled = true;

void sim_scheduler_trace_enable(bool enable) {
    s_sim_trace_enabled = enable;
}

bool sim_scheduler_trace_is_enabled(void) {
    return s_sim_trace_enabled;
}

void sim_scheduler_trace_reset(void) {
    memset(s_sim_trace_buffer, 0, sizeof(s_sim_trace_buffer));
    s_sim_trace_count = 0;
    s_sim_trace_sequence = 0;
}

uint32_t sim_scheduler_trace_count(void) {
    return s_sim_trace_count;
}

const wink_sim_trace_entry_t* sim_scheduler_trace_get(uint32_t index) {
    if (index >= s_sim_trace_count || index >= WINK_SIM_TRACE_CAPACITY) {
        return NULL;
    }
    return &s_sim_trace_buffer[index];
}

void sim_scheduler_trace_record(uint64_t virtual_time_us, uint32_t task_slot,
                                uint32_t resource_id, wink_sim_wake_reason_t wake_reason,
                                wink_sim_trace_event_type_t event_type) {
    if (!s_sim_trace_enabled) return;
    if (s_sim_trace_count >= WINK_SIM_TRACE_CAPACITY) return;

    wink_sim_trace_entry_t* entry = &s_sim_trace_buffer[s_sim_trace_count++];
    entry->virtual_time_us = (virtual_time_us > 0) ? virtual_time_us : pal_os_get_us();
    entry->sequence = ++s_sim_trace_sequence;
    entry->task_slot = task_slot;
    entry->resource_id = resource_id;
    entry->wake_reason = (uint8_t)wake_reason;
    entry->event_type = (uint8_t)event_type;
    entry->reserved = 0;

    if (task_slot < WINK_SIM_MAX_TASKS &&
        s_tasks[task_slot].state != SIM_TASK_STATE_INVALID &&
        s_tasks[task_slot].state != SIM_TASK_STATE_TERMINATED) {
        entry->task_id = s_tasks[task_slot].id;
        strncpy(entry->task_name, s_tasks[task_slot].name, sizeof(entry->task_name) - 1);
        entry->task_name[sizeof(entry->task_name) - 1] = '\0';
    } else {
        entry->task_id = 0;
        entry->task_name[0] = '\0';
    }
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused))
#endif
static uint32_t sim_prng_next(void) {
    uint32_t x = s_prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_prng_state = x;
    return x;
}

void sim_scheduler_reset(uint32_t prng_seed) {
    SCHED_TRACE("Resetting scheduler with seed %u", prng_seed);

    assert(s_current_task_id == SIM_SCHED_NO_READY &&
           "sim_scheduler_reset called while task fiber is running; "
           "return to main scheduler ctx before resetting");

    for (uint32_t i = 0; i < WINK_SIM_MAX_TASKS; ++i) {
        if (s_tasks[i].state != SIM_TASK_STATE_INVALID &&
            s_tasks[i].state != SIM_TASK_STATE_TERMINATED) {
            if (s_tasks[i].ctx) {
                sim_ctx_destroy(s_tasks[i].ctx);
                s_tasks[i].ctx = NULL;
            }
        }
    }

    memset(s_tasks, 0, sizeof(s_tasks));
    s_task_id_counter = 0;
    s_current_task_id = SIM_SCHED_NO_READY;
    s_last_scheduled_task_id = SIM_SCHED_NO_READY;
    s_prng_state = prng_seed ? prng_seed : 42;
    sim_scheduler_trace_reset();
}

wink_status_t sim_scheduler_register(void (*func)(void*), void* arg,
                                     const char* name, int32_t priority,
                                     int32_t core_id, uint32_t stack_depth,
                                     uint32_t* out_id) {
    uint32_t slot = UINT32_MAX;
    for (uint32_t i = 0; i < WINK_SIM_MAX_TASKS; ++i) {
        if (s_tasks[i].state == SIM_TASK_STATE_INVALID || 
            s_tasks[i].state == SIM_TASK_STATE_TERMINATED) {
            slot = i;
            break;
        }
    }
    
    if (slot == UINT32_MAX) {
        SCHED_TRACE("Failed to register task '%s': no free slot", name);
        return WINK_ERR_NO_MEM;
    }
    
    uint32_t eff_stack = stack_depth;
    if (eff_stack < WINK_SIM_STACK_MIN) {
        fprintf(stderr, "[WARN] task '%s' stack_depth=%u < sim min=%u, clamped (ADR-0013 §sim-stack-contract)\n",
                name, stack_depth, WINK_SIM_STACK_MIN);
        eff_stack = WINK_SIM_STACK_MIN;
    }
    
    sim_ctx_t* ctx = sim_ctx_create(func, arg, eff_stack);
    if (!ctx) {
        SCHED_TRACE("Failed to create context for task '%s'", name);
        return WINK_ERR_NO_MEM;
    }
    
    sim_task_t* t = &s_tasks[slot];
    t->func = func;
    t->arg = arg;
    t->priority = priority;
    t->core_id = core_id;
    t->wakeup_us = 0;
    t->blocked_on = 0;
    t->timeout_fired = false;
    t->last_wake_reason = (uint8_t)WINK_SIM_WAKE_NONE;
    t->state = SIM_TASK_STATE_READY;
    t->id = s_task_id_counter++;
    t->ctx = ctx;
    
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';
    
    if (out_id) {
        *out_id = slot;
    }
    
    SCHED_TRACE("Registered task '%s' [slot=%u, id=%u] with eff_stack=%u", name, slot, t->id, eff_stack);
    return WINK_OK;
}

void sim_scheduler_mark_zombie(uint32_t task_id) {
    if (task_id < WINK_SIM_MAX_TASKS) {
        sim_task_t* t = &s_tasks[task_id];
        if (t->state != SIM_TASK_STATE_INVALID && t->state != SIM_TASK_STATE_TERMINATED) {
            t->state = SIM_TASK_STATE_ZOMBIE;
            SCHED_TRACE("Marked task '%s' [slot=%u] as ZOMBIE", t->name, task_id);
        }
    }
}

void sim_scheduler_gc_zombies(void) {
    for (uint32_t i = 0; i < WINK_SIM_MAX_TASKS; ++i) {
        sim_task_t* t = &s_tasks[i];
        if (t->state == SIM_TASK_STATE_ZOMBIE) {
            SCHED_TRACE("GC collecting task '%s' [slot=%u]", t->name, i);
            if (t->ctx) {
                sim_ctx_destroy(t->ctx);
                t->ctx = NULL;
            }
            t->state = SIM_TASK_STATE_TERMINATED;
        }
    }
}

uint32_t sim_scheduler_wakeup_by_time(uint64_t now_us) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < WINK_SIM_MAX_TASKS; ++i) {
        sim_task_t* t = &s_tasks[i];
        if ((t->state == SIM_TASK_STATE_WAITING || t->state == SIM_TASK_STATE_BLOCKED) && 
             t->wakeup_us > 0 && t->wakeup_us <= now_us) {
            
            bool was_blocked = (t->state == SIM_TASK_STATE_BLOCKED);
            uint32_t res_id = t->blocked_on;
            t->state = SIM_TASK_STATE_READY;
            t->wakeup_us = 0;
            if (was_blocked) {
                t->timeout_fired = true;
                t->blocked_on = 0;
            }
            t->last_wake_reason = (uint8_t)WINK_SIM_WAKE_TIMEOUT;
            sim_scheduler_trace_record(now_us, i, res_id,
                                       WINK_SIM_WAKE_TIMEOUT, WINK_SIM_TRACE_EVENT_TASK_WOKEN);
            count++;
            SCHED_TRACE("Woke up task '%s' [slot=%u] due to timeout (was_blocked=%d)", t->name, i, was_blocked);
        }
    }
    return count;
}

uint32_t sim_scheduler_pick_next(void) {
    /* ADR-0053 Total Order Arbitration:
     * In the same virtual instant, tasks woken by an external/IRQ causal chain
     * take deterministic precedence over pure timer/delay timeout tasks. */
    for (uint32_t i = 0; i < WINK_SIM_MAX_TASKS; ++i) {
        if (s_tasks[i].state == SIM_TASK_STATE_READY &&
            s_tasks[i].last_wake_reason == WINK_SIM_WAKE_IRQ) {
            s_last_scheduled_task_id = i;
            SCHED_TRACE("Picked IRQ-woken task slot=%u (ADR-0053 causal precedence)", i);
            return i;
        }
    }

    uint32_t start_id = (s_last_scheduled_task_id == SIM_SCHED_NO_READY)
                        ? 0u
                        : (s_last_scheduled_task_id + 1u) % WINK_SIM_MAX_TASKS;

    for (uint32_t i = 0; i < WINK_SIM_MAX_TASKS; ++i) {
        uint32_t id = (start_id + i) % WINK_SIM_MAX_TASKS;
        if (s_tasks[id].state == SIM_TASK_STATE_READY) {
            s_last_scheduled_task_id = id;
            SCHED_TRACE("Picked next slot=%u (round-robin from %u)", id, start_id);
            return id;
        }
    }
    return SIM_SCHED_NO_READY;
}

void sim_scheduler_yield_timed(uint32_t task_id, uint64_t now_us, uint64_t duration_us) {
    if (task_id < WINK_SIM_MAX_TASKS) {
        sim_task_t* t = &s_tasks[task_id];
        t->state = SIM_TASK_STATE_WAITING;
        t->wakeup_us = now_us + duration_us;
        t->last_wake_reason = (uint8_t)WINK_SIM_WAKE_NONE;
        sim_scheduler_trace_record(now_us, task_id, 0,
                                   WINK_SIM_WAKE_NONE, WINK_SIM_TRACE_EVENT_TASK_YIELD);
        SCHED_TRACE("Task '%s' [slot=%u] yielding for %llu us (until %llu)", t->name, task_id, duration_us, t->wakeup_us);
    }
}

void sim_scheduler_block(uint32_t task_id, uint32_t resource_id,
                          uint64_t now_us, uint64_t timeout_us) {
    if (task_id < WINK_SIM_MAX_TASKS) {
        sim_task_t* t = &s_tasks[task_id];
        t->state = SIM_TASK_STATE_BLOCKED;
        t->blocked_on = resource_id;
        t->wakeup_us = (timeout_us == 0) ? 0 : (now_us + timeout_us);
        t->timeout_fired = false;
        t->last_wake_reason = (uint8_t)WINK_SIM_WAKE_NONE;
        sim_scheduler_trace_record(now_us, task_id, resource_id,
                                   WINK_SIM_WAKE_NONE, WINK_SIM_TRACE_EVENT_TASK_BLOCK);
        SCHED_TRACE("Task '%s' [slot=%u] blocked on res=%u, timeout=%llu", t->name, task_id, resource_id, timeout_us);
    }
}

void sim_scheduler_resume(uint32_t task_id) {
    if (task_id < WINK_SIM_MAX_TASKS) {
        sim_task_t* t = &s_tasks[task_id];
        if (t->state == SIM_TASK_STATE_BLOCKED) {
            uint32_t res_id = t->blocked_on;
            t->state = SIM_TASK_STATE_READY;
            t->blocked_on = 0;
            t->wakeup_us = 0;
            t->timeout_fired = false;

            wink_sim_wake_reason_t reason = pal_os_in_isr() ? WINK_SIM_WAKE_IRQ : WINK_SIM_WAKE_SYNC_RES;
            t->last_wake_reason = (uint8_t)reason;

            sim_scheduler_trace_record(pal_os_get_us(), task_id, res_id,
                                       reason, WINK_SIM_TRACE_EVENT_TASK_WOKEN);
            SCHED_TRACE("Resumed task '%s' [slot=%u, reason=%u]", t->name, task_id, reason);
        }
    }
}

uint64_t sim_scheduler_next_wakeup_us(void) {
    uint64_t min_wakeup = UINT64_MAX;
    for (uint32_t i = 0; i < WINK_SIM_MAX_TASKS; ++i) {
        sim_task_t* t = &s_tasks[i];
        if ((t->state == SIM_TASK_STATE_WAITING || t->state == SIM_TASK_STATE_BLOCKED) && 
             t->wakeup_us > 0) {
            if (t->wakeup_us < min_wakeup) {
                min_wakeup = t->wakeup_us;
            }
        }
    }
    return min_wakeup;
}

uint32_t sim_scheduler_task_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < WINK_SIM_MAX_TASKS; ++i) {
        if (s_tasks[i].state != SIM_TASK_STATE_INVALID && 
            s_tasks[i].state != SIM_TASK_STATE_TERMINATED) {
            count++;
        }
    }
    return count;
}

const sim_task_t* sim_scheduler_get(uint32_t task_id) {
    if (task_id < WINK_SIM_MAX_TASKS) {
        return &s_tasks[task_id];
    }
    return NULL;
}

uint32_t sim_scheduler_current_id(void) {
    return s_current_task_id;
}

void sim_scheduler_set_current(uint32_t task_id) {
    s_current_task_id = task_id;
}

sim_ctx_t* sim_scheduler_current_ctx(void) {
    if (s_current_task_id >= WINK_SIM_MAX_TASKS) return NULL;
    return s_tasks[s_current_task_id].ctx;
}

static sim_ctx_t* s_sim_main_ctx = NULL;

void sim_scheduler_set_main_ctx(sim_ctx_t* ctx) {
    s_sim_main_ctx = ctx;
}

sim_ctx_t* sim_scheduler_main_ctx(void) {
    return s_sim_main_ctx;
}

void sim_scheduler_yield_context(void) {
    sim_ctx_t* cur = sim_scheduler_current_ctx();
    assert(cur != NULL && s_sim_main_ctx != NULL &&
           "sim_scheduler_yield_context called outside valid fiber context or main_ctx unset");
    sim_ctx_switch(cur, s_sim_main_ctx);
}

