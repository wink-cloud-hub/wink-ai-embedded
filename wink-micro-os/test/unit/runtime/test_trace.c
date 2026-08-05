// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_trace.c
 * @brief Wink trace and fault logging unit tests.
 */
#include "unity.h"
#include "wink_trace.h"

void setUp(void) { wink_trace_reset(); }
void tearDown(void) {}

void test_reset_clears_buffer(void) {
    wink_trace_fault(1001);
    wink_trace_reset();
    TEST_ASSERT_EQUAL_UINT32(0, wink_trace_count());
}

void test_fault_recorded_in_order(void) {
    wink_trace_fault(1001);
    wink_trace_fault(1002);
    TEST_ASSERT_EQUAL_UINT32(2, wink_trace_count());
    TEST_ASSERT_EQUAL_UINT32(1002, wink_trace_last());
}

void test_ring_buffer_overwrites_oldest(void) {
    for (uint32_t i = 0; i < WINK_TRACE_CAPACITY + 5; i++) {
        wink_trace_fault(i);
    }
    TEST_ASSERT_EQUAL_UINT32(WINK_TRACE_CAPACITY, wink_trace_count());
    TEST_ASSERT_EQUAL_UINT32(WINK_TRACE_CAPACITY + 4, wink_trace_last());
}

void test_last_when_empty_is_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0, wink_trace_last());
}

void test_warn_counter_independent_of_fault(void) {
    wink_trace_fault(1001);
    wink_trace_fault(1002);
    wink_trace_warn(8002);
    wink_trace_warn(8003);
    wink_trace_warn(8002);
    TEST_ASSERT_EQUAL_UINT32(2, wink_trace_count());
    TEST_ASSERT_EQUAL_UINT32(3, wink_warn_count());
    TEST_ASSERT_EQUAL_UINT32(1002, wink_trace_last());
}

void test_reset_clears_warns_too(void) {
    wink_trace_warn(8002);
    wink_trace_warn(8003);
    TEST_ASSERT_EQUAL_UINT32(2, wink_warn_count());
    wink_trace_reset();
    TEST_ASSERT_EQUAL_UINT32(0, wink_warn_count());
    TEST_ASSERT_EQUAL_UINT32(0, wink_trace_count());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reset_clears_buffer);
    RUN_TEST(test_fault_recorded_in_order);
    RUN_TEST(test_ring_buffer_overwrites_oldest);
    RUN_TEST(test_last_when_empty_is_zero);
    RUN_TEST(test_warn_counter_independent_of_fault);
    RUN_TEST(test_reset_clears_warns_too);
    return UNITY_END();
}
