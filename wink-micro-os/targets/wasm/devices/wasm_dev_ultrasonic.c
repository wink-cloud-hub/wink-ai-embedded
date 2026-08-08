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

static void ultrasonic_do_reset(void) {
    for (int i = 0; i < WASM_SIM_MAX_PINS; i++) {
        s_virtual_ultrasonic_distance[i] = -1.0f;
    }
}

WINK_CONSTRUCTOR(ultrasonic_boot_init) {
    ultrasonic_do_reset();
}

void wasm_dev_ultrasonic_reset(void) {
    ultrasonic_do_reset();
}

EMSCRIPTEN_KEEPALIVE void pal_wasm_set_ultrasonic_distance(uint8_t pin, float distance_cm) {
    if (pin >= WASM_SIM_MAX_PINS) {
        return;
    }
    s_virtual_ultrasonic_distance[pin] = distance_cm;
}

EMSCRIPTEN_KEEPALIVE float pal_wasm_get_ultrasonic_distance(uint8_t pin) {
    if (pin >= WASM_SIM_MAX_PINS) {
        return -1.0f;
    }
    if (s_virtual_ultrasonic_distance[pin] >= 0.0f) {
        return s_virtual_ultrasonic_distance[pin];
    }
    if (pin > 0 && s_virtual_ultrasonic_distance[pin - 1] >= 0.0f) {
        return s_virtual_ultrasonic_distance[pin - 1];
    }
    if (pin + 1 < WASM_SIM_MAX_PINS && s_virtual_ultrasonic_distance[pin + 1] >= 0.0f) {
        return s_virtual_ultrasonic_distance[pin + 1];
    }
    return -1.0f;
}

