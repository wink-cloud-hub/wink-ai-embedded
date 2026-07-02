/**
 * @file wink_trace.h
 * @brief Golden Trace —— 故障/事件记录的一等 peer 层。
 *
 * 定位见 03-directory-architecture.md §4（trace 独立顶层，非 runtime 子特性）。
 * 零动态分配（§6.1 约束1）：内部用静态环形缓冲。
 * 隔离契约（§6.1 约束2）：DAL/PAL 驱动禁调本 API；仅 runtime 调度器与 App 回调调用。
 *
 * 并发契约（ADR-0016 双入口显式分流）：
 *   - `wink_trace_fault`               —— **TASK 上下文**；用 `pal_os_critical_enter/exit`；
 *   - `wink_trace_fault_from_isr`      —— **ISR 上下文**；用 `pal_os_critical_enter_isr/exit_isr`；
 *   - `wink_trace_reset/count/last`    —— **仅诊断/查询**，均为 TASK 上下文（不需要 ISR 变体）。
 *   两条 fault 入口共享同一环形缓冲；task/ISR 之间的互斥由共享全局 mux 保证。
 */
#ifndef WINK_TRACE_H
#define WINK_TRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 环形缓冲容量（静态分配，可按平台调整） */
#ifndef WINK_TRACE_CAPACITY
#define WINK_TRACE_CAPACITY 32
#endif

/**
 * @brief 清空 trace 缓冲（启动/测试前调用）
 * @note Thread-safety: TASK 上下文。使用 pal_os_critical_enter/exit 保护。
 * @warning **不可 ISR 调用**（ADR-0016）。ISR 需要清缓冲时，先延迟到 task 层再调本 API。
 */
void wink_trace_reset(void);

/**
 * @brief 记录一个故障码（TASK 上下文）
 * @param fault_code 业务自定义故障码（由 App/runtime 在 fault 路径上报）
 * @note 满则覆盖最旧记录（环形）
 * @note Thread-safety: TASK 上下文。使用 pal_os_critical_enter/exit 保护。
 * @warning **不可 ISR 调用**（ADR-0016）。ISR 请改用 wink_trace_fault_from_isr()。
 */
void wink_trace_fault(uint32_t fault_code);

/**
 * @brief 记录一个故障码（ISR 上下文, ADR-0016）
 * @param fault_code 业务自定义故障码
 * @note 满则覆盖最旧记录（环形），与 `wink_trace_fault` 共享同一环形缓冲。
 * @note Thread-safety: ISR 上下文。使用 pal_os_critical_enter_isr/exit_isr 保护，
 *       与 TASK 版共享同一 mux → task/ISR 之间的互斥仍然成立。
 * @warning **仅 ISR 上下文调用**。TASK 上下文请改用 wink_trace_fault()——ISR 版在
 *          host/wasm Debug 构建下会 assert `s_sim_in_isr`。
 * @warning ADR-0016 §4.2 约束：ISR 内**禁止**同步调用 `wink_runtime_fault()` 或任何
 *          可能阻塞 / I/O 的用户故障回调。ISR 只做静态日志记录（本函数），真正的
 *          Safe-off 关断链必须延迟到 TASK 层（主 Loop tick 回收期）。
 */
void wink_trace_fault_from_isr(uint32_t fault_code);

/**
 * @brief 当前已记录条数（≤ WINK_TRACE_CAPACITY）
 * @note Thread-safety: TASK 上下文。使用 pal_os_critical_enter/exit 保护。
 * @warning **不可 ISR 调用**（ADR-0016）：诊断/查询接口，ISR 不需要读它。
 */
uint32_t wink_trace_count(void);

/**
 * @brief 最近一条故障码；无记录返回 0
 * @note Thread-safety: TASK 上下文。使用 pal_os_critical_enter/exit 保护。
 * @warning **不可 ISR 调用**（ADR-0016）：诊断/查询接口，ISR 不需要读它。
 */
uint32_t wink_trace_last(void);

#ifdef __cplusplus
}
#endif

#endif /* WINK_TRACE_H */
