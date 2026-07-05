/**
 * @file wink_trace.c
 * @brief Golden Trace 实现：静态环形缓冲（零动态分配）。
 *
 * INVARIANT（ADR-0016 双入口显式分流后）：
 *   - `wink_trace_fault`         —— TASK 上下文；pal_os_critical_enter/exit 保护；
 *   - `wink_trace_fault_from_isr`—— ISR 上下文；pal_os_critical_enter_isr/exit_isr 保护；
 *     两者共享 `s_record_fault_locked`（相同环形写入逻辑），task/ISR 互斥由 PAL 全局 mux 保证；
 *   - `wink_trace_reset/count/last` —— TASK 上下文（诊断/查询接口，无 ISR 变体）。
 */
#include "wink_trace.h"
#include "pal_osal.h"

static uint32_t s_buffer[WINK_TRACE_CAPACITY];
static uint32_t s_count = 0;     /* 已写入总数（含覆盖） */
static uint32_t s_head = 0;      /* 下一个写入位置 */
static uint32_t s_warn_count = 0; /* 警告计数器（预算/性能类，非致命） */

/* 环形写入的公共逻辑。调用方须已持有 PAL 全局临界区（task 或 ISR 版本任一）。
 * 提炼独立函数为的是保证 task/ISR 两条路径记录同一 fault code 后 buffer/head/count
 * 状态**bit-for-bit 等价**（Task D-2 单测的验收硬门槛）。 */
static inline void s_record_fault_locked(uint32_t fault_code) {
    s_buffer[s_head] = fault_code;
    s_head = (s_head + 1u) % WINK_TRACE_CAPACITY;
    s_count++;                   /* 溢出回绕由 count() 截断 */
}

void wink_trace_reset(void) {
    uint32_t key = pal_os_critical_enter();
    s_count = 0;
    s_head = 0;
    s_warn_count = 0;
    pal_os_critical_exit(key);
}

void wink_trace_fault(uint32_t fault_code) {
    uint32_t key = pal_os_critical_enter();
    s_record_fault_locked(fault_code);
    pal_os_critical_exit(key);
}

void wink_trace_fault_from_isr(uint32_t fault_code) {
    uint32_t key = pal_os_critical_enter_isr();
    s_record_fault_locked(fault_code);
    pal_os_critical_exit_isr(key);
}

uint32_t wink_trace_count(void) {
    uint32_t key = pal_os_critical_enter();
    uint32_t count = (s_count < WINK_TRACE_CAPACITY) ? s_count : WINK_TRACE_CAPACITY;
    pal_os_critical_exit(key);
    return count;
}

void wink_trace_warn(uint32_t warn_code) {
    (void)warn_code; /* 当前只计数，不入环形缓冲 —— 环形缓冲仅供 fault 诊断用 */
    uint32_t key = pal_os_critical_enter();
    s_warn_count++;
    pal_os_critical_exit(key);
}

uint32_t wink_warn_count(void) {
    uint32_t key = pal_os_critical_enter();
    uint32_t count = s_warn_count;
    pal_os_critical_exit(key);
    return count;
}

uint32_t wink_trace_last(void) {
    uint32_t key = pal_os_critical_enter();
    if (s_count == 0) {
        pal_os_critical_exit(key);
        return 0u;
    }
    /* 最近写入在 s_head 的前一个位置 */
    uint32_t last_idx = (s_head + WINK_TRACE_CAPACITY - 1u) % WINK_TRACE_CAPACITY;
    uint32_t last_val = s_buffer[last_idx];
    pal_os_critical_exit(key);
    return last_val;
}
