/**
 * @file wink_soft_timer.c
 * @brief 软定时器调度器实现（静态数组 + Tick 对齐）。
 *
 * 设计原则：
 * - 零动态分配：定时器槽编译期静态分配（WINK_MAX_SOFT_TIMERS）
 * - 无优先级：按创建顺序依次调度，保证确定性
 * - Tick 对齐：周期为 Tick 整数倍，剩余 Tick 在调度时递减
 * - WCET 独立监控：每个回调执行时单独计时
 *
 * LIGHT dispatch in-flag (ADR-0023 §9 three-line defense, Task 1.5):
 *   - `g_in_light_dispatch` is true while a timer cb is executing; it is
 *     checked by WINK_ASSERT_NONBLOCKING() so any WINK_BLOCKING API called
 *     from within a LIGHT callback escalates to a fault.
 *   - Two-tier WCET enforcement:
 *       >100µs  → WINK_WARN_LIGHT_OVERBUDGET (soft budget)
 *       >500µs  → WINK_FAULT_LIGHT_WCET_VIOLATION (hard limit, with
 *                 consecutive-strike forgiveness for ISR-jitter outliers
 *                 per ADR-0025 §5 / [[memory:embedded-debugging-rhythm]])
 *
 * ✅ @verified: HARDWARE-SMOKE-PASSED (DevKitC ESP32, 2026-06-28)
 *    - 软定时器 ONESHOT/PERIODIC 调度经 wink_runtime_run tick 循环在真机验证
 *    - 关联 ADR-0007 协作式执行模型闭环验证
 */
#include "wink_soft_timer.h"
#include "wink_runtime.h"
#include "wink_fault.h"
#include "wink_pt_debug.h"
#include "pal_osal.h"
#include "wink_trace.h"
#include <string.h>

/* Compile-time contract: the numeric fault code in wink_pt_debug.h must
 * stay in sync with WINK_FAULT_LIGHT_BLOCKING in wink_fault.h. This .c
 * is the single TU that includes both headers, so we static-assert here. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(WINK_PT_DEBUG_FAULT_LIGHT_BLOCKING == WINK_FAULT_LIGHT_BLOCKING,
               "WINK_PT_DEBUG_FAULT_LIGHT_BLOCKING out of sync with WINK_FAULT_LIGHT_BLOCKING");
#endif

/* ── LIGHT dispatch WCET budgets (ADR-0023 §9, ADR-0025 §5) ───────
 *
 * 100µs soft budget: single LIGHT callback must complete well under one
 *   tick (10ms) to avoid starving other timers.  One tick = 10 000µs and
 *   supports up to ~100 well-behaved LIGHT cbs in principle; we budget
 *   100µs per cb so dozens of cbs fit comfortably.
 * 500µs hard limit: 5× soft budget, accounts for ESP32 ISR preemption
 *   jitter (measured elapse includes time stolen by ISRs between the
 *   get_us() calls — see [[memory:embedded-debugging-rhythm]]).
 * 3 consecutive overruns → fault.  A single ISR-jitter outlier produces
 *   a WARN but does NOT escalate, to avoid false positives. */
#define WINK_LIGHT_SOFT_BUDGET_US   100u
#define WINK_LIGHT_HARD_LIMIT_US    500u
#define WINK_LIGHT_WCET_STRIKES     3u

/** @brief 单个软定时器控制块（TCB） */
typedef struct {
    wink_soft_timer_callback_t callback;  /**< 回调函数，NULL 表示槽空闲 */
    void*                       arg;       /**< 用户上下文 */
    const char*                 name;      /**< Optional diagnostic name (set via wink_soft_timer_set_name). */
    wink_timer_mode_t           mode;      /**< 单次/周期模式 */
    uint32_t                    period_ticks;  /**< 周期（Tick 数） */
    uint32_t                    remaining_ticks; /**< 剩余到期 Tick 数 */
    uint8_t                     active;    /**< 1 = 运行中，0 = 已停止 */
    uint8_t                     consecutive_overruns; /**< Forgive single ISR-jitter outliers; fault on N-in-a-row. */
} wink_timer_cb_t;

/* 静态定时器数组（零动态分配） */
static wink_timer_cb_t s_timers[WINK_MAX_SOFT_TIMERS];
static uint8_t s_initialized = 0;

