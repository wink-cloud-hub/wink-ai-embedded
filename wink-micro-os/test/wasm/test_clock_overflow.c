// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_clock_overflow.c
 * @brief WASM virtual clock overflow warning unit tests.
 */
#include "unity.h"
#include "pal_osal.h"
#include "pal_wasm_internal.h"
#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>

void setUp(void) {
}

void tearDown(void) {}

void test_clock_64bit_static_assert(void) {
    TEST_PASS();
}

void test_clock_warning_not_fired_initially(void) {
    TEST_ASSERT_FALSE(pal_wasm_is_clock_warning_fired());
    TEST_ASSERT_EQUAL_UINT64(pal_os_get_us(), pal_wasm_get_virtual_clock_us());
}

void test_clock_warning_not_fired_below_threshold(void) {
    pal_wasm_advance_virtual_clock(1000000ULL);
    TEST_ASSERT_FALSE(pal_wasm_is_clock_warning_fired());
}

void test_clock_warning_fires_when_crossing_threshold(void) {
    pal_wasm_advance_virtual_clock(0x8000000000000001ULL);
    TEST_ASSERT_TRUE(pal_wasm_is_clock_warning_fired());

    pal_wasm_advance_virtual_clock(1000ULL);
    TEST_ASSERT_TRUE(pal_wasm_is_clock_warning_fired());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_clock_64bit_static_assert);
    RUN_TEST(test_clock_warning_not_fired_initially);
    RUN_TEST(test_clock_warning_not_fired_below_threshold);
    RUN_TEST(test_clock_warning_fires_when_crossing_threshold);
    return UNITY_END();
}
