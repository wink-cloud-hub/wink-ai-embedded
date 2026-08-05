// SPDX-License-Identifier: Apache-2.0
/**
 * @file wasm_dev_ultrasonic.c
 * @brief Wasm simulation HC-SR04 ultrasonic virtual peripheral model implementation.
 */
#include "wasm_sim_registry.h"
#include "wasm_bridge.h"
#include "wink_init_ctor.h"
#include <stdio.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#define WASM_SIM_MAX_PINS 40

static float s_virtual_ultrasonic_distance[WASM_SIM_MAX_PINS];

WINK_CONSTRUCTOR(ultrasonic_boot_init) {
    for (int i = 0; i < WASM_SIM_MAX_PINS; i++) {
        s_virtual_ultrasonic_distance[i] = -1.0f;
    }
}

void wasm_dev_ultrasonic_reset(void) {
    ultrasonic_boot_init();
}

EMSCRIPTEN_KEEPALIVE void pal_wasm_set_ultrasonic_distance(uint8_t pin, float distance_cm) {
    if (pin >= WASM_SIM_MAX_PINS) {
        return;
    }
    s_virtual_ultrasonic_distance[pin] = distance_cm;
}

uint32_t wasm_dev_ultrasonic_get_pulse_us(uint8_t pin) {
    if (pin >= WASM_SIM_MAX_PINS) {
        return 0;
    }

    float distance_cm = js_sim_get_plugin_channel("ultrasonic:0", "distanceCm");

    if (distance_cm < 0.0f && s_virtual_ultrasonic_distance[pin] >= 0.0f) {
        distance_cm = s_virtual_ultrasonic_distance[pin];
    }
    else if (distance_cm < 0.0f && pin > 0 && s_virtual_ultrasonic_distance[pin - 1] >= 0.0f) {
        distance_cm = s_virtual_ultrasonic_distance[pin - 1];
    }
    else if (distance_cm < 0.0f && pin < WASM_SIM_MAX_PINS - 1 && s_virtual_ultrasonic_distance[pin + 1] >= 0.0f) {
        distance_cm = s_virtual_ultrasonic_distance[pin + 1];
    }

    if (distance_cm < 0.0f) {
        return 0;
    }

    uint32_t pulse_us = (uint32_t)(distance_cm * 58.0f);
    if (distance_cm <= 2.0f) {
        pulse_us = 116;
    } else if (distance_cm >= 400.0f) {
        pulse_us = 0;
    }

    return pulse_us;
}
