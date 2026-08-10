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
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

extern void pal_wasm_ch1_gpio_reset(void);
extern void pal_wasm_ch2_bus_reset(void);
extern void pal_wasm_ch2_uart_reset(void);
extern void pal_wasm_ch2b_pwm_reset(void);
extern void pal_wasm_ch3_adc_reset(void);
extern void pal_wasm_ch4_buffer_reset(void);
extern void pal_wasm_irq_reset(void);

static wink_sim_faults_t s_faults;
static wink_phys_debounce_ctx_t s_debounce_ctx[WASM_SIM_MAX_PINS];
static uint8_t s_fidelity_level = 0;
static uint32_t s_prng_state = 1u;

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_fidelity_level(uint8_t level) { WASM_FAULT_GUARD_VOID(); s_fidelity_level = level; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_bounce_us(uint32_t us) { WASM_FAULT_GUARD_VOID(); s_faults.bounce_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_warmup_us(uint32_t us) { WASM_FAULT_GUARD_VOID(); s_faults.warmup_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_sample_interval_us(uint32_t us) { WASM_FAULT_GUARD_VOID(); s_faults.sample_interval_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_adc_noise_v(float v) { WASM_FAULT_GUARD_VOID(); s_faults.adc_noise_v = v; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_rc_tau_s(float s) { WASM_FAULT_GUARD_VOID(); s_faults.rc_tau_s = s; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_i2c_drop_permil(uint16_t permil) { WASM_FAULT_GUARD_VOID(); s_faults.i2c_drop_permil = permil; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_prng_seed(uint32_t seed) { WASM_FAULT_GUARD_VOID(); s_prng_state = seed; }

uint32_t pal_wasm_get_bounce_us(void)        { return s_faults.bounce_us; }
uint16_t pal_wasm_get_i2c_drop_permil(void)  { return s_faults.i2c_drop_permil; }

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_prng_state(void)       { return s_prng_state; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_prng_state(uint32_t state)
{
    WASM_FAULT_GUARD_VOID();
    s_prng_state = state;
}

#define PAL_WASM_ABI_HASH 0x50333033u

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_abi_hash(void)
{
    return PAL_WASM_ABI_HASH;
}

void pal_wasm_advance_prng_state(uint32_t new_state) { s_prng_state = new_state; }

wink_sim_faults_t *pal_wasm_get_faults_ref(void) { return &s_faults; }

wink_phys_debounce_ctx_t *pal_wasm_get_debounce_ctx(uint16_t pin) {
    if (pin >= WASM_SIM_MAX_PINS) {
        return NULL;
    }
    return &s_debounce_ctx[pin];
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_reset_physical(void) {
    pal_wasm_clear_fault_latch();
    memset(&s_faults, 0, sizeof(s_faults));
    memset(s_debounce_ctx, 0, sizeof(s_debounce_ctx));
    s_prng_state = 1u;
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
    pal_wasm_ch2b_pwm_reset();
#ifndef WINK_STRICT_NONBLOCKING
    pal_wasm_ch3_adc_reset();
#endif
    pal_wasm_ch4_buffer_reset();
    pal_wasm_irq_reset();
}
