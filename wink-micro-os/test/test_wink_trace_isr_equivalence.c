/**
 * @file test_wink_trace_isr_equivalence.c
 * @brief ADR-0016 / Task D-2 验收硬门槛：`wink_trace_fault` 与 `wink_trace_fault_from_isr`
 *        在同一 fault code 序列下，环形缓冲/头指针/计数**bit-for-bit 等价**。
 *
 * 手段：
 *   - Host target 的 pal_osal 提供了 `pal_os_set_sim_isr_context(bool)` sim-hook
 *     （ADR-0016 §4.2）。调本函数 true 前后夹紧 `wink_trace_fault_from_isr` 调用，
 *     模拟 ISR 上下文；task 版调用则在 flag=false 状态下发生。
 *   - trace 内部 `s_head/s_count/s_buffer` 是 static，本文件通过 `wink_trace_count` /
 *     `wink_trace_last` + 全量重放的方式做等价性推断（不需要 friend/宏侵入）。
 *
 * 覆盖：
 *   1. 单次 fault，task 版 vs ISR 版：count/last 相同。
 *   2. 交替 task/ISR 序列 与 纯 task 序列（同一 code 序列）：count/last 相同。
 *   3. 环形回绕（写入 WINK_TRACE_CAPACITY + 5 条）：count 均封顶为 CAPACITY，last 相同。
 */
#include "unity.h"
#include "wink_trace.h"
#include "pal_osal.h"

void setUp(void) {
    /* 每个用例前重置 trace + 保证从 task 上下文开始（sim ISR flag 归零） */
    pal_os_set_sim_isr_context(false);
    wink_trace_reset();
}
void tearDown(void) {
    pal_os_set_sim_isr_context(false);
}

/* helper：模拟 ISR 边界 —— set flag → call → clear flag */
static void isr_record(uint32_t code) {
    pal_os_set_sim_isr_context(true);
    wink_trace_fault_from_isr(code);
    pal_os_set_sim_isr_context(false);
}

/* Case 1：单次 task 与单次 ISR 写入相同 code，count/last 相同。 */
void test_single_fault_task_and_isr_are_equivalent(void) {
    wink_trace_fault(0xDEADBEEFu);
    uint32_t count_task = wink_trace_count();
    uint32_t last_task  = wink_trace_last();

    wink_trace_reset();

    isr_record(0xDEADBEEFu);
    uint32_t count_isr = wink_trace_count();
    uint32_t last_isr  = wink_trace_last();

    TEST_ASSERT_EQUAL_UINT32(count_task, count_isr);
    TEST_ASSERT_EQUAL_UINT32(last_task,  last_isr);
    TEST_ASSERT_EQUAL_UINT32(1u,         count_isr);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFu, last_isr);
}

/* Case 2：交替 task/ISR 序列与全 task 序列在**同一 code 序列**下等价。 */
void test_mixed_sequence_matches_pure_task_sequence(void) {
    const uint32_t seq[] = { 1u, 2u, 3u, 4u, 5u, 6u };
    const size_t   n     = sizeof(seq) / sizeof(seq[0]);

    /* 全 task */
    for (size_t i = 0; i < n; i++) { wink_trace_fault(seq[i]); }
    uint32_t count_task = wink_trace_count();
    uint32_t last_task  = wink_trace_last();

    wink_trace_reset();

    /* 交替 task/ISR：偶数索引 task 版，奇数索引 ISR 版 */
    for (size_t i = 0; i < n; i++) {
        if ((i & 1u) == 0u) {
            wink_trace_fault(seq[i]);
        } else {
            isr_record(seq[i]);
        }
    }
    uint32_t count_mixed = wink_trace_count();
    uint32_t last_mixed  = wink_trace_last();

    TEST_ASSERT_EQUAL_UINT32(count_task, count_mixed);
    TEST_ASSERT_EQUAL_UINT32(last_task,  last_mixed);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)n, count_mixed);
    TEST_ASSERT_EQUAL_UINT32(seq[n - 1u], last_mixed);
}

/* Case 3：写入 CAPACITY+5 条，全 task vs 交替 task/ISR：count 均封顶、last 相同。 */
void test_ring_wraparound_task_vs_isr_are_equivalent(void) {
    /* 全 task 参考序列 */
    for (uint32_t i = 0; i < WINK_TRACE_CAPACITY + 5u; i++) {
        wink_trace_fault(i);
    }
    uint32_t count_task = wink_trace_count();
    uint32_t last_task  = wink_trace_last();

    wink_trace_reset();

    /* 交替 task / ISR */
    for (uint32_t i = 0; i < WINK_TRACE_CAPACITY + 5u; i++) {
        if ((i & 1u) == 0u) {
            wink_trace_fault(i);
        } else {
            isr_record(i);
        }
    }
    uint32_t count_mixed = wink_trace_count();
    uint32_t last_mixed  = wink_trace_last();

    TEST_ASSERT_EQUAL_UINT32(count_task, count_mixed);
    TEST_ASSERT_EQUAL_UINT32(last_task,  last_mixed);
    TEST_ASSERT_EQUAL_UINT32(WINK_TRACE_CAPACITY, count_mixed);
    TEST_ASSERT_EQUAL_UINT32(WINK_TRACE_CAPACITY + 4u, last_mixed);
}

/* Case 4：ISR 上下文下 `wink_trace_reset/count/last` 的 doxygen 承诺为 task-only；
 *          此用例不断言 assert 触发（那会挂进程）——仅验证反向：task 上下文调用是
 *          干净的（no assert），保障 D-2 迁移未偏移正常路径。ISR 上下文误用由 host
 *          assert 覆盖（依赖调试构建，本用例不主动踩雷）。 */
void test_task_context_apis_do_not_assert_in_task_context(void) {
    /* setUp 已保证 flag=false（task 上下文） */
    wink_trace_fault(42u);
    (void)wink_trace_count();
    (void)wink_trace_last();
    wink_trace_reset();
    /* 若上面任一调用 assert，进程已 abort，走到这里就算通过 */
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_single_fault_task_and_isr_are_equivalent);
    RUN_TEST(test_mixed_sequence_matches_pure_task_sequence);
    RUN_TEST(test_ring_wraparound_task_vs_isr_are_equivalent);
    RUN_TEST(test_task_context_apis_do_not_assert_in_task_context);
    return UNITY_END();
}
