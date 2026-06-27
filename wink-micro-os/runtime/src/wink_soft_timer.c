/**
 * @file wink_soft_timer.c
 * @brief 软定时器调度器实现（静态数组 + Tick 对齐）。
 *
 * 设计原则：
 * - 零动态分配：定时器槽编译期静态分配（WINK_MAX_SOFT_TIMERS）
 * - 无优先级：按创建顺序依次调度，保证确定性
 * - Tick 对齐：周期为 Tick 整数倍，剩余 Tick 在调度时递减
 * - WCET 独立监控：每个回调执行时单独计时
 */
#include "wink_soft_timer.h"
#include "wink_runtime.h"
#include "pal_osal.h"
#include "wink_trace.h"
#include <string.h>

/** @brief 单个软定时器控制块（TCB） */
typedef struct {
    wink_soft_timer_callback_t callback;  /**< 回调函数，NULL 表示槽空闲 */
    void*                       arg;       /**< 用户上下文 */
    wink_timer_mode_t           mode;      /**< 单次/周期模式 */
    uint32_t                    period_ticks;  /**< 周期（Tick 数） */
    uint32_t                    remaining_ticks; /**< 剩余到期 Tick 数 */
    uint8_t                     active;    /**< 1 = 运行中，0 = 已停止 */
} wink_timer_cb_t;

/* 静态定时器数组（零动态分配） */
static wink_timer_cb_t s_timers[WINK_MAX_SOFT_TIMERS];
static uint8_t s_initialized = 0;

/* ============================================================
 *  Public API Implementation
 * ============================================================ */

wink_status_t wink_soft_timer_init(void) {
    memset(s_timers, 0, sizeof(s_timers));
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
    s_timers[i].mode            = mode;
    s_timers[i].period_ticks    = period_ticks;
    s_timers[i].remaining_ticks = period_ticks;
    s_timers[i].active          = 0;  /* 创建后默认 STOPPED */

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
    return WINK_OK;
}

wink_status_t wink_soft_timer_stop(int32_t handle) {
    if (!s_initialized || handle < 0 || handle >= (int32_t)WINK_MAX_SOFT_TIMERS) {
        return WINK_ERR_INVALID_ARG;
    }

    s_timers[handle].active = 0;
    return WINK_OK;
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
         *  定时器到期：执行回调 + 独立 WCET 监控
         * ============================================================ */
        start_us   = pal_get_us();
        status     = timer->callback(timer->arg);
        elapsed_us = pal_get_us() - start_us;

        /* 单个回调 WCET 超限警告（50% Tick 阈值） */
        if (elapsed_us > (WINK_RUNTIME_TICK_MS * 1000U / 2U)) {
            wink_trace_fault(WINK_WARN_WCET_EXCEEDED);
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
