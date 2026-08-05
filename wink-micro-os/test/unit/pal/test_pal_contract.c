// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_contract.c
 * @brief PAL contract integrity compilation probe and unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_log.h"

#define STATIC_ASSERT_CONCAT(a, b) a##b
#define STATIC_ASSERT_CONCAT2(a, b) STATIC_ASSERT_CONCAT(a, b)
#define STATIC_ASSERT(cond) typedef char STATIC_ASSERT_CONCAT2(static_assertion_at_line_, __LINE__)[(cond) ? 1 : -1]

STATIC_ASSERT(WINK_OK == 0);
STATIC_ASSERT(WINK_ERR_HARDWARE == -12);
STATIC_ASSERT(WINK_MUTEX_WAIT_FOREVER == 0xFFFFFFFFu);
STATIC_ASSERT(PAL_OS_RESET_REASON_WATCHDOG == 2);
STATIC_ASSERT(PAL_OS_RESET_REASON_PANIC    == 3);
STATIC_ASSERT(PAL_OS_RESET_REASON_SOFTWARE == 4);
STATIC_ASSERT(PAL_OS_RESET_REASON_BROWNOUT == 5);

STATIC_ASSERT(PAL_LOG_LEVEL_NONE  == 0);
STATIC_ASSERT(PAL_LOG_LEVEL_ERROR == 1);
STATIC_ASSERT(PAL_LOG_LEVEL_WARN  == 2);
STATIC_ASSERT(PAL_LOG_LEVEL_INFO  == 3);
STATIC_ASSERT(PAL_LOG_LEVEL_DEBUG == 4);

#ifndef NDEBUG
STATIC_ASSERT(PAL_LOG_COMPILE_LEVEL >= PAL_LOG_LEVEL_DEBUG);
#endif

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

void test_contract_error_codes_and_macros_exist(void) {
    static volatile wink_status_t code = WINK_ERR_HARDWARE;
    volatile uint32_t forever = WINK_MUTEX_WAIT_FOREVER;
    TEST_ASSERT_EQUAL_INT(-12, (int)code);
    TEST_ASSERT_EQUAL_INT(0xFFFFFFFFu, forever);
}

void test_contract_release_roundtrip(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 13, "probe"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 13, "other"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          pal_resource_release(PAL_RESOURCE_GPIO_PIN, 13, "other"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_release(PAL_RESOURCE_GPIO_PIN, 13, "probe"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 13, "other"));
}

void test_contract_pal_log_macros_link_and_run(void) {
    static const char *TAG = "contract";
    pal_log_e(TAG, "contract err test: %d", -1);
    pal_log_w(TAG, "contract warn test");
    pal_log_i(TAG, "contract info test: %s", "ok");
    pal_log_d(TAG, "contract debug test");
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_contract_error_codes_and_macros_exist);
    RUN_TEST(test_contract_release_roundtrip);
    RUN_TEST(test_contract_pal_log_macros_link_and_run);
    return UNITY_END();
}
