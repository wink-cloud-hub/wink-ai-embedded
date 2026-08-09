// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_osal_wasm.c
 * @brief WASM simulation target PAL OSAL adapter (delay/tick/mutex).
 *
 * Virtual clock SSOT architecture (ADR-0003 Decision 3 + ADR-0009 §4.1):
 *   - `s_virtual_us` is the single source of truth for the virtual clock on WASM side, starting at 0.
 *   - Only write entry point: `pal_wasm_advance_virtual_clock()` (exported to JS Worker).
 *   - Read entry points: `pal_os_get_us()` / `pal_os_get_ms()`, pure memory access.
 *   - Architectural Invariant: Do NOT advance virtual clock inside `pal_os_sleep_ms/us()`.
 */
#include "pal_osal.h"
#include "wasm_bridge.h"
#include "pal_wasm_common.h"
#include "wink_sim_scheduler.h"
#include "wink_trace.h"
#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static bool s_sim_in_isr = false;
static bool s_sim_in_pt = false;

/* ---------------------------------------------------------
 * Virtual Clock
 * --------------------------------------------------------- */

static uint64_t s_virtual_us = 0;
static bool s_clock_warning_fired = false;

_Static_assert(sizeof(s_virtual_us) == 8, "Virtual clock must be 64-bit");

#define CLOCK_WARNING_THRESHOLD (UINT64_C(0x8000000000000000))

