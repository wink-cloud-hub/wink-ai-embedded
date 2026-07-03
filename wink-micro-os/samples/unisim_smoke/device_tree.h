/**
 * @file device_tree.h
 * @brief Static device tree for the unisim_smoke wasm fixture.
 *
 * Minimum surface to exercise all 13 js_* imports: LED (GPIO write/read),
 * ultrasonic (js_sim_*), plus explicit pal_* calls for PWM/I2C/interrupts.
 */
#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H

#include "dal_led.h"
#include "dal_ultrasonic.h"

#define SMOKE_LED_PIN            2u
#define SMOKE_ULTRASONIC_TRIG    12u
#define SMOKE_ULTRASONIC_ECHO    13u
#define SMOKE_PWM_CHANNEL        1u
#define SMOKE_PWM_FREQ_HZ        1000u
#define SMOKE_I2C_PORT           0u
#define SMOKE_I2C_ADDR           0x3Cu
#define SMOKE_ISR_PIN            4u

extern dal_led_t         board_led;
extern dal_ultrasonic_t  us_sensor;

#endif /* DEVICE_TREE_H */
