// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_degradation.c
 * @brief Wasm target physical degradation engine implementation.
 */
#include "wink_sim_physical.h"
#include "pal_wasm_degradation.h"
#include "wasm_bridge.h"
#include "pal_hal.h"
#include "wink_status.h"

#include <emscripten.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "pal_wasm_sim_state.h"

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif

extern void pal_wasm_ch1_gpio_reset(void);
extern void pal_wasm_ch2_bus_reset(void);
extern void pal_wasm_ch2_uart_reset(void);
extern void pal_wasm_ch1b_pwm_reset(void);
extern void pal_wasm_ch3_adc_reset(void);
extern void pal_wasm_ch4_buffer_reset(void);
extern void pal_wasm_irq_reset(void);
extern void pal_wasm_reset_scheduler_state(void);

sim_state_t g_sim = {
    .prng_state = 1u,
    .fidelity_level = 0,
};

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_fidelity_level(uint8_t level) { WASM_FAULT_GUARD_VOID(); g_sim.fidelity_level = level; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_bounce_us(uint32_t us) { WASM_FAULT_GUARD_VOID(); g_sim.faults.bounce_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_warmup_us(uint32_t us) { WASM_FAULT_GUARD_VOID(); g_sim.faults.warmup_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_sample_interval_us(uint32_t us) { WASM_FAULT_GUARD_VOID(); g_sim.faults.sample_interval_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_adc_noise_v(float v) { WASM_FAULT_GUARD_VOID(); g_sim.faults.adc_noise_v = v; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_rc_tau_s(float s) { WASM_FAULT_GUARD_VOID(); g_sim.faults.rc_tau_s = s; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_i2c_drop_permil(uint16_t permil) { WASM_FAULT_GUARD_VOID(); g_sim.faults.i2c_drop_permil = permil; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_prng_seed(uint32_t seed) { WASM_FAULT_GUARD_VOID(); g_sim.prng_state = seed; }

uint32_t pal_wasm_get_bounce_us(void)        { return g_sim.faults.bounce_us; }
uint16_t pal_wasm_get_i2c_drop_permil(void)  { return g_sim.faults.i2c_drop_permil; }

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_prng_state(void)       { return g_sim.prng_state; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_prng_state(uint32_t state)
{
    WASM_FAULT_GUARD_VOID();
    g_sim.prng_state = state;
}

#define PAL_WASM_ABI_HASH 0x50333036u

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_abi_hash(void)
{
    return PAL_WASM_ABI_HASH;
}

/**
 * Export firmware state buffer for TS ReplayHashCollector.
 * Two-stage query: if out_buf == NULL or max_len == 0, returns sizeof(sim_state_t).
 */
EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_export_state_hash_buffer(uint8_t* out_buf, uint32_t max_len)
{
    uint32_t req_len = (uint32_t)sizeof(sim_state_t);
    if (out_buf == NULL || max_len < req_len) {
        return req_len;
    }
    memcpy(out_buf, &g_sim, req_len);
    return req_len;
}

void pal_wasm_advance_prng_state(uint32_t new_state) { g_sim.prng_state = new_state; }

wink_sim_faults_t *pal_wasm_get_faults_ref(void) { return &g_sim.faults; }

wink_phys_debounce_ctx_t *pal_wasm_get_debounce_ctx(uint16_t pin) {
    if (pin >= WASM_SIM_MAX_PINS) {
        return NULL;
    }
    return &g_sim.debounce_ctx[pin];
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_reset_physical(void) {
    pal_wasm_clear_fault_latch();
    memset(&g_sim, 0, sizeof(g_sim));
    g_sim.prng_state = 1u;
    pal_wasm_reset_fault_log();
    pal_wasm_reset_fault_domains();
}

/** G3 requirement: preserve ABI export pal_wasm_sim_reset_all_devices */
EMSCRIPTEN_KEEPALIVE
void pal_wasm_sim_reset_all_devices(void) {
    pal_wasm_reset_physical();
    pal_wasm_ch1_gpio_reset();
    pal_wasm_ch2_bus_reset();
    pal_wasm_ch2_uart_reset();
    pal_wasm_ch1b_pwm_reset();
#ifndef WINK_STRICT_NONBLOCKING
    pal_wasm_ch3_adc_reset();
#endif
    pal_wasm_ch4_buffer_reset();
    pal_wasm_irq_reset();
    pal_wasm_reset_scheduler_state();
}
