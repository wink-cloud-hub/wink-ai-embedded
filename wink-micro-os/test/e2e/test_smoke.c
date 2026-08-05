// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_smoke.c
 * @brief Smoke tests for wink_status helpers.
 */
#include "unity.h"
#include "wink_status.h"

void setUp(void) {}
void tearDown(void) {}

void test_wink_status_is_error_negative(void) {
    TEST_ASSERT_TRUE(wink_status_is_error(WINK_ERR_INVALID_ARG));
    TEST_ASSERT_TRUE(wink_status_is_error(WINK_ERR_TIMEOUT));
    TEST_ASSERT_TRUE(wink_status_is_error(WINK_ERR_CONFIG_CORRUPT_DEGRADED));
}

void test_wink_status_ok_is_not_error(void) {
    TEST_ASSERT_FALSE(wink_status_is_error(WINK_OK));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wink_status_is_error_negative);
    RUN_TEST(test_wink_status_ok_is_not_error);
    return UNITY_END();
}
