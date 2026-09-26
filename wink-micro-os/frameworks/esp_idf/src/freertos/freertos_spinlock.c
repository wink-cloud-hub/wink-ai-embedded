/* SPDX-License-Identifier: LGPL-3.0-only
 *
 * freertos_spinlock.c — Phase 2 ESP-IDF simulation spinlock state tracking
 *
 * Implements:
 *   - portMUX_TYPE owner + nesting-depth bookkeeping       (ISSUE-02, Task 1.3)
 *   - Critical-section illegal-yield assertion gate        (ISSUE-02, Task 1.3)
 *   - Per-fiber busy-wait spin counter + self-healing      (ISSUE-06, Task 3.1)
 *
 * Design constraints (ADR-0012, ADR-0014, ADR-0045, ADR-0072):
 *   - Zero dynamic allocation: all state in static arrays.
 *   - Single virtual core: no physical races; spinlock is a boundary marker.
 *   - Self-healing yields MUST advance virtual clock (never "empty yield").
 *   - Cold-start safe: arrays are zero-initialized by C runtime.
 */
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "freertos/portmacro.h"
#include "freertos_sync.h"
#include "wink_sim_scheduler.h"
#include "pal_log.h"
#include "pal_osal.h"

/* ── Slot count ─────────────────────────────────────────────────────────────
 * Slot 0 is reserved for the scheduler main fiber; tasks occupy slots 1..N.
 * Both critical-depth and spin-count arrays use the same size to keep the
 * boundary checks uniform (P2/I2 fix from plan review).                   */
#define ESP_FREERTOS_TASK_SLOT_COUNT (FREERTOS_MAX_TASKS + 1u)

/* ── Critical-section depth table (ISSUE-02) ────────────────────────────── */
static uint32_t s_task_critical_depth[ESP_FREERTOS_TASK_SLOT_COUNT];

/* ── Busy-wait spin counter table (ISSUE-06) ────────────────────────────── */
#define ESP_SIM_SPIN_WAIT_THRESHOLD   500u
#define ESP_SIM_SPIN_ADVANCE_TIME_US  10u

static uint32_t s_fiber_spin_count[ESP_FREERTOS_TASK_SLOT_COUNT];

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint32_t current_slot(void) {
    uint32_t id = sim_scheduler_current_id();
    if (id == SIM_SCHED_NO_READY) {
        return 0u; /* Slot 0 is reserved for scheduler main fiber / test thread */
    }
    return (id + 1u < ESP_FREERTOS_TASK_SLOT_COUNT) ? (id + 1u) : 0u;
}

