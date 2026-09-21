// SPDX-License-Identifier: Apache-2.0
/**
 * @file board_config.c
 * @brief DevKitC board hardware routing (overriding esp32 target weak default pal_pwm_pin_map).
 */
#include "pal_hal.h"

/* Strong definition overriding the weak default for ESP32 target. */
const wink_pin_t pal_pwm_pin_map[PAL_PWM_CHANNELS] = {2, 4, 5, 18, 19, 21, 22, 23};

/* DevKitC I2C routing: default hardware mapping (I2C0: SDA=21/SCL=22, I2C1: SDA=33/SCL=32). */
const wink_pin_t pal_i2c_pin_map[PAL_I2C_PORTS][2] = {
    {21, 22},
    {33, 32}
};
