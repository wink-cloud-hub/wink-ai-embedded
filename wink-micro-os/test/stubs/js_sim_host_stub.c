// SPDX-License-Identifier: Apache-2.0
/**
 * @file js_sim_host_stub.c
 * @brief Host simulation js_sim_* stubs.
 */
#include "js_sim_host_stub.h"
#include "wink_init_ctor.h"

static uint32_t s_injected_pulse_us = 0;

void sim_set_echo_pulse_us(uint32_t pulse_us) { s_injected_pulse_us = pulse_us; }

void js_sim_trigger_ultrasonic(uint16_t trig_pin) { (void)trig_pin; }

uint32_t js_sim_measure_echo_pulse_us(uint16_t trig_pin) {
    (void)trig_pin;
    return s_injected_pulse_us;
}

WINK_CONSTRUCTOR(register_sim_ultrasonic_callbacks) {
    extern void host_register_sim_ultrasonic(void (*trigger_fn)(uint16_t), uint32_t (*measure_fn)(uint16_t));
    host_register_sim_ultrasonic(js_sim_trigger_ultrasonic, js_sim_measure_echo_pulse_us);
}
