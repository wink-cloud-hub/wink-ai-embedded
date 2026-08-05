// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_eeprom.c
 * @brief DAL EEPROM driver unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "dal_eeprom.h"
#include "pal_resource.h"

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED
#ifdef _MSC_VER
#  pragma warning(disable: 4996)
#endif

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

void test_eeprom_deinit_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_eeprom_deinit(NULL));
}

void test_eeprom_deinit_uninitialized_is_idempotent_noop(void) {
    dal_eeprom_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_eeprom_deinit(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_eeprom_deinit(&dev));
}

void test_eeprom_init_stub_reports_unsupported_and_does_not_claim(void) {
    dal_eeprom_t dev = {0};
    const dal_eeprom_config_t cfg = {
        .i2c_port = 0, .i2c_addr = 0x50, .capacity_bytes = 256,
        .page_size = 8, .write_time_ms = 5, .owner = "eeprom0",
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_eeprom_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);
    uint32_t res_id = pal_resource_i2c_id(0, 0x50);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_I2C_ADDR, res_id));
}

void test_eeprom_rw_stub_returns_unsupported_and_leaves_buf_untouched(void) {
    dal_eeprom_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          dal_eeprom_write_blocking(&dev, 0, NULL, 3));
    const uint8_t w[3] = {'a', 'b', 'c'};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED,
                          dal_eeprom_write_blocking(&dev, 0, w, sizeof(w)));
    uint8_t out[4] = {0x11, 0x22, 0x33, 0x44};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_eeprom_read_blocking(&dev, 0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8(0x11, out[0]);
    TEST_ASSERT_EQUAL_UINT8(0x44, out[3]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_eeprom_deinit_null_returns_invalid_arg);
    RUN_TEST(test_eeprom_deinit_uninitialized_is_idempotent_noop);
    RUN_TEST(test_eeprom_init_stub_reports_unsupported_and_does_not_claim);
    RUN_TEST(test_eeprom_rw_stub_returns_unsupported_and_leaves_buf_untouched);
    return UNITY_END();
}
