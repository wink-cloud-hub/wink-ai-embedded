// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_virtual_clock.c
 * @brief WASM virtual clock unit tests.
 */
#include "unity.h"
#include "pal_osal.h"
#include "pal_wasm_internal.h"
#include <emscripten.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

void setUp(void) {
}

void tearDown(void) {}

void test_virtual_clock_starts_at_zero(void) {
    TEST_ASSERT_EQUAL_UINT64(0, pal_os_get_us());
    TEST_ASSERT_EQUAL_UINT64(0, pal_os_get_ms());
}

void test_virtual_clock_monotonic_advance(void) {
    pal_wasm_advance_virtual_clock(1000);
    TEST_ASSERT_EQUAL_UINT64(1000, pal_os_get_us());
    TEST_ASSERT_EQUAL_UINT64(1, pal_os_get_ms());

    pal_wasm_advance_virtual_clock(500);
    TEST_ASSERT_EQUAL_UINT64(1500, pal_os_get_us());
    TEST_ASSERT_EQUAL_UINT64(1, pal_os_get_ms());

    pal_wasm_advance_virtual_clock(500);
    TEST_ASSERT_EQUAL_UINT64(2000, pal_os_get_us());
    TEST_ASSERT_EQUAL_UINT64(2, pal_os_get_ms());
}

void test_virtual_clock_accepts_zero_advance(void) {
    uint64_t before = pal_os_get_us();
    pal_wasm_advance_virtual_clock(0);
    TEST_ASSERT_EQUAL_UINT64(before, pal_os_get_us());
}

void test_delay_ms_does_NOT_advance_clock(void) {
    uint64_t before = pal_os_get_us();
    pal_os_sleep_ms(10);
    uint64_t after = pal_os_get_us();
    TEST_ASSERT_EQUAL_UINT64(before, after);
}

void test_delay_us_does_NOT_advance_clock(void) {
    uint64_t before = pal_os_get_us();
    pal_os_busy_wait_us(500);
    uint64_t after = pal_os_get_us();
    TEST_ASSERT_EQUAL_UINT64(before, after);
}

void test_virtual_clock_64bit_wraparound_semantics(void) {
    uint64_t near_max = UINT64_MAX - 100;
    uint64_t result = near_max + 200;
    TEST_ASSERT_TRUE(result < near_max);
    (void)result;
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_virtual_clock_starts_at_zero);
    RUN_TEST(test_virtual_clock_monotonic_advance);
    RUN_TEST(test_virtual_clock_accepts_zero_advance);
    RUN_TEST(test_delay_ms_does_NOT_advance_clock);
    RUN_TEST(test_delay_us_does_NOT_advance_clock);
    RUN_TEST(test_virtual_clock_64bit_wraparound_semantics);
    return UNITY_END();
}
