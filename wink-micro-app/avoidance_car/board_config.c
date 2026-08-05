// SPDX-License-Identifier: Apache-2.0
/**
 * @file board_config.c
 * @brief Board-level hardware pin mapping configuration.
 */
#include "pal_hal.h"

/* Strong definition overriding the default weak map for ESP32 target. */
const wink_pin_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};

/* avoidance_car I2C routing: I2C0 for OLED (SDA=21, SCL=22), I2C1 reserved (SDA=33, SCL=32). */
const wink_pin_t pal_i2c_pin_map[PAL_I2C_PORTS][2] = {
    {21, 22},
    {33, 32}
};
