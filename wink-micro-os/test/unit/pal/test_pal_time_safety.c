// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_time_safety.c
 * @brief PAL 64-bit timing safety and rollover boundary unit tests.
 */
#include "unity.h"
#include "pal_osal.h"
#include <stdint.h>
#include <stdbool.h>

void setUp(void) {}
void tearDown(void) {}

void test_uint64_subtraction_across_32bit_boundary(void) {
    /* Simulate starting just before 32-bit (4294.96s / 71.5 min) rollover */
    uint64_t start_us = 0xFFFFFFFFULL - 100ULL; /* 4294967195 */
    uint64_t now_us   = 0x100000000ULL + 200ULL; /* 4294967496 */

    uint64_t elapsed_us = now_us - start_us;
    TEST_ASSERT_EQUAL_UINT64(301ULL, elapsed_us);

    uint64_t timeout_us = 300ULL;
    TEST_ASSERT_TRUE(now_us - start_us > timeout_us);

    timeout_us = 500ULL;
    TEST_ASSERT_FALSE(now_us - start_us > timeout_us);
}

void test_pal_os_get_us_monotonicity(void) {
    uint64_t t0 = pal_os_get_us();
    pal_os_busy_wait_us(50);
    uint64_t t1 = pal_os_get_us();

    TEST_ASSERT_TRUE(t1 >= t0);
}

void test_pal_os_get_ms_monotonicity(void) {
    uint64_t m0 = pal_os_get_ms();
    pal_os_sleep_ms(5);
    uint64_t m1 = pal_os_get_ms();

    TEST_ASSERT_TRUE(m1 >= m0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_uint64_subtraction_across_32bit_boundary);
    RUN_TEST(test_pal_os_get_us_monotonicity);
    RUN_TEST(test_pal_os_get_ms_monotonicity);
    return UNITY_END();
}
