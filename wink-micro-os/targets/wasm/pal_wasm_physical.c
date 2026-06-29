/**
 * @file pal_wasm_physical.c
 * @brief ADR-0009 Wave 2 — WASM-side wrapper around the physical degradation
 *        algorithm library (`wink_sim_physical.c`).
 *
 * Responsibilities:
 *   1. Hold the per-WASM-instance degradation state (faults config, PRNG,
 *      per-pin debounce contexts) — all in BSS, zero dynamic memory.
 *   2. Expose setters/getters across the WASM↔JS bridge so a JS Worker can
 *      drive fault injection without touching C internals.
 *   3. Provide an in-bounds, per-pin debounce context accessor for
 *      pal_hal_wasm.c's GPIO middleware (Wave 2 Task 3).
 *
 * Architecture invariants:
 *   - Zero dynamic memory (§3.2 of the plan). Static arrays only.
 *   - BSS-only initialisation: C standard guarantees zero-init for
 *     `static` storage; we do not memset at startup.
 *   - WASM_SIM_MAX_PINS = 128. Any pin index ≥ 128 returns NULL from the ctx
 *     getter and the caller (HAL middleware) must treat it as "no degradation
 *     for this pin". This prevents JS-supplied out-of-range indices from
 *     causing an OOB write into BSS.
 *   - PRNG state is global by design (see comment block below) — this is the
 *     ADR-0009 §4.1 "single seed reproduces the whole system" contract.
 *
 * Symbol export style:
 *   - `EMSCRIPTEN_KEEPALIVE` (matches pal_osal_wasm.c). It keeps the symbol
 *     past `-Oz` stripping and surfaces it on the export table when the
 *     linker is given `EXPORTED_FUNCTIONS=['_pal_wasm_*']` patterns.
 *   - Internal helpers (used by pal_hal_wasm.c only) have no KEEPALIVE.
 */
#include "wink_sim_physical.h"
#include "pal_wasm_internal.h"
#include "wasm_bridge.h"

#include <emscripten.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

/* ─────────────────────────────────────────────────────────
 * Capacity
 * ─────────────────────────────────────────────────────────
 * 128 pins covers every embedded-class chip we ship to (ESP32-S3 has 49,
 * Cortex-M class boards <100). Each ctx is ~24B → ~3KB total: cheap.
 */
#define WASM_SIM_MAX_PINS  128

/* ─────────────────────────────────────────────────────────
 * Global state (BSS — zero-initialised by the C runtime)
 * ─────────────────────────────────────────────────────────
 * No constructor / startup memset needed: spec [C11 §6.7.9 p10] says objects
 * with static storage duration and no explicit initialiser are zero-init.
 * Emscripten honours this; the .bss segment is zeroed by the loader before
 * main() runs.
 */

/* Fault config — initial all-zero == WINK_SIM_FAULTS_IDEAL == degradation off */
static wink_sim_faults_t s_faults;

/* Per-pin debounce contexts. Indexed by pin number ∈ [0, WASM_SIM_MAX_PINS). */
static wink_phys_debounce_ctx_t s_debounce_ctx[WASM_SIM_MAX_PINS];

/* Deterministic PRNG state.
 *
 * Design note (architectural intent, NOT a bug):
 *   Every degradation primitive that consumes randomness (I2C drop, RC noise,
 *   …) shares this single PRNG. This is deliberate: it makes "one seed
 *   reproduces the entire simulation" possible. The trade-off is that
 *   changing the call frequency of one peripheral (say, polling ADC twice as
 *   often) shifts the random sequence consumed by all others. For typical
 *   bug-repro scenarios — same code path, same input sequence — this is
 *   exactly what we want.
 *
 *   If a future use case ("isolate I2C flakiness from ADC noise during a
 *   parameter sweep") needs per-peripheral PRNGs, evolve to a struct of
 *   sub-states; do NOT silently split: it would break golden vectors.
 *
 * Default seed is 1 so wasm starts in the same state as the host golden
 * vectors. JS sets a real seed via pal_wasm_set_prng_seed() at init.
 */
