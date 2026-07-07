/**
 * @file wink_pt_debug.h
 * @brief Protothread 上下文调试探针（ADR-0017 第 3 层：运行期 WINK_ASSERT_NONBLOCKING）。
 *
 * 层级职责修正（2026-07-04）：
 *   本头文件从 pal/include/wink_status.h 迁出。原位置存在层级反转 ——
 *   PAL 契约头文件 forward-declare 了 wink_pt_in_context()（一个 runtime 概念，
 *   由协作式调度器的 SIM/host 语义拥有），破坏了 "PAL 位于依赖栈最底" 的约束。
 *   现在：
 *     - PAL 头**不再**声明 wink_pt_in_context()；
 *     - `WINK_ASSERT_NONBLOCKING()` 宏与该 hook 一起放到 runtime 层；
 *     - 需要 runtime PT-context 断言的 DAL/App 源文件应 #include "wink_pt_debug.h"；
 *     - 未 #include 本头的 TU 里，`WINK_ASSERT_NONBLOCKING()` 未定义 —— 这是
 *       契约缺失的显性化。若旧代码通过 pal/wink_status.h 隐式拿到 no-op 版，
 *       现在需要显式 include 本头（或依赖 wink_runtime.h 的传递 include）。
 *
 * API 稳定性：宏体在 T5 阶段替换（ADR-0017 §阶段二 §5），签名与语义不变。
 */
#ifndef WINK_PT_DEBUG_H
#define WINK_PT_DEBUG_H

#include <stdbool.h>
#include <stdint.h>
#include "wink_status.h"   /* wink_status_t / WINK_ERR_PANIC */

/* Fault code for "blocking API called from LIGHT callback".
 * We do NOT include wink_fault.h here to avoid dragging wink_trace.h into
 * DAL/runtime header include chains (DAL .c files include wink_pt_debug.h
 * but don't always have trace/ on their include path).
 * The numeric value must stay in sync with WINK_FAULT_LIGHT_BLOCKING in
 * runtime/include/wink_fault.h — a WINK_PT_DEBUG build will catch drift
 * via compile-time assert in the runtime .c that includes both. */
#define WINK_PT_DEBUG_FAULT_LIGHT_BLOCKING 8006u

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 当前调用点是否处于 Protothread 上下文中。
 *
 * 各 target 由其 OSAL 实现负责定义：
 *   - host  (targets/host/pal_osal_host.c)  → 追踪 sim scheduler 切换
 *   - wasm  (targets/wasm/pal_osal_wasm.c)  → 同上
 *   - esp32 (targets/esp32/pal_osal_esp32.c) → 固定 false（真机无 PT 仿真）
 *
 * @note 由 runtime 侧 owner，PAL 不得反向 forward-declare（消除层级反转）。
 */
bool wink_pt_in_context(void);

/**
 * @brief 当前调用点是否处于 LIGHT (soft-timer) 回调 dispatch 中。
 *
 * Maintained by soft_timer dispatch: true while a LIGHT callback is
 * executing.  WINK_ASSERT_NONBLOCKING checks this to escalate blocking
 * calls from within LIGHT callbacks into faults (ADR-0023 §9 three-line
 * defense).
 *
 * Forward-declared here to avoid a circular include with wink_soft_timer.h.
 * Defined in runtime/src/wink_soft_timer.c.
 */
bool wink_soft_timer_in_light_dispatch(void);

#ifdef WINK_PT_DEBUG
#include <assert.h>
extern void wink_trace_fault(uint32_t fault_code);
#define WINK_ASSERT_NONBLOCKING() do { \
    if (wink_pt_in_context()) { \
        wink_trace_fault((uint32_t)WINK_ERR_PANIC); \
        assert(!wink_pt_in_context() && "Fatal: Blocking API called within Protothread context!"); \
    } \
    if (wink_soft_timer_in_light_dispatch()) { \
        wink_trace_fault(WINK_PT_DEBUG_FAULT_LIGHT_BLOCKING); \
        assert(!wink_soft_timer_in_light_dispatch() && "Fatal: Blocking API called within LIGHT (soft-timer) callback context!"); \
    } \
} while (0)
#else
#define WINK_ASSERT_NONBLOCKING() ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINK_PT_DEBUG_H */
