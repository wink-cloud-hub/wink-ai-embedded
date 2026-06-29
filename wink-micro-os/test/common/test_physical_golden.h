/**
 * @file test_physical_golden.h
 * @brief ADR-0009 Wave 2 SSOT golden vectors — host + wasm shared.
 *
 * Single source of truth for the physical degradation algorithm reference outputs.
 * Both host tests (`test/test_sim_physical.c`) and wasm tests
 * (`test/wasm/test_wasm_physical.c`) include this header to assert bit-level
 * identical behaviour across targets — that's the "algorithm library is
 * target-agnostic" guarantee in action.
 *
 * Editing rules:
 *   - Anyone tightening these values MUST run BOTH host and wasm test suites,
 *     and verify both pass with the new values.
 *   - Existing host expectations in `test_sim_physical.c` were the original
 *     golden source; the values here are extracted verbatim from there so
 *     host tests remain green by construction.
 *   - Float tolerance is 1e-4 (matches host test_sim_physical.c).
 *
 * Determinism contract (ADR-0009 §4.1):
 *   PRNG is the deterministic uniform generator `wink_phys_prng_next`. Its
 *   internal LCG transition is `s = (s*1103515245 + 12345) & 0x7fffffff` and
 *   the return value is `(s >> 8) / 8388608.0f`. Any change to that formula
 *   is a determinism breaking change — bump the golden values AND the schema.
 */
#ifndef WINK_TEST_PHYSICAL_GOLDEN_H
#define WINK_TEST_PHYSICAL_GOLDEN_H

#include <stdint.h>
#include <stdbool.h>

/* ─────────────────────────────────────────────────────────
 * PRNG golden (seed = 1)
 * ─────────────────────────────────────────────────────────
 * After one call to wink_phys_prng_next(&seed) with seed=1:
 *   new seed = (1 * 1103515245 + 12345) & 0x7fffffff = 1103527590
 *   value    = (1103527590 >> 8) / 8388608.0f ≈ 0.51387
 */
#define GOLDEN_PRNG_SEED1         1u
#define GOLDEN_PRNG_AFTER_CALL1   1103527590u
#define GOLDEN_PRNG_VALUE1        0.51387f
#define GOLDEN_PRNG_TOLERANCE     0.0001f

/* ─────────────────────────────────────────────────────────
 * Debounce forced-alternation golden
 * ─────────────────────────────────────────────────────────
 * Initial ctx: stable_level=false, in_bounce=false, bounce_start_us=0,
 *              bounce_flip=false.
 * Each step: target_level=true, bounce_us=30000us; now_us monotonically rises.
 * Algorithm flips bounce_flip every sample inside the bounce window and
 * returns `flip ? target : !target`, i.e. true/false/true/false ...
 */
#define GOLDEN_BOUNCE_US          30000u
#define GOLDEN_BOUNCE_TARGET      true
#define GOLDEN_BOUNCE_STEP1       true   /* flip false->true  -> returns target=true  */
#define GOLDEN_BOUNCE_STEP2       false  /* flip true->false  -> returns !target=false */
#define GOLDEN_BOUNCE_STEP3       true   /* flip false->true  -> returns target=true  */

/* Sample times for debounce steps (us). Spacing irrelevant to behaviour as
 * long as all are within the bounce window [start, start+bounce_us). */
#define GOLDEN_BOUNCE_NOW1_US     1000u
#define GOLDEN_BOUNCE_NOW2_US     2000u
#define GOLDEN_BOUNCE_NOW3_US     3000u

#endif /* WINK_TEST_PHYSICAL_GOLDEN_H */
