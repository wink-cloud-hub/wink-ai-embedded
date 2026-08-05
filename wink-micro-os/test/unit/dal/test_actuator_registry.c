// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_actuator_registry.c
 * @brief Actuator safe-off registry unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "wink_actuator_registry.h"
#include "wink_trace.h"
#include <stdint.h>

void setUp(void) { wink_actuator_registry_reset(); wink_trace_reset(); }
void tearDown(void) {}

static int s_off_calls = 0;
static wink_status_t mock_safe_off_ok(void *ctx) { (void)ctx; s_off_calls++; return WINK_OK; }

void test_register_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_actuator_register(NULL, NULL));
}

void test_register_duplicate_is_idempotent(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_safe_off_ok, (void *)1));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_safe_off_ok, (void *)1));
}

void test_safe_off_all_calls_all_registered_actuators(void) {
    s_off_calls = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_safe_off_ok, (void *)1));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_safe_off_ok, (void *)2));
    wink_actuator_safe_off_all();
    TEST_ASSERT_EQUAL_INT(2, s_off_calls);
}

void test_registry_full_returns_resource_exhausted(void) {
    for (uint32_t i = 0; i < WINK_ACTUATOR_REGISTRY_CAPACITY; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK,
            wink_actuator_register(mock_safe_off_ok, (void *)(uintptr_t)(i + 1)));
    }
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED,
        wink_actuator_register(mock_safe_off_ok, (void *)(uintptr_t)(WINK_ACTUATOR_REGISTRY_CAPACITY + 1)));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_register_null_returns_invalid_arg);
    RUN_TEST(test_register_duplicate_is_idempotent);
    RUN_TEST(test_safe_off_all_calls_all_registered_actuators);
    RUN_TEST(test_registry_full_returns_resource_exhausted);
    return UNITY_END();
}
