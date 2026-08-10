// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_fault_domain.c
 * @brief Wasm simulation fault domain isolation framework and power model stubs.
 */
#include "pal_wasm_common.h"
#include "wink_status.h"

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

static wasm_fault_domain_t s_fault_domains[WASM_FAULT_DOMAIN_COUNT];

void pal_wasm_reset_fault_domains(void) {
    for (uint32_t i = 0; i < WASM_FAULT_DOMAIN_COUNT; i++) {
        s_fault_domains[i].domain_id     = i;
        s_fault_domains[i].armed         = true;
        s_fault_domains[i].trigger_count = 0u;
    }
}

EMSCRIPTEN_KEEPALIVE
wink_status_t pal_wasm_set_pin_power_model(uint8_t pin,
                                           const wasm_pin_power_model_t *model) {
    WASM_FAULT_GUARD_WINKERR();
    if (pin >= WASM_SIM_MAX_PINS) {
        return WINK_ERR_INVALID_ARG;
    }
    if (model == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    (void)pin;
    (void)model;
    return WINK_OK;
}

EMSCRIPTEN_KEEPALIVE
uint64_t pal_wasm_get_total_energy_mj(void) {
    return 0;
}

wink_sim_faults_t *pal_wasm_get_domain_config(uint32_t domain_id) {
    if (domain_id >= WASM_FAULT_DOMAIN_COUNT) {
        return NULL;
    }
    return pal_wasm_get_faults_ref();
}

wink_status_t pal_wasm_arm_fault_domain(uint32_t domain_id, bool armed) {
    if (domain_id >= WASM_FAULT_DOMAIN_COUNT) {
        return WINK_ERR_INVALID_ARG;
    }
    s_fault_domains[domain_id].armed = armed;
    return WINK_OK;
}

uint32_t pal_wasm_get_domain_trigger_count(uint32_t domain_id) {
    if (domain_id >= WASM_FAULT_DOMAIN_COUNT) {
        return 0u;
    }
    return s_fault_domains[domain_id].trigger_count;
}

#endif