static inline bool slot_valid(uint32_t slot) {
    return slot < ESP_FREERTOS_TASK_SLOT_COUNT;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Task 1.3: portMUX_TYPE spinlock operations
 * ═══════════════════════════════════════════════════════════════════════════ */

void vPortEnterCritical(portMUX_TYPE *mux) {
    if (mux == NULL) {
        pal_log_e("SPINLOCK", "vPortEnterCritical: NULL mux");
        assert(mux != NULL);
        return;
    }

    uint32_t cur = current_slot();

    if (mux->owner == portMUX_NO_OWNER) {
        /* First acquisition: claim the mux */
        mux->owner = cur;
        mux->count = 1u;
    } else if (slot_valid(cur) && mux->owner == cur) {
        /* Re-entrant acquisition by same owner: increment depth */
        mux->count++;
    } else {
        /* In cooperative single-core scheduling, another task holding a mux
         * while we are running means that task yielded while inside a critical
         * section — an architectural violation. Fail-loud per ADR-0012. */
        pal_log_e("SPINLOCK",
                  "vPortEnterCritical: mux already owned by slot %u (cur=%u). "
                  "Task yielded inside critical section — arch violation!",
                  (unsigned)mux->owner, (unsigned)cur);
        assert(0 && "portMUX_TYPE cross-task ownership: illegal yield inside critical section");
        return;
    }

    /* Track per-task critical depth for yield-gate queries */
    if (slot_valid(cur)) {
        s_task_critical_depth[cur]++;
    }
}

void vPortExitCritical(portMUX_TYPE *mux) {
    if (mux == NULL) {
        pal_log_e("SPINLOCK", "vPortExitCritical: NULL mux");
        assert(mux != NULL);
        return;
    }

    uint32_t cur = current_slot();

    if (mux->owner == portMUX_NO_OWNER) {
        pal_log_e("SPINLOCK", "vPortExitCritical: mux not owned (double-release?)");
        assert(0 && "vPortExitCritical: releasing unheld mux");
        return;
    }

    if (slot_valid(cur) && mux->owner != cur) {
        pal_log_e("SPINLOCK",
                  "vPortExitCritical: owner mismatch — owned by slot %u, released by slot %u",
                  (unsigned)mux->owner, (unsigned)cur);
        assert(0 && "vPortExitCritical: non-owner release");
        return;
    }

    if (mux->count > 0u) {
        mux->count--;
    }
    if (mux->count == 0u) {
        mux->owner = portMUX_NO_OWNER;
    }

    /* Decrement per-task depth */
    if (slot_valid(cur) && s_task_critical_depth[cur] > 0u) {
        s_task_critical_depth[cur]--;
    }
}

/* M1: ISR variants — independent function bodies for Phase 3 divergence.
 * In Phase 2 single-virtual-core there is no concurrent ISR; reuse task logic. */
void vPortEnterCritical_ISR(portMUX_TYPE *mux) {
    vPortEnterCritical(mux);
}

void vPortExitCritical_ISR(portMUX_TYPE *mux) {
    vPortExitCritical(mux);
}

/* Safe variants: in Phase 2 context-detection is trivial (always task ctx). */
void vPortEnterCriticalSafe(portMUX_TYPE *mux) {
    vPortEnterCritical(mux);
}

void vPortExitCriticalSafe(portMUX_TYPE *mux) {
    vPortExitCritical(mux);
}

/* ── Public query API ───────────────────────────────────────────────────── */

uint32_t esp_freertos_get_critical_depth(uint32_t task_id) {
    if (task_id >= ESP_FREERTOS_TASK_SLOT_COUNT) {
        return 0u;
    }
    return s_task_critical_depth[task_id];
}

/**
 * @brief Hard-assert that the calling fiber is NOT inside a portMUX_TYPE
 *        critical section. Called at the entry of every blocking/yield API.
 *
 * Fail-loud: prints a fatal log then asserts. The test harness that expects
 * this path should use WILL_FAIL TRUE (Plan M3 option A) or setjmp capture
 * (Plan M3 option B).
 */
void esp_freertos_assert_not_in_critical(const char *api_name) {
    uint32_t cur = current_slot();
    if (!slot_valid(cur)) {
        return; /* Scheduler not running yet — benign (static constructor path) */
    }
    if (s_task_critical_depth[cur] > 0u) {
        pal_log_e("SPINLOCK",
                  "FATAL: %s called while holding spinlock (slot=%u, depth=%u). "
                  "Blocking inside a critical section is undefined behavior on real SMP ESP32.",
                  api_name ? api_name : "<unknown>",
                  (unsigned)cur,
                  (unsigned)s_task_critical_depth[cur]);
        assert(0 && "Illegal blocking call inside portMUX_TYPE critical section");
    }
}

/** Reset all spinlock state (called from esp_freertos_pools_reset on cold-start). */
void esp_freertos_spinlock_reset(void) {
    for (uint32_t i = 0u; i < ESP_FREERTOS_TASK_SLOT_COUNT; i++) {
        s_task_critical_depth[i] = 0u;
        s_fiber_spin_count[i]    = 0u;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Task 3.1: per-fiber busy-wait spin accounting (ISSUE-06)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Called from read-only high-frequency polling APIs (gpio_get_level,
 *        esp_timer_get_time, esp_rom_delay_us middle-range).
 *
 * When a single fiber has polled more than ESP_SIM_SPIN_WAIT_THRESHOLD times
 * without voluntarily yielding, self-healing kicks in:
 *   1. Advance virtual clock by ESP_SIM_SPIN_ADVANCE_TIME_US (never empty yield).
 *   2. Yield context so other fibers can run.
 * This prevents browser tab freeze / host thread 100% CPU (ADR-0072 spirit).
 */
void esp_sim_spin_wait_account(void) {
    uint32_t cur = current_slot();
    /* P2 fix: guard uses total slot count, not MAX_TASKS, for uniform boundary */
    if (!slot_valid(cur)) {
        return;
    }

    s_fiber_spin_count[cur]++;
    if (s_fiber_spin_count[cur] >= ESP_SIM_SPIN_WAIT_THRESHOLD) {
        s_fiber_spin_count[cur] = 0u;
        /* Advance virtual clock first — zero-advance yields are forbidden */
        pal_os_busy_wait_us(ESP_SIM_SPIN_ADVANCE_TIME_US);
        if (sim_scheduler_current_ctx() != NULL) {
            sim_scheduler_yield_context();
        }
    }
}

/**
 * @brief Reset the busy-wait spin counter for a given task slot.
 *        Must be called on any voluntary yield to prevent false positives.
 *        (I1 fix: explicit reset on both voluntary and forced yield paths.)
 */
void esp_sim_spin_wait_reset(uint32_t task_id) {
    if (task_id == SIM_SCHED_NO_READY) {
        s_fiber_spin_count[0] = 0u;
    } else {
        if (task_id + 1u < ESP_FREERTOS_TASK_SLOT_COUNT) {
            s_fiber_spin_count[task_id + 1u] = 0u;
        }
        if (task_id < ESP_FREERTOS_TASK_SLOT_COUNT) {
            s_fiber_spin_count[task_id] = 0u;
        }
    }
}
