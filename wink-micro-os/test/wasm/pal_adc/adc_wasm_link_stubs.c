// SPDX-License-Identifier: Apache-2.0
/**
 * @file adc_wasm_link_stubs.c
 * @brief Minimal stubs for test_pal_adc_wasm: virtual clock, fault config,
 *        and any external symbols the focused build doesn't pull in.
 *
 * We deliberately do NOT link pal_osal_wasm.c (drags in scheduler + Asyncify +
 * fault logging) or pal_wasm_physical.c (depends on the full fault domain
 * stack). The physics library wink_sim_physical.c is linked for real.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sim_physical.h"
#include "wink_status.h"

/* ── Virtual clock (overrides the symbol pal_wasm_adc.c calls) ─────────── */
static uint64_t s_clock_us = 0;
uint64_t pal_os_get_us(void) { return s_clock_us; }
void test_clock_reset(void) { s_clock_us = 0; }
void test_clock_advance_us(uint64_t us) { s_clock_us += us; }

/* ── Faults config — pal_wasm_adc.c calls pal_wasm_get_faults_ref() ────── */
static wink_sim_faults_t s_faults;
wink_sim_faults_t *pal_wasm_get_faults_ref(void) { return &s_faults; }
void test_faults_reset(void) {
    s_faults = (wink_sim_faults_t){0};
}
void test_faults_set_warmup_us(uint32_t us)             { s_faults.warmup_us = us; }
void test_faults_set_sample_interval_us(uint32_t us)    { s_faults.sample_interval_us = us; }
void test_faults_set_rc_tau_s(float tau_s)              { s_faults.rc_tau_s = tau_s; }
void test_faults_set_adc_noise_v(float noise_v)         { s_faults.adc_noise_v = noise_v; }

/* ── pal_resource.c references (if any extra symbols needed) ───────────── */
/* pal_resource.c is self-contained; no extra stubs. */
