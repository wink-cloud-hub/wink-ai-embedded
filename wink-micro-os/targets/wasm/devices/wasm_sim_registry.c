// SPDX-License-Identifier: Apache-2.0
/**
 * @file wasm_sim_registry.c
 * @brief Wasm virtual peripheral dispatch registry implementation.
 */
#include "wasm_sim_registry.h"
#include "wasm_bridge.h"
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define WASM_SIM_MAX_PINS 40

void wasm_dev_servo_reset(void);
void wasm_dev_servo_set_duty(uint8_t channel, float duty_cycle_percent);

void     wasm_dev_ultrasonic_reset(void);
uint32_t wasm_dev_ultrasonic_get_pulse_us(uint8_t pin);
#ifdef __EMSCRIPTEN__
void     wasm_sim_pin_events_reset(void);
#endif

static bool s_gpio_inputs[WASM_SIM_MAX_PINS];
static bool s_gpio_input_set[WASM_SIM_MAX_PINS];
static bool s_gpio_outputs[WASM_SIM_MAX_PINS];

void wasm_sim_devices_reset(void) {
#if defined(WINK_USE_RC_SERVO) && (WINK_USE_RC_SERVO)
    wasm_dev_servo_reset();
#endif
#if defined(WINK_USE_ULTRASONIC) && (WINK_USE_ULTRASONIC)
    wasm_dev_ultrasonic_reset();
#endif
#ifdef __EMSCRIPTEN__
    wasm_sim_pin_events_reset();
#endif
    memset(s_gpio_inputs, 0, sizeof(s_gpio_inputs));
    memset(s_gpio_input_set, 0, sizeof(s_gpio_input_set));
    memset(s_gpio_outputs, 0, sizeof(s_gpio_outputs));
}

EMSCRIPTEN_KEEPALIVE void pal_wasm_sim_reset_all_devices(void) {
    wasm_sim_devices_reset();
}

bool wasm_sim_i2c_dev_exists(uint16_t dev_addr) {
    (void)dev_addr;
    return false;
}

wink_status_t wasm_sim_i2c_dev_transfer(uint8_t port, uint16_t dev_addr,
                                        const uint8_t *write_buf, uint32_t write_len,
                                        uint8_t *read_buf, uint32_t read_len) {
    (void)port;
    (void)dev_addr;
    (void)write_buf;
    (void)write_len;
    (void)read_buf;
    (void)read_len;
    return WINK_ERR_UNSUPPORTED;
}

bool wasm_sim_pwm_channel_exists(uint8_t channel) {
    return (channel < 16);
}

void wasm_sim_pwm_set_duty(uint8_t channel, float duty_cycle_percent) {
#if defined(WINK_USE_RC_SERVO) && (WINK_USE_RC_SERVO)
    wasm_dev_servo_set_duty(channel, duty_cycle_percent);
#else
    (void)channel;
    (void)duty_cycle_percent;
#endif
}

void wasm_sim_gpio_set_input(uint8_t pin, bool level) {
    if (pin < WASM_SIM_MAX_PINS) {
        s_gpio_inputs[pin] = level;
        s_gpio_input_set[pin] = true;
    }
}

bool wasm_sim_gpio_input_is_set(uint8_t pin, bool *out_level) {
    if (pin < WASM_SIM_MAX_PINS && s_gpio_input_set[pin]) {
        if (out_level != NULL) {
            *out_level = s_gpio_inputs[pin];
        }
        return true;
    }
    return false;
}

bool wasm_sim_gpio_get_input(uint8_t pin) {
    if (pin < WASM_SIM_MAX_PINS) {
        return s_gpio_inputs[pin];
    }
    return false;
}

bool wasm_sim_gpio_get_output(uint8_t pin) {
    if (pin < WASM_SIM_MAX_PINS) {
        return s_gpio_outputs[pin];
    }
    return false;
}

void wasm_sim_gpio_write(uint8_t pin, bool level) {
    if (pin < WASM_SIM_MAX_PINS) {
        s_gpio_outputs[pin] = level;
    }
}

EMSCRIPTEN_KEEPALIVE void pal_wasm_set_gpio_input(uint8_t pin, bool level) {
    js_pal_gpio_drive_ideal((uint16_t)pin, level);
}

#ifndef __EMSCRIPTEN__
void js_pal_gpio_drive_ideal(uint16_t pin, bool level) {
    (void)pin;
    (void)level;
}
#endif

EMSCRIPTEN_KEEPALIVE bool pal_wasm_get_gpio_output(uint8_t pin) {
    return wasm_sim_gpio_get_output(pin);
}
