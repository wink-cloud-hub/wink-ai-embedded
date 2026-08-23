// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_gps.c
 * @brief DAL GPS driver unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "dal_gps.h"
#include "pal_resource.h"
#include <string.h>

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED
#ifdef _MSC_VER
#  pragma warning(disable: 4996)
#endif

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

void test_gps_deinit_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_gps_deinit(NULL));
}

void test_gps_deinit_uninitialized_is_idempotent_noop(void) {
    dal_gps_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_gps_deinit(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_gps_deinit(&dev));
}

void test_gps_init_claims_resource_and_deinit_releases(void) {
    dal_gps_t dev = {0};
    const dal_gps_config_t cfg = {
        .uart_port = 1, .baudrate = 9600, .owner = "gps0",
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_gps_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_UART_PORT, 1));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_gps_poll(&dev));

    dal_gps_position_t pos;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_gps_get_position(&dev, &pos));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_gps_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_UART_PORT, 1));
}

void test_gps_get_position_null_returns_invalid_arg(void) {
    dal_gps_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_gps_get_position(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_gps_get_position(NULL, NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_gps_deinit_null_returns_invalid_arg);
    RUN_TEST(test_gps_deinit_uninitialized_is_idempotent_noop);
    RUN_TEST(test_gps_init_claims_resource_and_deinit_releases);
    RUN_TEST(test_gps_get_position_null_returns_invalid_arg);
    return UNITY_END();
}