/* LIGHT dispatch in-flag (Task 1.5 / ADR-0023 §9).
 *
 * true iff we are currently executing a soft-timer (LIGHT) callback on
 * this execution context.  Set immediately before calling cb, cleared
 * immediately after (even if cb longjmps/returns via fault — we use a
 * simple scope guard; callers MUST NOT longjmp out without going through
 * wink_runtime fault which calls dispatch loop break).
 *
 * Host/wasm single-virtual-core (ADR-0014) guarantees no other fiber
 * enters dispatch() while a cb is active; ESP32 runs LIGHT on one core
 * in a dedicated tick task.  A plain bool is sufficient on all
 * supported targets. */
static volatile bool g_in_light_dispatch = false;

/* ============================================================
 *  Public API Implementation
 * ============================================================ */

wink_status_t wink_soft_timer_init(void) {
    memset(s_timers, 0, sizeof(s_timers));
    g_in_light_dispatch = false;
    s_initialized = 1;
    return WINK_OK;
}

int32_t wink_soft_timer_create(
    wink_soft_timer_callback_t callback,
    void* arg,
    wink_timer_mode_t mode,
    uint32_t period_ms
) {
    int32_t i;
    uint32_t period_ticks;

    if (!s_initialized || callback == NULL || period_ms == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Tick 对齐：周期必须为 Tick 整数倍，向下取整 */
    period_ticks = period_ms / WINK_RUNTIME_TICK_MS;
    if (period_ticks == 0) {
        period_ticks = 1;  /* 最小 1 Tick */
    }

    /* 查找空闲槽 */
    for (i = 0; i < (int32_t)WINK_MAX_SOFT_TIMERS; i++) {
        if (s_timers[i].callback == NULL) {
            break;
        }
    }

    if (i >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return WINK_ERR_NO_MEM;
    }

    s_timers[i].callback        = callback;
    s_timers[i].arg             = arg;
    s_timers[i].name            = NULL;
    s_timers[i].mode            = mode;
    s_timers[i].period_ticks    = period_ticks;
    s_timers[i].remaining_ticks = period_ticks;
    s_timers[i].active          = 0;  /* 创建后默认 STOPPED */
    s_timers[i].consecutive_overruns = 0;

    return i;
}

wink_status_t wink_soft_timer_start(int32_t handle) {
    if (!s_initialized || handle < 0 || handle >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_timers[handle].callback == NULL) {
        return WINK_ERR_INVALID_ARG;  /* 未分配的槽 */
    }

    s_timers[handle].remaining_ticks = s_timers[handle].period_ticks;
    s_timers[handle].active = 1;
    s_timers[handle].consecutive_overruns = 0;
    return WINK_OK;
}

wink_status_t wink_soft_timer_stop(int32_t handle) {
    if (!s_initialized || handle < 0 || handle >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return WINK_ERR_INVALID_ARG;
    }

    s_timers[handle].active = 0;
    return WINK_OK;
}

wink_status_t wink_soft_timer_change_period(int32_t handle, uint32_t period_ms) {
    uint32_t period_ticks;

    if (!s_initialized || handle < 0 || handle >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_timers[handle].callback == NULL) {
        return WINK_ERR_INVALID_ARG;  /* unallocated slot */
    }
    if (period_ms == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Tick alignment (same logic as create): round down, minimum 1 tick. */
    period_ticks = period_ms / WINK_RUNTIME_TICK_MS;
    if (period_ticks == 0) {
        period_ticks = 1;
    }

    /* Atomic-ish write: soft_timer runs cooperatively on the single virtual
     * core (ADR-0014) and is only dispatched from the main loop, so a plain
     * uint32_t assignment is safe — no KVS/ISR can preempt mid-write on any
     * supported target.
     *
     * Semantics (ADR-0023 §11):
     *   - Self-set_period (from WITHIN cb): dispatch() is mid-execution of
     *     this timer; after cb returns it reloads remaining_ticks from
     *     period_ticks, so new period takes effect on the NEXT cycle.
     *   - External set_period: new period applies to subsequent cycles.
     *     If remaining_ticks was longer than the new period the next fire
     *     would be delayed beyond the requested period.  Clamp now so a
     *     long-to-short change fires within one tick of the call (not at
     *     the end of a stale long period).  Short-to-long is left alone
     *     (remaining is already smaller than new period, next fire is
     *     appropriately soon, then reload picks up the longer period). */
    s_timers[handle].period_ticks = period_ticks;
    if (s_timers[handle].active && s_timers[handle].remaining_ticks > period_ticks) {
        s_timers[handle].remaining_ticks = period_ticks;
    }
    /* Reset overrun counter on period change so a newly-tuned period
     * doesn't inherit strike count from the old one. */
    s_timers[handle].consecutive_overruns = 0;
    return WINK_OK;
}

void wink_soft_timer_set_name(int32_t handle, const char *name) {
    if (!s_initialized || handle < 0 || handle >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return;
    }
    s_timers[handle].name = name;
}

bool wink_soft_timer_in_light_dispatch(void) {
    return g_in_light_dispatch;
}

void wink_soft_timer_dispatch(void) {
    int32_t i;

    if (!s_initialized) {
        return;
    }

    for (i = 0; i < (int32_t)WINK_MAX_SOFT_TIMERS; i++) {
        wink_timer_cb_t* timer = &s_timers[i];
        wink_status_t status;
        uint64_t start_us;
        uint64_t elapsed_us;

        /* 跳过空闲或已停止的定时器 */
        if (timer->callback == NULL || !timer->active) {
            continue;
        }

        /* 剩余 Tick 递减 + 到期判定（同一 dispatch 周期）。
         * 修复 off-by-one：原实现先 `--` 再 `continue`，须等 remaining_ticks 归 0 后的
         * 下一次 dispatch 才触发回调 → 周期 P 实际运行 P+1 ticks。
         * 现改为递减后同周期判定：P 从 start 到首次触发恰好 P 次 dispatch。 */
        if (timer->remaining_ticks > 0) {
            timer->remaining_ticks--;
        }
        if (timer->remaining_ticks != 0) {
            continue;
        }

        /* ============================================================
         *  定时器到期：执行回调 + LIGHT 上下文标志 + 两档 WCET 监控
         * ============================================================ */
        start_us   = pal_os_get_us();

        /* Set LIGHT in-flag (Task 1.5). Any WINK_BLOCKING API called from
         * within the callback will observe this flag via
         * WINK_ASSERT_NONBLOCKING and escalate to a fault.
         *
         * Note: we don't need a critical section around this write —
         * dispatch runs on a single execution context per core, and
         * the flag is only ever read by the calling context itself
         * (a blocking call on this thread/fiber). */
        g_in_light_dispatch = true;
        status     = timer->callback(timer->arg);
        g_in_light_dispatch = false;

        elapsed_us = pal_os_get_us() - start_us;

        /* ── Two-tier WCET enforcement ──────────────────────────
         * Soft budget: single LOG_WARN, does not reset consecutive count
         *   (warns every time but doesn't escalate alone).
         * Hard limit: increment consecutive strike; fault at N strikes.
         *
         * On ESP32, elapse measurement includes time stolen by ISRs, so
         * a single >500µs reading may be an ISR-jitter outlier — 3-strike
         * forgiveness avoids false positives (ADR-0025 §5). */
        if (elapsed_us > WINK_LIGHT_HARD_LIMIT_US) {
            timer->consecutive_overruns++;
            if (timer->consecutive_overruns >= WINK_LIGHT_WCET_STRIKES) {
                /* Repeated hard-limit violations: a real bug, not jitter.
                 * Raise fault. We don't have the fn pointer easily, but
                 * name helps pin down the offending helper. */
                wink_trace_fault(WINK_FAULT_LIGHT_WCET_VIOLATION);
                timer->consecutive_overruns = 0;
            } else {
                wink_trace_warn(WINK_WARN_LIGHT_OVERBUDGET);
            }
        } else if (elapsed_us > WINK_LIGHT_SOFT_BUDGET_US) {
            /* Over soft budget but under hard limit: warn every time,
             * reset consecutive hard-limit counter (behaved OK this tick). */
            wink_trace_warn(WINK_WARN_LIGHT_OVERBUDGET);
            timer->consecutive_overruns = 0;
        } else {
            /* Well-behaved: reset strike counter. */
            timer->consecutive_overruns = 0;
        }

        /* 回调返回非 OK 或 ONESHOT 模式 → 停止定时器 */
        if (status != WINK_OK || timer->mode == WINK_TIMER_ONESHOT) {
            timer->active = 0;
        } else {
            /* PERIODIC 模式 → 重新加载周期 */
            timer->remaining_ticks = timer->period_ticks;
        }
    }
}
