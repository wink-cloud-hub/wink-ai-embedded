/**
 * @file wink_trace.c
 * @brief Golden Trace 实现：静态环形缓冲（零动态分配）。
 */
#include "wink_trace.h"
#include "pal_osal.h"

/* INVARIANT: Thread-safe / ISR-safe.
   各函数操作共享静态变量，内部均通过 PAL OSAL 临界区保护，支持多任务与中断并发调用。 */
static uint32_t s_buffer[WINK_TRACE_CAPACITY];
static uint32_t s_count = 0;     /* 已写入总数（含覆盖） */
static uint32_t s_head = 0;      /* 下一个写入位置 */

void wink_trace_reset(void) {
    uint32_t key = pal_os_critical_enter();
    s_count = 0;
    s_head = 0;
    pal_os_critical_exit(key);
}

void wink_trace_fault(uint32_t fault_code) {
    uint32_t key = pal_os_critical_enter();
    s_buffer[s_head] = fault_code;
    s_head = (s_head + 1u) % WINK_TRACE_CAPACITY;
    s_count++;                   /* 溢出回绕由 count() 截断 */
    pal_os_critical_exit(key);
}

uint32_t wink_trace_count(void) {
    uint32_t key = pal_os_critical_enter();
    uint32_t count = (s_count < WINK_TRACE_CAPACITY) ? s_count : WINK_TRACE_CAPACITY;
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
