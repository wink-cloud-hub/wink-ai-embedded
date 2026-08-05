// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_i2c_bus.c
 * @brief Host target PAL I2C bus lifecycle unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "pal_resource.h"
#include "hal/pal_i2c.h"
#include "host_test_ctrl.h"

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED
#ifdef _MSC_VER
#  pragma warning(disable: 4996)
#endif

void setUp(void) {
    pal_resource_reset();
    sim_reset_time();
}
void tearDown(void) {}

void test_bus_init_rejects_invalid_port(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          pal_i2c_bus_init(PAL_I2C_PORTS, 21, 22, 100000));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(0, 21, 22, 100000));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(0, 21, 22, 100000));
    pal_i2c_bus_deinit(0);
}

void test_bus_deinit_idempotent_on_uninited_port(void) {
    pal_i2c_bus_deinit(1);
    pal_i2c_bus_deinit(1);
    pal_i2c_bus_deinit(PAL_I2C_PORTS);
    TEST_PASS();
}

void test_bus_init_enables_transfer(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(0, 21, 22, 100000));
    uint8_t w = 0x00;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_transfer(0, 0x3C, &w, 1, NULL, 0));
    TEST_ASSERT_EQUAL_INT(0x3C, sim_last_i2c_addr());
    pal_i2c_bus_deinit(0);
}

void test_transfer_after_deinit_lazy_reinits_as_transitional_behavior(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(0, 21, 22, 100000));
    pal_i2c_bus_deinit(0);
    uint8_t w = 0xAE;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_transfer(0, 0x3C, &w, 1, NULL, 0));
    pal_i2c_bus_deinit(0);
}

void test_bus_ports_are_independent(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(0, 21, 22, 100000));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_bus_init(1, 33, 34, 400000));
    uint8_t w = 0x00;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_transfer(0, 0x3C, &w, 1, NULL, 0));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_transfer(1, 0x50, &w, 1, NULL, 0));
    pal_i2c_bus_deinit(0);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_i2c_transfer(1, 0x50, &w, 1, NULL, 0));
    pal_i2c_bus_deinit(1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bus_init_rejects_invalid_port);
    RUN_TEST(test_bus_deinit_idempotent_on_uninited_port);
    RUN_TEST(test_bus_init_enables_transfer);
    RUN_TEST(test_transfer_after_deinit_lazy_reinits_as_transitional_behavior);
    RUN_TEST(test_bus_ports_are_independent);
    return UNITY_END();
}
