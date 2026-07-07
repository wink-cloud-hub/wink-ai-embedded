/**
 * @file wink_soft_timer.h
 * @brief 软定时器调度器（ADR-0007 协作式执行模型）。
 *
 * 基于 Tick 的软件定时器，支持：
 * - 单次触发 (ONESHOT) 与周期触发 (PERIODIC)
 * - 静态内存分配，零动态分配（max timers 编译期配置）
 * - 每个回调独立 WCET 监控
 * - Tick 对齐：回调执行时间是 Tick 的整数倍
 *
 * 注意：period_ms 必须是 WINK_RUNTIME_TICK_MS 的整数倍！
 */
#ifndef WINK_SOFT_TIMER_H
#define WINK_SOFT_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 软定时器模式 */
typedef enum {
    WINK_TIMER_ONESHOT  = 0,  /**< 执行一次后停止 */
    WINK_TIMER_PERIODIC = 1,  /**< 周期重复执行 */
} wink_timer_mode_t;

/** @brief 软定时器回调函数类型
 *  @param arg 用户上下文指针
 *  @return WINK_OK 继续运行，非 WINK_OK 停止定时器
 */
typedef wink_status_t (*wink_soft_timer_callback_t)(void* arg);

/**
 * @brief 初始化软定时器子系统
 * @return wink_status_t WINK_OK 成功
 * @note 必须在 wink_runtime_run() 之前调用
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_soft_timer_init(void);

/**
 * @brief 创建新的软定时器
 *
 * @param callback 定时器回调函数（非 NULL）
 * @param arg 回调上下文指针
 * @param mode 定时器模式：单次或周期
 * @param period_ms 周期（毫秒，必须是 WINK_RUNTIME_TICK_MS 的整数倍）
 * @return int32_t >= 0 成功（timer handle），< 0 错误（WINK_ERR_*）
 *
 * @note 定时器创建后处于 STOPPED 状态，需调用 wink_soft_timer_start() 启动
 */
WINK_WARN_UNUSED_RESULT
int32_t wink_soft_timer_create(
    wink_soft_timer_callback_t callback,
    void* arg,
    wink_timer_mode_t mode,
    uint32_t period_ms
);

/**
 * @brief 启动定时器
 * @param handle 定时器句柄（由 create 返回）
 * @return wink_status_t WINK_OK 成功，WINK_ERR_INVALID_ARG 句柄无效
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_soft_timer_start(int32_t handle);

/**
 * @brief 停止定时器
 * @param handle 定时器句柄（由 create 返回）
 * @return wink_status_t WINK_OK 成功，WINK_ERR_INVALID_ARG 句柄无效
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_soft_timer_stop(int32_t handle);

/**
 * @brief Dynamically change a running timer's period (zero-stall).
 *
 * For PERIODIC timers the new period takes effect on the NEXT cycle
 * (i.e., after the running callback returns, remaining_ticks is
 * reloaded from the updated period_ticks).  Safe to call from WITHIN
 * the timer's own callback (ADR-0023 §11 self-set_period re-entrancy).
 *
 * @param handle       Timer handle (from wink_soft_timer_create).
 * @param period_ms    New period in milliseconds (must be >0; tick
 *                     alignment rules identical to create: rounded
 *                     down to tick multiple, minimum 1 tick).
 * @return wink_status_t WINK_OK on success; WINK_ERR_INVALID_ARG on bad
 *                     handle / zero period / unallocated slot.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_soft_timer_change_period(int32_t handle, uint32_t period_ms);

/**
 * @brief 调度到期的定时器回调（主循环每个 Tick 调用一次）
 *
 * 遍历所有活动定时器，递减剩余 Tick，到期则执行回调。
 * 每个回调执行时带独立 WCET 监控。
 * PERIODIC 定时器执行后重新加载周期计数。
 * ONESHOT 定时器执行后自动停止。
 */
void wink_soft_timer_dispatch(void);

/**
 * @brief Return true while we are actively dispatching a LIGHT (soft-timer)
 *        callback on this thread/fiber.
 *
 * WINK_ASSERT_NONBLOCKING() checks this to escalate any blocking call from
 * within a LIGHT callback to a fault (ADR-0017 layer 3 + ADR-0023 §9 three-
 * line defense).
 *
 * Host/wasm: maintained per-fiber by dispatch().  ESP32: LIGHT callbacks
 * run on the tick task, so a simple global bool suffices (not ISR-preemptible
 * at the WCET logging points we use).
 */
bool wink_soft_timer_in_light_dispatch(void);

/**
 * @brief Attach a human-readable name to a timer slot for diagnostics.
 *
 * Called by wink_periodic_start_ex when routing a LIGHT periodic callback
 * through the soft timer, so WCET/LIGHT-blocking fault logs can identify
 * the offending callback.  NULL resets to an empty name.
 */
void wink_soft_timer_set_name(int32_t handle, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* WINK_SOFT_TIMER_H */
