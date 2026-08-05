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

void test_gps_init_stub_reports_unsupported_and_does_not_claim(void) {
    dal_gps_t dev = {0};
    const dal_gps_config_t cfg = {
        .uart_port = 1, .baudrate = 9600, .owner = "gps0",
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_UART_PORT, 1));
}

void test_gps_poll_stub_returns_unsupported(void) {
    dal_gps_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_poll(&dev));
}

void test_gps_get_position_leaves_output_untouched_on_error(void) {
    dal_gps_t dev = {0};
    dal_gps_position_t pos;
    memset(&pos, 0x55, sizeof(pos));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_get_position(&dev, &pos));
    uint8_t *raw = (uint8_t *)&pos;
    for (size_t i = 0; i < sizeof(pos); i++) {
        TEST_ASSERT_EQUAL_UINT8(0x55, raw[i]);
    }
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
    RUN_TEST(test_gps_init_stub_reports_unsupported_and_does_not_claim);
    RUN_TEST(test_gps_poll_stub_returns_unsupported);
    RUN_TEST(test_gps_get_position_leaves_output_untouched_on_error);
    RUN_TEST(test_gps_get_position_null_returns_invalid_arg);
    return UNITY_END();
}
