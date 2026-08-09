// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_resource_wire.c
 * @brief DAL-PAL resource allocation wiring end-to-end integration tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "pal_resource.h"

#include "dal_button.h"
#include "dal_led.h"
#include "dal_rc_servo.h"
#include "dal_ultrasonic.h"
#include "dal_gps.h"
#include "dal_eeprom.h"

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

void test_wire_dal_button_same_pin_conflict(void) {
    dal_button_t a = {0};
    dal_button_t b = {0};
    const dal_button_config_t cfg_a = { .owner = "button_a", .pin = 12, .active_low = true };
    const dal_button_config_t cfg_b = { .owner = "button_b", .pin = 12, .active_low = true };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&a, &cfg_a));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_button_init(&b, &cfg_b));
    TEST_ASSERT_FALSE(b.initialized);
}

void test_wire_dal_led_same_pin_conflict(void) {
    dal_led_t a = {0};
    dal_led_t b = {0};
    const dal_led_config_t cfg_a = { .owner = "led_a", .pin = 5, .active_high = true };
    const dal_led_config_t cfg_b = { .owner = "led_b", .pin = 5, .active_high = true };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&a, &cfg_a));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_led_init(&b, &cfg_b));
    TEST_ASSERT_FALSE(b.initialized);
}

void test_wire_cross_dal_button_vs_led_same_pin(void) {
    dal_button_t btn = {0};
    dal_led_t    led = {0};
    const dal_button_config_t cfg_btn = { .owner = "user_button",   .pin = 8, .active_low  = true };
    const dal_led_config_t    cfg_led = { .owner = "status_led",    .pin = 8, .active_high = true };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&btn, &cfg_btn));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_led_init(&led, &cfg_led));
    TEST_ASSERT_FALSE(led.initialized);
}

void test_wire_dal_rc_servo_same_channel_conflict(void) {
    dal_rc_servo_t a = {0};
    dal_rc_servo_t b = {0};
    const dal_rc_servo_config_t cfg_a = {
        .owner = "servo_a", .pwm_channel = 3,
        .min_pulse_us = 500, .max_pulse_us = 2500
    };
    const dal_rc_servo_config_t cfg_b = {
        .owner = "servo_b", .pwm_channel = 3,
        .min_pulse_us = 500, .max_pulse_us = 2500
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&a, &cfg_a));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_rc_servo_init(&b, &cfg_b));
    TEST_ASSERT_FALSE(b.initialized);
}

void test_wire_dal_ultrasonic_trig_pin_conflict(void) {
    dal_button_t btn = {0};
    const dal_button_config_t cfg_btn = { .owner = "btn_squatter", .pin = 20, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&btn, &cfg_btn));

    dal_ultrasonic_t us = {0};
    const dal_ultrasonic_config_t cfg_us = {
        .owner = "us_a", .trig_pin = 20, .echo_pin = 21, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_ultrasonic_init(&us, &cfg_us));
    TEST_ASSERT_FALSE(us.initialized);
}

void test_wire_dal_ultrasonic_echo_conflict_rolls_back_trig(void) {
    dal_button_t squatter = {0};
    const dal_button_config_t cfg_sq = { .owner = "echo_squatter", .pin = 31, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&squatter, &cfg_sq));

    dal_ultrasonic_t us = {0};
    const dal_ultrasonic_config_t cfg_us = {
        .owner = "us_rollback", .trig_pin = 30, .echo_pin = 31, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_ultrasonic_init(&us, &cfg_us));
    TEST_ASSERT_FALSE(us.initialized);

    dal_led_t probe_led = {0};
    const dal_led_config_t cfg_led = { .owner = "probe_led", .pin = 30, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&probe_led, &cfg_led));
    TEST_ASSERT_TRUE(probe_led.initialized);
}

void test_wire_uart_port_resource_conflict_via_pal(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "gps_a"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "gps_b"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "gps_a"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 2, "gps_b"));

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_UART_PORT, 1, "gps_a"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_UART_PORT, 2, "gps_b"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "gps_c"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_UART_PORT, 1, "gps_c"));
}

