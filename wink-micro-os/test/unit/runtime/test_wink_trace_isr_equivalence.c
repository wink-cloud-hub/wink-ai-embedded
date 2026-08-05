// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_wink_trace_isr_equivalence.c
 * @brief Unit tests verifying equivalence between task and ISR trace recording functions.
 */
#include "unity.h"
#include "wink_trace.h"
#include "pal_osal.h"

void setUp(void) {
    pal_os_set_sim_isr_context(false);
    wink_trace_reset();
}
void tearDown(void) {
    pal_os_set_sim_isr_context(false);
}

static void isr_record(uint32_t code) {
    pal_os_set_sim_isr_context(true);
    wink_trace_fault_from_isr(code);
    pal_os_set_sim_isr_context(false);
}

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

void test_mixed_sequence_matches_pure_task_sequence(void) {
    const uint32_t seq[] = { 1u, 2u, 3u, 4u, 5u, 6u };
    const size_t   n     = sizeof(seq) / sizeof(seq[0]);

    for (size_t i = 0; i < n; i++) { wink_trace_fault(seq[i]); }
    uint32_t count_task = wink_trace_count();
    uint32_t last_task  = wink_trace_last();

    wink_trace_reset();

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

void test_ring_wraparound_task_vs_isr_are_equivalent(void) {
    for (uint32_t i = 0; i < WINK_TRACE_CAPACITY + 5u; i++) {
        wink_trace_fault(i);
    }
    uint32_t count_task = wink_trace_count();
    uint32_t last_task  = wink_trace_last();

    wink_trace_reset();

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

void test_task_context_apis_do_not_assert_in_task_context(void) {
    wink_trace_fault(42u);
    (void)wink_trace_count();
    (void)wink_trace_last();
    wink_trace_reset();
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
