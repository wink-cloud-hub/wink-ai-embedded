/**
 * @file wink_runtime.h
 * @brief OS 主循环入口 + app-facing fault/task helpers.
 *
 * 各 target 的 *_entry.c 实例化 wink_app_callbacks_t 后调用 wink_runtime_run。
 * 调度器仅用 PAL OSAL 做 tick，挂起语义由 target 实现（ADR-0002 双 target 对齐落点）。
 *
 * Tick 周期 SSOT：优先从 wink_config.h 获取（由 wink_app.json codegen 生成），
 * 若未定义则退化为 10ms 兼容默认值。
 */
#ifndef WINK_RUNTIME_H
#define WINK_RUNTIME_H

#include "wink_app.h"
#include "wink_fault.h"
#include "wink_config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Portable noreturn attribute (GCC/Clang __attribute__ vs MSVC __declspec). */
#ifndef WINK_NORETURN
#  if defined(__GNUC__) || defined(__clang__)
#    define WINK_NORETURN __attribute__((noreturn))
#  elif defined(_MSC_VER)
#    define WINK_NORETURN __declspec(noreturn)
#  else
#    define WINK_NORETURN
#  endif
#endif

/**
 * @brief 运行 OS 主循环
 * @param callbacks App 生命周期回调（NULL 返回 WINK_ERR_INVALID_ARG）
 * @param max_ticks 最多跑多少个 loop tick；传 0 表示无限循环（真机/wasm）
 * @return WINK_OK（max_ticks>0 跑完后）或 WINK_ERR_INVALID_ARG / WINK_ERR_LOCKED
 * @note host/测试传有限 max_ticks 避免 while(1) 卡死
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_runtime_run(const wink_app_callbacks_t *callbacks, uint32_t max_ticks);

/* ── Boot safe-lock 常量（ADR-0010） ────────────────────── */
/** @brief 连续异常复位锁死阈值：达此值才锁死；单次/偶发复位自动恢复 */
#define WINK_BOOT_LOCK_THRESHOLD    3u
/** @brief 健康里程碑 tick 数（≈2s @默认 10ms tick）：init 成功且跑满则清零异常复位计数 */
#define WINK_BOOT_HEALTHY_TICKS     200u

/* ── Fault reporting (app-facing) ───────────────────────── */
/**
 * @brief Raise a fatal fault from app code or DAL.
 *
 * Sequences: wink_trace_fault(code) → wink_actuator_safe_off_all() →
 * callbacks->on_fault / on_fault_status.
 *
 * Unlike wink_trace_fault() (logging only), this function triggers full
 * fail-safe processing.  Call this when an unrecoverable error is detected
 * (init failure, critical sensor fault, etc.).
 *
 * Thread/ISR safety: safe to call from task context.  From ISR context use
 * wink_trace_fault_from_isr() and defer the raise to task context.
 */
void wink_runtime_raise_fault(uint32_t fault_code);

/**
 * @brief Internal entry used by wink_runtime_run itself; same action but
 *        with explicit callbacks pointer (used before s_active_cbs is set).
 */
void wink_runtime_fault(const wink_app_callbacks_t *callbacks, uint32_t fault_code);

/* ── Runtime statistics ─────────────────────────────────── */
typedef struct {
    uint32_t uptime_ms;
    uint32_t free_heap;
    uint32_t min_free_stack;
    uint32_t fault_count;
    uint32_t warn_count;
    uint32_t abnormal_boot_count;
    wink_reset_reason_t last_reset_reason;
} wink_runtime_stats_t;

/** @brief Snap uptime/heap/faults/warns/stack/abnormal to *out. */
void wink_runtime_get_stats(wink_runtime_stats_t *out);

/**
 * @brief Trigger a controlled WDT reset test (noreturn).
 *
 * Initializes the watchdog with @p timeout_ms, briefly feeds it, then
 * enters an infinite loop to let the WDT expire and reset the chip.
 * Useful for S8 "long-press → WDT test" selftest.
 */
WINK_NORETURN void wink_runtime_trigger_wdt_test(uint32_t timeout_ms);

/**
 * @brief Register a poll callback to be invoked by the runtime every tick
 *        (for DAL auto-poll, e.g. button debounce state machine).
 *
 * This is a DAL-facing API; apps should not call it directly.  Registered
 * fns run in soft-timer/task context before app_loop each tick.
 *
 * @param fn   Poll callback
 * @param ctx  Opaque arg passed to fn
 * @return WINK_OK on registration; WINK_ERR_RESOURCE_EXHAUSTED if table full.
 */
wink_status_t wink_runtime_register_poll(void (*fn)(void *ctx), void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* WINK_RUNTIME_H */