void test_wire_i2c_addr_resource_conflict_via_pal(void) {
    uint32_t id_0_50 = pal_resource_i2c_id(0, 0x50);
    uint32_t id_0_51 = pal_resource_i2c_id(0, 0x51);
    uint32_t id_1_50 = pal_resource_i2c_id(1, 0x50);

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id_0_50, "eeprom_a"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id_0_50, "eeprom_b"));

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id_0_51, "oled"));

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id_1_50, "eeprom_c"));

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_I2C_ADDR, id_0_50, "eeprom_a"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_I2C_ADDR, id_0_51, "oled"));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_release(PAL_RESOURCE_I2C_ADDR, id_1_50, "eeprom_c"));
}

void test_wire_gps_stub_returns_not_supported(void) {
    dal_gps_t dev = {0};
    const dal_gps_config_t cfg = {
        .owner = "test_gps", .uart_port = 1, .baudrate = 9600, .rx_buffer_size = 256
    };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);

    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_poll(&dev));
    dal_gps_position_t pos;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_gps_get_position(&dev, &pos));
}

void test_wire_eeprom_stub_returns_not_supported(void) {
    dal_eeprom_t dev = {0};
    const dal_eeprom_config_t cfg = {
        .owner = "test_eeprom", .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_eeprom_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);

    uint8_t buf[4] = {0x11, 0x22, 0x33, 0x44};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_eeprom_read_blocking(&dev, 0, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8(0x11, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x44, buf[3]);

    const uint8_t wbuf[4] = {1,2,3,4};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_eeprom_write_blocking(&dev, 0, wbuf, sizeof(wbuf)));
}

void test_wire_all_dal_reject_null_owner(void) {
    dal_button_t btn = {0};
    const dal_button_config_t bcfg = { .owner = NULL, .pin = 4, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_button_init(&btn, &bcfg));

    dal_led_t led = {0};
    const dal_led_config_t lcfg = { .owner = NULL, .pin = 4, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_led_init(&led, &lcfg));

    dal_rc_servo_t servo = {0};
    const dal_rc_servo_config_t scfg = {
        .owner = NULL, .pwm_channel = 0, .min_pulse_us = 500, .max_pulse_us = 2500
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_rc_servo_init(&servo, &scfg));

    dal_ultrasonic_t us = {0};
    const dal_ultrasonic_config_t ucfg = {
        .owner = NULL, .trig_pin = 4, .echo_pin = 5, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_init(&us, &ucfg));

    dal_gps_t gps = {0};
    const dal_gps_config_t gcfg = {
        .owner = NULL, .uart_port = 1, .baudrate = 9600, .rx_buffer_size = 256
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_gps_init(&gps, &gcfg));

    dal_eeprom_t ee = {0};
    const dal_eeprom_config_t ecfg = {
        .owner = NULL, .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_eeprom_init(&ee, &ecfg));
}

void test_wire_release_then_reclaim_across_dal(void) {
    dal_led_t led_a = {0};
    const dal_led_config_t cfg_a = { .owner = "led_a", .pin = 15, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&led_a, &cfg_a));

    TEST_ASSERT_EQUAL_INT(WINK_OK,
                          pal_resource_release(PAL_RESOURCE_GPIO_PIN, 15, "led_a"));

    dal_button_t btn = {0};
    const dal_button_config_t cfg_btn = { .owner = "btn_after_release", .pin = 15, .active_low = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&btn, &cfg_btn));
    TEST_ASSERT_TRUE(btn.initialized);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wire_dal_button_same_pin_conflict);
    RUN_TEST(test_wire_dal_led_same_pin_conflict);
    RUN_TEST(test_wire_cross_dal_button_vs_led_same_pin);
    RUN_TEST(test_wire_dal_rc_servo_same_channel_conflict);
    RUN_TEST(test_wire_dal_ultrasonic_trig_pin_conflict);
    RUN_TEST(test_wire_dal_ultrasonic_echo_conflict_rolls_back_trig);
    RUN_TEST(test_wire_uart_port_resource_conflict_via_pal);
    RUN_TEST(test_wire_i2c_addr_resource_conflict_via_pal);
    RUN_TEST(test_wire_gps_stub_returns_not_supported);
    RUN_TEST(test_wire_eeprom_stub_returns_not_supported);
    RUN_TEST(test_wire_all_dal_reject_null_owner);
    RUN_TEST(test_wire_release_then_reclaim_across_dal);
    return UNITY_END();
}