static uint32_t s_prng_state = 1u;

/* ─────────────────────────────────────────────────────────
 * Fault config setters — exported to JS Worker
 * ─────────────────────────────────────────────────────────
 * One setter per field. JSON deserialisation lives in WasmPhysicalBridge.ts;
 * see plan §3.2 (zero dynamic memory → no cJSON, no malloc).
 *
 * All scalar types ≤32 bits → JS `number` is lossless (per BigInt contract).
 */

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_bounce_us(uint32_t us) { s_faults.bounce_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_warmup_us(uint32_t us) { s_faults.warmup_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_sample_interval_us(uint32_t us) { s_faults.sample_interval_us = us; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_adc_noise_v(float v) { s_faults.adc_noise_v = v; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_rc_tau_s(float s) { s_faults.rc_tau_s = s; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_i2c_drop_permil(uint16_t permil) { s_faults.i2c_drop_permil = permil; }

EMSCRIPTEN_KEEPALIVE
void pal_wasm_set_prng_seed(uint32_t seed) { s_prng_state = seed; }

/* ─────────────────────────────────────────────────────────
 * Internal getters
 * ─────────────────────────────────────────────────────────
 * Consumed by pal_hal_wasm.c (Wave 2 Task 3) and by unit tests.
 *
 * `pal_wasm_get_prng_state` is also exported because tests / JS may want to
 * snapshot the PRNG for "scenario replay" workflows. JS MUST NOT write the
 * state directly other than via pal_wasm_set_prng_seed() — that would break
 * the single-seed-reproduces-all-degradation invariant.
 */
uint32_t pal_wasm_get_bounce_us(void)        { return s_faults.bounce_us; }
uint16_t pal_wasm_get_i2c_drop_permil(void)  { return s_faults.i2c_drop_permil; }

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_prng_state(void)       { return s_prng_state; }

/* HAL middleware writes back the PRNG state after consuming bytes. Internal —
 * not exported; JS goes through pal_wasm_set_prng_seed() if it needs to
 * reseed. */
void pal_wasm_advance_prng_state(uint32_t new_state) { s_prng_state = new_state; }

/* ─────────────────────────────────────────────────────────
 * Per-pin debounce context accessor
 * ─────────────────────────────────────────────────────────
 * Returns NULL for any pin ≥ WASM_SIM_MAX_PINS. The HAL middleware treats
 * NULL as "no debounce context for this pin → fall through to ideal level",
 * preserving the bounds-check-but-don't-crash contract from the plan §3.3.
 *
 * BSS guarantees s_debounce_ctx[pin] starts with all fields zero, which
 * matches the documented "fresh ctx" state (stable=false, in_bounce=false,
 * bounce_start_us=0, bounce_flip=false). No runtime memset needed.
 */
wink_phys_debounce_ctx_t *pal_wasm_get_debounce_ctx(uint16_t pin) {
    if (pin >= WASM_SIM_MAX_PINS) {
        return NULL;
    }
    return &s_debounce_ctx[pin];
}

/* ─────────────────────────────────────────────────────────
 * Reset (test-only utility)
 * ─────────────────────────────────────────────────────────
 * memset is acceptable here even though BSS init was free, because we're
 * resetting from an arbitrary mid-test state, not initialising. JS test
 * harnesses call this between scenarios.
 *
 * Resets:
 *   - faults to all-zero (== ideal == no degradation)
 *   - every per-pin debounce ctx to fresh state
 *   - PRNG seed to the default of 1 (matches BSS init)
 */
EMSCRIPTEN_KEEPALIVE
void pal_wasm_reset_physical(void) {
    memset(&s_faults, 0, sizeof(s_faults));
    memset(s_debounce_ctx, 0, sizeof(s_debounce_ctx));
    s_prng_state = 1u;
}
