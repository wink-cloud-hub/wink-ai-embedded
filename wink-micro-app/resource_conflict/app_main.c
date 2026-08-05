// SPDX-License-Identifier: Apache-2.0
/**
 * @file app_main.c
 * @brief Resource conflict verification sample.
 */
#define LOG_TAG "resource_conflict"

#include "dal_led.h"
#include "dal_rc_servo.h"
#include "dal_gps.h"
#include "dal_eeprom.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "wink_status.h"
#include "wink_blocking_region.h"
#include <stdlib.h>

#define ASSERT_EQ(expected, actual, msg) do {                                                  \
    wink_status_t _e = (expected);                                                             \
    wink_status_t _a = (actual);                                                               \
    if (_e != _a) {                                                                            \
        LOG_E("SAMPLE FAIL: " msg " (expected=%d, got=%d)", (int)_e, (int)_a);                 \
        exit(1);                                                                               \
    }                                                                                          \
} while (0)

static void case_gpio_pin_conflict(void)
{
    dal_led_t led_a = {0};
    dal_led_t led_b = {0};
    const dal_led_config_t cfg_a = { .owner = "status_led",  .pin = 2, .active_high = true };
    const dal_led_config_t cfg_b = { .owner = "warning_led", .pin = 2, .active_high = true };

    ASSERT_EQ(WINK_OK,        dal_led_init(&led_a, &cfg_a), "GPIO: first LED init");
    ASSERT_EQ(WINK_ERR_BUSY,  dal_led_init(&led_b, &cfg_b), "GPIO: second LED same pin should BUSY");
    LOG_I("GPIO pin 2 conflict correctly rejected");
}

static void case_pwm_channel_conflict(void)
{
    dal_rc_servo_t servo_a = {0};
    dal_rc_servo_t servo_b = {0};
    const dal_rc_servo_config_t cfg_a = {
        .owner = "steering", .pwm_channel = 0,
        .min_pulse_us = 500, .max_pulse_us = 2500
    };
    const dal_rc_servo_config_t cfg_b = {
        .owner = "gripper",  .pwm_channel = 0,
        .min_pulse_us = 500, .max_pulse_us = 2500
    };

    ASSERT_EQ(WINK_OK,        dal_rc_servo_init(&servo_a, &cfg_a), "PWM: first servo init");
    ASSERT_EQ(WINK_ERR_BUSY,  dal_rc_servo_init(&servo_b, &cfg_b), "PWM: second servo same channel should BUSY");
    LOG_I("PWM channel 0 conflict correctly rejected");
}

static void case_uart_port_conflict(void)
{
    ASSERT_EQ(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "primary_gps"),
        "UART: first claim port 1");
    ASSERT_EQ(WINK_ERR_BUSY,
        pal_resource_claim(PAL_RESOURCE_UART_PORT, 1, "secondary_gps"),
        "UART: second claim same port should BUSY");
    ASSERT_EQ(WINK_OK,
        pal_resource_release(PAL_RESOURCE_UART_PORT, 1, "primary_gps"),
        "UART: release port 1");
    LOG_I("UART port 1 conflict correctly rejected via pal_resource");
}

static void case_i2c_addr_conflict(void)
{
    uint32_t id = pal_resource_i2c_id(0, 0x50);
    ASSERT_EQ(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id, "config_eeprom"),
        "I2C: first claim (port=0,addr=0x50)");
    ASSERT_EQ(WINK_ERR_BUSY,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id, "logging_eeprom"),
        "I2C: second claim same (port,addr) should BUSY");
    uint32_t id2 = pal_resource_i2c_id(0, 0x51);
    ASSERT_EQ(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_I2C_ADDR, id2, "oled_display"),
        "I2C: same port different addr should OK");
    ASSERT_EQ(WINK_OK,
        pal_resource_release(PAL_RESOURCE_I2C_ADDR, id, "config_eeprom"),
        "I2C: release eeprom");
    ASSERT_EQ(WINK_OK,
        pal_resource_release(PAL_RESOURCE_I2C_ADDR, id2, "oled_display"),
        "I2C: release oled");
    LOG_I("I2C (port=0, addr=0x50) conflict correctly rejected via pal_resource");
}

static void case_stub_honesty(void)
{
    WINK_INIT_BLOCKING_REGION_BEGIN
    dal_gps_t gps = {0};
    const dal_gps_config_t gps_cfg = {
        .owner = "test_gps", .uart_port = 1, .baudrate = 9600, .rx_buffer_size = 256
    };
    ASSERT_EQ(WINK_ERR_UNSUPPORTED, dal_gps_init(&gps, &gps_cfg),
              "Stub honesty: dal_gps_init must return NOT_SUPPORTED (no fake success)");

    dal_eeprom_t ee = {0};
    const dal_eeprom_config_t ee_cfg = {
        .owner = "test_eeprom", .i2c_port = 0, .i2c_addr = 0x50,
        .capacity_bytes = 32768, .page_size = 32, .write_time_ms = 5
    };
    ASSERT_EQ(WINK_ERR_UNSUPPORTED, dal_eeprom_init(&ee, &ee_cfg),
              "Stub honesty: dal_eeprom_init must return NOT_SUPPORTED (no fake success)");
    WINK_INIT_BLOCKING_REGION_END
    LOG_I("stub honesty (ADR-0012): unimplemented DALs return NOT_SUPPORTED, not fake WINK_OK");
}

int main(void)
{
    LOG_I("=== resource_conflict sample: verifying pal_resource conflict wiring + stub honesty ===");

    case_gpio_pin_conflict();
    case_pwm_channel_conflict();
    case_uart_port_conflict();
    case_i2c_addr_conflict();
    case_stub_honesty();

    LOG_I("SAMPLE PASS: all resource conflict checks and stub-honesty checks passed");
    return 0;
}
