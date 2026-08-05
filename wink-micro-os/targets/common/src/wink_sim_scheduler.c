// SPDX-License-Identifier: Apache-2.0
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

static sim_task_t s_tasks[WINK_SIM_MAX_TASKS];
static uint32_t s_task_id_counter = 0;
static uint32_t s_current_task_id = SIM_SCHED_NO_READY;
static uint32_t s_prng_state = 42;
static uint32_t s_last_scheduled_task_id = SIM_SCHED_NO_READY;

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
            t->state = SIM_TASK_STATE_READY;
            t->wakeup_us = 0;
            if (was_blocked) {
                t->timeout_fired = true;
                t->blocked_on = 0;
            }
            count++;
            SCHED_TRACE("Woke up task '%s' [slot=%u] due to timeout (was_blocked=%d)", t->name, i, was_blocked);
        }
    }
    return count;
}

uint32_t sim_scheduler_pick_next(void) {
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
        SCHED_TRACE("Task '%s' [slot=%u] blocked on res=%u, timeout=%llu", t->name, task_id, resource_id, timeout_us);
    }
}

void sim_scheduler_resume(uint32_t task_id) {
    if (task_id < WINK_SIM_MAX_TASKS) {
        sim_task_t* t = &s_tasks[task_id];
        if (t->state == SIM_TASK_STATE_BLOCKED) {
            t->state = SIM_TASK_STATE_READY;
            t->blocked_on = 0;
            t->wakeup_us = 0;
            t->timeout_fired = false;
            SCHED_TRACE("Resumed task '%s' [slot=%u]", t->name, task_id);
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
