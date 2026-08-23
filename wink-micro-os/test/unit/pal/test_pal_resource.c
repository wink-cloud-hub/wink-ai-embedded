// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_resource.c
 * @brief PAL resource allocation and conflict management unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "pal_resource.h"

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

void test_resource_claim_same_owner_idempotent(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 4, "devA"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 4, "devA"));
}

void test_resource_claim_conflict_returns_busy(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, 0, "servoA"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, 0, "servoB"));
}

void test_resource_claim_i2c_addr_shared_port_ok(void) {
    uint32_t id_a = pal_resource_i2c_id(0, 0x3C);
    uint32_t id_b = pal_resource_i2c_id(0, 0x3D);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id_a, "oled"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id_b, "sensor"));
}

void test_resource_claim_i2c_addr_conflict_returns_busy(void) {
    uint32_t id = pal_resource_i2c_id(0, 0x3C);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id, "oled0"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id, "oled1"));
}

void test_resource_claim_i2c_addr_table_full_returns_exhausted(void) {
    for (uint32_t i = 0; i < PAL_RESOURCE_MAX_CLAIMS; i++) {
        uint32_t id = pal_resource_i2c_id(0, (uint16_t)(0x10u + i));
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id, "filler"));
    }
    uint32_t overflow_id = pal_resource_i2c_id(0, 0xFFFF);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED,
                          pal_resource_claim(PAL_RESOURCE_I2C_ADDR, overflow_id, "overflow"));
}

void test_resource_release_then_reclaim_ok(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 7, "devA"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_release(PAL_RESOURCE_GPIO_PIN, 7, "devA"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 7, "devA"));
}

void test_resource_release_wrong_owner_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, 1, "servoA"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          pal_resource_release(PAL_RESOURCE_PWM_CHANNEL, 1, "servoB"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY,
                          pal_resource_claim(PAL_RESOURCE_PWM_CHANNEL, 1, "servoB"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          pal_resource_release(PAL_RESOURCE_GPIO_PIN, 99, "nobody"));
}

void test_resource_max_bounds(void) {
    TEST_ASSERT_EQUAL_UINT32(2, pal_resource_max(PAL_RESOURCE_SPI_BUS));
    TEST_ASSERT_EQUAL_UINT32(8, pal_resource_max(PAL_RESOURCE_PCNT_UNIT));
    TEST_ASSERT_EQUAL_UINT32(2, pal_resource_max(PAL_RESOURCE_MCPWM_UNIT));
    TEST_ASSERT_EQUAL_UINT32(8, pal_resource_max(PAL_RESOURCE_RMT_CHAN));
    TEST_ASSERT_EQUAL_UINT32(4, pal_resource_max(PAL_RESOURCE_HWTIMER));
    TEST_ASSERT_EQUAL_UINT32(3, pal_resource_max(PAL_RESOURCE_UART_PORT));
    TEST_ASSERT_EQUAL_UINT32(PAL_RESOURCE_UNLIMITED_MAX, pal_resource_max(PAL_RESOURCE_I2C_ADDR));
}

void test_resource_claim_out_of_bounds_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          pal_resource_claim(PAL_RESOURCE_SPI_BUS, 2, "spi_overflow"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
                          pal_resource_claim(PAL_RESOURCE_SPI_BUS, 1, "spi1"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          pal_resource_claim(PAL_RESOURCE_PCNT_UNIT, 8, "pcnt_overflow"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
                          pal_resource_claim(PAL_RESOURCE_PCNT_UNIT, 7, "pcnt7"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          pal_resource_claim(PAL_RESOURCE_MCPWM_UNIT, 2, "mcpwm_overflow"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          pal_resource_claim(PAL_RESOURCE_HWTIMER, 4, "hwtimer_overflow"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_resource_claim_same_owner_idempotent);
    RUN_TEST(test_resource_claim_conflict_returns_busy);
    RUN_TEST(test_resource_claim_i2c_addr_shared_port_ok);
    RUN_TEST(test_resource_claim_i2c_addr_conflict_returns_busy);
    RUN_TEST(test_resource_claim_i2c_addr_table_full_returns_exhausted);
    RUN_TEST(test_resource_release_then_reclaim_ok);
    RUN_TEST(test_resource_release_wrong_owner_returns_invalid_arg);
    RUN_TEST(test_resource_max_bounds);
    RUN_TEST(test_resource_claim_out_of_bounds_returns_invalid_arg);
    return UNITY_END();
}