static inline void wink_vclock_advance_internal(uint64_t delta_us) {
    s_virtual_us += delta_us;
    if (s_virtual_us > CLOCK_WARNING_THRESHOLD && !s_clock_warning_fired) {
        s_clock_warning_fired = true;
    }
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_advance_virtual_clock(uint64_t us) {
    WASM_FAULT_GUARD_VOID();
    wink_vclock_advance_internal(us);
}

static wink_sim_mode_t s_sim_mode = WINK_SIM_MODE_INTERACTIVE;
static bool s_sim_mode_explicit = false;

static void wink_sim_mode_init_from_env(void) {
    if (s_sim_mode_explicit) return;
    const char* env = getenv("WINK_SIM_MODE");
    if (env && strcmp(env, "HEADLESS") == 0)          s_sim_mode = WINK_SIM_MODE_HEADLESS;
    else if (env && strcmp(env, "INTERACTIVE") == 0)  s_sim_mode = WINK_SIM_MODE_INTERACTIVE;
    else {
#if defined(__EMSCRIPTEN__)
        s_sim_mode = WINK_SIM_MODE_INTERACTIVE;
#else
        s_sim_mode = WINK_SIM_MODE_HEADLESS;
#endif
    }
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_sim_mode(uint32_t mode) {
    if (mode <= WINK_SIM_MODE_HEADLESS) {
        s_sim_mode = (wink_sim_mode_t)mode;
        s_sim_mode_explicit = true;
    }
}

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_sim_mode(void) {
    return (uint32_t)s_sim_mode;
}

void wink_sim_set_mode(wink_sim_mode_t mode) {
    pal_wasm_set_sim_mode((uint32_t)mode);
}

wink_sim_mode_t wink_sim_get_mode(void) {
    return (wink_sim_mode_t)pal_wasm_get_sim_mode();
}

EMSCRIPTEN_KEEPALIVE
uint64_t pal_os_get_us(void) { return s_virtual_us; }
EMSCRIPTEN_KEEPALIVE
uint64_t pal_os_get_ms(void) { return s_virtual_us / 1000u; }

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_is_clock_warning_fired(void) {
    return s_clock_warning_fired;
}

EMSCRIPTEN_KEEPALIVE
uint64_t pal_wasm_get_virtual_clock_us(void) {
    return s_virtual_us;
}

static sim_ctx_t* s_main_ctx = NULL;
static bool s_scheduler_running = false;

EMSCRIPTEN_KEEPALIVE
void pal_wasm_reset_scheduler_state(void) {
    s_scheduler_running = false;
    sim_scheduler_set_current(SIM_SCHED_NO_READY);
    pal_wasm_clear_fault_latch();
}

void pal_os_sleep_ms(uint32_t ms) {
    if (s_main_ctx == NULL) {
        js_pal_os_sleep_ms(ms);
        return;
    }
    uint32_t cur = sim_scheduler_current_id();
    assert(cur != SIM_SCHED_NO_READY &&
           "pal_os_sleep_ms called from main thread while scheduler is active; "
           "tasks must sleep inside their fiber context.");
    sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
    assert(cur_ctx != NULL && "sim_scheduler_current_ctx returned NULL in task context");
    sim_scheduler_yield_timed(cur, pal_os_get_us(), (uint64_t)ms * 1000);
    sim_ctx_switch(cur_ctx, s_main_ctx);
}

void pal_os_busy_wait_us(uint32_t us) {
    js_pal_os_busy_wait_us(us);
}

/* ---------------------------------------------------------
 * Mutex: Cooperative Scheduler BLOCKED Path Implementation
 * --------------------------------------------------------- */

#define WASM_MUTEX_POOL_SIZE  WINK_SIM_MAX_TASKS
#define WASM_MUTEX_NO_OWNER   (WINK_SIM_MAX_TASKS + 1)

typedef struct {
    bool     used;
    uint32_t owner;
    uint32_t waiters[WINK_SIM_MAX_TASKS];
    uint8_t  w_head;
    uint8_t  w_tail;
    uint8_t  w_count;
} wasm_mutex_t;

static wasm_mutex_t s_mtx_pool[WASM_MUTEX_POOL_SIZE];

#define WASM_SEM_POOL_SIZE  WINK_SIM_MAX_TASKS

typedef struct {
    bool     used;
    uint32_t count;
    uint32_t waiters[WINK_SIM_MAX_TASKS];
    uint8_t  w_head;
    uint8_t  w_tail;
    uint8_t  w_count;
} wasm_sem_t;

static wasm_sem_t s_sem_pool[WASM_SEM_POOL_SIZE];

static wasm_sem_t* wasm_sem_from_handle(pal_os_sem_t h) {
    if (h == NULL) return NULL;
    uintptr_t idx = (uintptr_t)h - 1;
    if (idx >= WASM_SEM_POOL_SIZE) return NULL;
    wasm_sem_t* s = &s_sem_pool[idx];
    if (!s->used) return NULL;
    return s;
}

static void sq_push(wasm_sem_t* s, uint32_t tid) {
    if (s->w_count >= WINK_SIM_MAX_TASKS) return;
    s->waiters[s->w_tail] = tid;
    s->w_tail = (uint8_t)((s->w_tail + 1u) % WINK_SIM_MAX_TASKS);
    s->w_count++;
}

static bool sq_pop(wasm_sem_t* s, uint32_t* out) {
    if (s->w_count == 0) return false;
    *out = s->waiters[s->w_head];
    s->w_head = (uint8_t)((s->w_head + 1u) % WINK_SIM_MAX_TASKS);
    s->w_count--;
    return true;
}

static void sq_remove(wasm_sem_t* s, uint32_t tid) {
    if (s->w_count == 0) return;
    uint8_t w = 0, r = 0;
    uint8_t cnt = s->w_count;
    for (uint8_t i = 0; i < cnt; ++i) {
        uint32_t t = s->waiters[(s->w_head + i) % WINK_SIM_MAX_TASKS];
        if (t != tid) {
            s->waiters[(s->w_head + w) % WINK_SIM_MAX_TASKS] = t;
            w++;
        } else { r++; }
    }
    s->w_tail = (uint8_t)((s->w_head + w) % WINK_SIM_MAX_TASKS);
    s->w_count = (uint8_t)(s->w_count - r);
}

static wasm_mutex_t* wasm_mtx_from_handle(pal_os_mutex_t h) {
    if (h == NULL) return NULL;
    uintptr_t idx = (uintptr_t)h - 1;
    if (idx >= WASM_MUTEX_POOL_SIZE) return NULL;
    wasm_mutex_t* m = &s_mtx_pool[idx];
    if (!m->used) return NULL;
    return m;
}

static void wq_push(wasm_mutex_t* m, uint32_t tid) {
    if (m->w_count >= WINK_SIM_MAX_TASKS) return;
    m->waiters[m->w_tail] = tid;
    m->w_tail = (uint8_t)((m->w_tail + 1u) % WINK_SIM_MAX_TASKS);
    m->w_count++;
}

static bool wq_pop(wasm_mutex_t* m, uint32_t* out) {
    if (m->w_count == 0) return false;
    *out = m->waiters[m->w_head];
    m->w_head = (uint8_t)((m->w_head + 1u) % WINK_SIM_MAX_TASKS);
    m->w_count--;
    return true;
}

static void wq_remove(wasm_mutex_t* m, uint32_t tid) {
    if (m->w_count == 0) return;
    uint8_t r = 0, w = 0;
    uint8_t cnt = m->w_count;
    for (uint8_t i = 0; i < cnt; ++i) {
        uint32_t t = m->waiters[(m->w_head + i) % WINK_SIM_MAX_TASKS];
        if (t != tid) {
            m->waiters[(m->w_head + w) % WINK_SIM_MAX_TASKS] = t;
            w++;
        } else {
            r++;
        }
    }
    m->w_tail = (uint8_t)((m->w_head + w) % WINK_SIM_MAX_TASKS);
    m->w_count = (uint8_t)(m->w_count - r);
}

pal_os_mutex_t pal_os_mutex_create(void) {
    for (uint32_t i = 0; i < WASM_MUTEX_POOL_SIZE; ++i) {
        if (!s_mtx_pool[i].used) {
            wasm_mutex_t* m = &s_mtx_pool[i];
            m->used = true;
            m->owner = WASM_MUTEX_NO_OWNER;
            m->w_head = 0;
            m->w_tail = 0;
            m->w_count = 0;
            return (pal_os_mutex_t)(uintptr_t)(i + 1);
        }
    }
    return NULL;
}

wink_status_t pal_os_mutex_lock(pal_os_mutex_t mutex, uint32_t timeout_ms) {
    wasm_mutex_t* m = wasm_mtx_from_handle(mutex);
    if (m == NULL) return WINK_ERR_INVALID_ARG;
    assert(!s_sim_in_isr && "pal_os_mutex_lock called from ISR context");

    uint32_t self = sim_scheduler_current_id();
    if (self == SIM_SCHED_NO_READY) {
        if (m->owner == WASM_MUTEX_NO_OWNER) {
            m->owner = self;
            return WINK_OK;
        }
        return WINK_ERR_TIMEOUT;
    }

    if (m->owner == WASM_MUTEX_NO_OWNER) {
        m->owner = self;
        return WINK_OK;
    }

    if (m->owner == self) {
        return WINK_ERR_BUSY;
    }

    uint64_t timeout_us = (timeout_ms == WINK_MUTEX_WAIT_FOREVER)
                           ? 0ULL
                           : (uint64_t)timeout_ms * 1000ULL;
    uint32_t resource_id = (uint32_t)((uintptr_t)mutex);
    wq_push(m, self);
    sim_scheduler_block(self, resource_id, pal_os_get_us(), timeout_us);
    sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
    assert(cur_ctx != NULL);
    sim_ctx_switch(cur_ctx, s_main_ctx);

    sim_task_t const* t = sim_scheduler_get(self);
    if (t->timeout_fired) {
        wq_remove(m, self);
        return WINK_ERR_TIMEOUT;
    }
    if (m->owner != self) {
        return WINK_ERR_IO;
    }
    return WINK_OK;
}

wink_status_t pal_os_mutex_unlock(pal_os_mutex_t mutex) {
    wasm_mutex_t* m = wasm_mtx_from_handle(mutex);
    if (m == NULL) return WINK_ERR_INVALID_ARG;

    uint32_t self = sim_scheduler_current_id();
    if (m->owner == WASM_MUTEX_NO_OWNER) {
        return WINK_ERR_INVALID_STATE;
    }
    if (m->owner != self && !(m->owner == SIM_SCHED_NO_READY && self == SIM_SCHED_NO_READY)) {
        return WINK_ERR_PERMISSION;
    }

    uint32_t waiter;
    if (wq_pop(m, &waiter)) {
        m->owner = waiter;
        sim_scheduler_resume(waiter);
    } else {
        m->owner = WASM_MUTEX_NO_OWNER;
    }
    return WINK_OK;
}

void pal_os_mutex_destroy(pal_os_mutex_t mutex) {
    wasm_mutex_t* m = wasm_mtx_from_handle(mutex);
    if (m == NULL) return;
    assert(m->owner == WASM_MUTEX_NO_OWNER && m->w_count == 0 &&
           "pal_os_mutex_destroy: mutex still held or has waiters");
    m->used = false;
}

/* ---------------------------------------------------------
 * Binary Semaphore
 * --------------------------------------------------------- */

pal_os_sem_t pal_os_sem_create(void) {
    for (uint32_t i = 0; i < WASM_SEM_POOL_SIZE; ++i) {
        if (!s_sem_pool[i].used) {
            wasm_sem_t* s = &s_sem_pool[i];
            s->used = true;
            s->count = 0;
            s->w_head = 0; s->w_tail = 0; s->w_count = 0;
            return (pal_os_sem_t)(uintptr_t)(i + 1);
        }
    }
    return NULL;
}

wink_status_t pal_os_sem_take(pal_os_sem_t sem, uint32_t timeout_ms) {
    wasm_sem_t* s = wasm_sem_from_handle(sem);
    if (s == NULL) return WINK_ERR_INVALID_ARG;
    assert(!pal_os_in_sim_isr_context() && "pal_os_sem_take called from ISR context");

    uint32_t self = sim_scheduler_current_id();
    uint64_t now = pal_os_get_us();

    if (s->count > 0) {
        s->count = 0;
        return WINK_OK;
    }
    if (self == SIM_SCHED_NO_READY) {
        return WINK_ERR_TIMEOUT;
    }

    uint64_t timeout_us = (timeout_ms == WINK_MUTEX_WAIT_FOREVER)
                           ? 0ULL : (uint64_t)timeout_ms * 1000ULL;
    uint32_t resource_id = (uint32_t)((uintptr_t)sem + 0x10000);
    sq_push(s, self);
    sim_scheduler_block(self, resource_id, now, timeout_us);
    sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
    assert(cur_ctx != NULL);
    sim_ctx_switch(cur_ctx, s_main_ctx);

    const sim_task_t* t = sim_scheduler_get(self);
    if (t->timeout_fired) {
        sq_remove(s, self);
        return WINK_ERR_TIMEOUT;
    }
    return WINK_OK;
}

wink_status_t pal_os_sem_give(pal_os_sem_t sem) {
    wasm_sem_t* s = wasm_sem_from_handle(sem);
    if (s == NULL) return WINK_ERR_INVALID_ARG;

    uint32_t next_task = 0;
    if (sq_pop(s, &next_task)) {
        sim_scheduler_resume(next_task);
        return WINK_OK;
    }
    s->count = 1;
    return WINK_OK;
}

wink_status_t pal_os_sem_give_isr(pal_os_sem_t sem) {
    return pal_os_sem_give(sem);
}

void pal_os_sem_destroy(pal_os_sem_t sem) {
    wasm_sem_t* s = wasm_sem_from_handle(sem);
    if (s != NULL) {
        s->used = false;
    }
}

pal_os_reset_reason_t pal_os_get_reset_reason(void) { return PAL_OS_RESET_REASON_UNKNOWN; }
uint32_t pal_os_get_abnormal_boot_count(void) { return 0; }
void pal_os_set_abnormal_boot_count(uint32_t count) { (void)count; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_init(uint32_t timeout_ms) { (void)timeout_ms; return WINK_ERR_UNSUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_os_wdt_feed(void) { return WINK_ERR_UNSUPPORTED; }

/* ---------------------------------------------------------
 * Critical Section (Explicit task/ISR dual-entry dispatch, ADR-0016)
 * --------------------------------------------------------- */

void pal_os_set_sim_isr_context(bool in_isr) { s_sim_in_isr = in_isr; }
bool pal_os_in_sim_isr_context(void) { return s_sim_in_isr; }
bool pal_os_in_isr(void) { return pal_os_in_sim_isr_context(); }

void pal_os_set_sim_pt_context(bool in_pt) { s_sim_in_pt = in_pt; }
bool pal_os_in_sim_pt_context(void) { return s_sim_in_pt; }
bool wink_pt_in_context(void) { return s_sim_in_pt; }

uint32_t pal_os_critical_enter(void) {
    assert(!s_sim_in_isr && "pal_os_critical_enter called from ISR context; use pal_os_critical_enter_isr (ADR-0016)");
    return 0;
}

void pal_os_critical_exit(uint32_t key) {
    (void)key;
    assert(!s_sim_in_isr && "pal_os_critical_exit called from ISR context (ADR-0016)");
}

uint32_t pal_os_critical_enter_isr(void) {
    assert(s_sim_in_isr && "pal_os_critical_enter_isr called from task context; use pal_os_critical_enter (ADR-0016)");
    return 0;
}

void pal_os_critical_exit_isr(uint32_t key) {
    (void)key;
    assert(s_sim_in_isr && "pal_os_critical_exit_isr called from task context (ADR-0016)");
}

/* ---------------------------------------------------------
 * Task Creation (WASM single-threaded simulation implementation)
 * --------------------------------------------------------- */

wink_status_t pal_os_task_create(
    void (*func)(void*), const char* name, uint32_t stack_depth,
    void* arg, int32_t priority, pal_os_core_id_t core_id,
    pal_os_task_handle_t* task_handle)
{
    uint32_t id;
    wink_status_t st = sim_scheduler_register(
        func, arg, name, priority, (int32_t)core_id, stack_depth, &id);
    if (st != WINK_OK) return st;
    if (task_handle) *task_handle = (pal_os_task_handle_t)(uintptr_t)(id + 1);
    return WINK_OK;
}

void pal_os_task_delete(pal_os_task_handle_t handle) {
    if (handle == NULL) {
        uint32_t cur = sim_scheduler_current_id();
        sim_ctx_t* cur_ctx = sim_scheduler_current_ctx();
        assert(cur_ctx != NULL && "self-delete outside task fiber context");
        sim_scheduler_mark_zombie(cur);
        sim_ctx_switch(cur_ctx, s_main_ctx);
    } else {
        uint32_t id = (uint32_t)(uintptr_t)handle - 1;
        sim_scheduler_mark_zombie(id);
    }
}

uint32_t pal_os_get_free_heap_size(void) { return 0u; }
uint32_t pal_os_get_min_free_heap_size(void) { return 0u; }
uint32_t pal_os_get_current_task_stack_free(void) { return 0u; }

static inline uint64_t wasm_wall_clock_us(void) {
    return (uint64_t)(emscripten_get_now() * 1000.0);
}

wink_status_t pal_sim_scheduler_run(const struct wink_app_callbacks* callbacks,
                                    uint32_t main_task_id, uint32_t max_ticks) {
    if (s_scheduler_running) {
        return WINK_OK;
    }
    s_scheduler_running = true;

    if (callbacks == NULL) {
        s_scheduler_running = false;
        return WINK_ERR_INVALID_ARG;
    }

    if (s_main_ctx == NULL) {
        s_main_ctx = sim_ctx_from_current();
    }
    wink_sim_mode_init_from_env();
    pal_wasm_clear_fault_latch();
    pal_wasm_fault_set_callbacks(callbacks);
    uint32_t ticks_run = 0;

    uint64_t wcet_threshold_us = WINK_SIM_TASK_WCET_THRESHOLD_US;
    const char* env_thr = getenv("WINK_SIM_WCET_THRESHOLD_US");
    if (env_thr) {
        wcet_threshold_us = strtoull(env_thr, NULL, 10);
    } else if (getenv("CI") != NULL) {
        wcet_threshold_us *= 10ULL;
    }
    bool bypass_wcet = (getenv("WINK_SIM_BYPASS_WCET") != NULL);
#if defined(__EMSCRIPTEN__)
    if (getenv("WINK_SIM_ENFORCE_WCET") == NULL) {
        bypass_wcet = true;
    }
#endif
    if (s_sim_mode == WINK_SIM_MODE_HEADLESS) {
        bypass_wcet = true;
    }

    sim_scheduler_set_current(SIM_SCHED_NO_READY);

    while (1) {
        pal_wasm_dispatch_pending_interrupts();
        sim_scheduler_gc_zombies();

        if (main_task_id != SIM_SCHED_NO_READY) {
            const sim_task_t* main_task = sim_scheduler_get(main_task_id);
            if (main_task->state == SIM_TASK_STATE_TERMINATED) {
                break;
            }
        }
        if (max_ticks > 0 && ticks_run >= max_ticks) {
            break;
        }

        uint64_t now = pal_os_get_us();
        sim_scheduler_wakeup_by_time(now);

        uint32_t next = sim_scheduler_pick_next();
        if (next == SIM_SCHED_NO_READY) {
            uint64_t wake = sim_scheduler_next_wakeup_us();
            if (wake == UINT64_MAX) break;

            now = pal_os_get_us();
            if (wake > now) {
                if (s_sim_mode == WINK_SIM_MODE_HEADLESS) {
                    wink_vclock_advance_internal(wake - now);
                } else {
                    uint32_t sleep_ms = (uint32_t)((wake - now + 999) / 1000);
                }
            }
            if (max_ticks > 0) {
                ticks_run++;
                break;
            }
            continue;
        }

        sim_scheduler_set_current(next);
        const sim_task_t* t = sim_scheduler_get(next);
        uint64_t wall_start_us = wasm_wall_clock_us();
        sim_ctx_switch(s_main_ctx, t->ctx);
        sim_scheduler_set_current(SIM_SCHED_NO_READY);
        uint64_t duration_us = wasm_wall_clock_us() - wall_start_us;

        if (!bypass_wcet && duration_us > wcet_threshold_us) {
            pal_wasm_invoke_fault(8002);
        }

        if (next == main_task_id) {
            ticks_run++;
        }
    }

    sim_scheduler_gc_zombies();
    sim_scheduler_set_current(SIM_SCHED_NO_READY);
    s_scheduler_running = false;
    return WINK_OK;
}
