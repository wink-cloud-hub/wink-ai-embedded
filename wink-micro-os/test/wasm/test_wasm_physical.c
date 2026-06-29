/**
 * @file test_wasm_physical.c
 * @brief ADR-0009 Wave 2 — WASM-side degradation engine unit tests.
 *
 * Coverage:
 *   1. PRNG golden parity with host (validates algorithm library is truly
 *      target-agnostic).
 *   2. Fault config setter/getter loopback (validates the JS↔C bridge plumbing
 *      compiles, links, and round-trips values bit-exact).
 *   3. Debounce forced-alternation golden (mirrors test_sim_physical.c).
 *   4. Per-pin debounce ctx accessor bounds checking — the OOB protection
 *      added in this task for memory-safety against JS-supplied pin indices.
 *
 * Build wiring (deferred to a follow-up plan task):
 *   This file is delivered as source-only, matching the pattern set by
 *   test_virtual_clock.c (Task 1). When a `add_wink_wasm_test` CMake helper
 *   lands, both files will be registered with it. Running these against the
 *   golden vectors then proves host/wasm bit-identical behaviour.
 *
 * Golden vectors come from test/common/test_physical_golden.h — DO NOT
 * hard-code values here. That header is the single source of truth.
 */
#include "unity.h"
#include "wink_sim_physical.h"
#include "pal_wasm_internal.h"
#include "test_physical_golden.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>

/* Exported-to-JS symbols we exercise from C-side tests. Declared here as
 * plain externs (KEEPALIVE on the definitions is what creates the export). */
extern void     pal_wasm_set_bounce_us(uint32_t us);
extern void     pal_wasm_set_prng_seed(uint32_t seed);
extern uint32_t pal_wasm_get_prng_state(void);
extern void     pal_wasm_reset_physical(void);

void setUp(void) {
    /* Fresh state per test. memset is acceptable in test harness — see
     * pal_wasm_reset_physical() docstring. */
    pal_wasm_reset_physical();
}

void tearDown(void) {}

/* ─────────────────────────────────────────────────────────
 * (1) PRNG golden parity with host
 * ─────────────────────────────────────────────────────────
 * Same algorithm library compiled for two targets; outputs must match
 * bit-for-bit with the host test_sim_physical.c value.
 */
void test_prng_golden_matches_host(void) {
    uint32_t seed = GOLDEN_PRNG_SEED1;
    float val = wink_phys_prng_next(&seed);
    TEST_ASSERT_EQUAL_UINT32(GOLDEN_PRNG_AFTER_CALL1, seed);
    TEST_ASSERT_FLOAT_WITHIN(GOLDEN_PRNG_TOLERANCE, GOLDEN_PRNG_VALUE1, val);
}

/* ─────────────────────────────────────────────────────────
 * (2) Fault config setter/getter loopback
 * ─────────────────────────────────────────────────────────
 * Validates: bridge symbols are linked, static state is reachable, the
 * setters actually write the struct (not just no-ops). pal_wasm_get_bounce_us
 * is an internal getter; including pal_wasm_internal.h surfaces it.
 */
void test_fault_config_setget_loopback(void) {
    pal_wasm_set_bounce_us(30000u);
    pal_wasm_set_prng_seed(42u);
    TEST_ASSERT_EQUAL_UINT32(30000u, pal_wasm_get_bounce_us());
    TEST_ASSERT_EQUAL_UINT32(42u, pal_wasm_get_prng_state());
}

/* Reset wipes both faults and PRNG back to defaults. */
void test_reset_clears_state(void) {
    pal_wasm_set_bounce_us(30000u);
    pal_wasm_set_prng_seed(42u);
    pal_wasm_reset_physical();
    TEST_ASSERT_EQUAL_UINT32(0u, pal_wasm_get_bounce_us());
    TEST_ASSERT_EQUAL_UINT32(1u, pal_wasm_get_prng_state());  /* default seed */
}

/* ─────────────────────────────────────────────────────────
 * (3) Debounce forced-alternation golden — mirrors host
 * ─────────────────────────────────────────────────────────
 * Initial ctx = all zero; target rises false→true; we expect the algorithm
 * to alternate true/false/true inside the bounce window.
 */
void test_debounce_forced_alternation_golden(void) {
    wink_phys_debounce_ctx_t ctx = {false, false, 0u, false};
    TEST_ASSERT_EQUAL(GOLDEN_BOUNCE_STEP1,
        wink_phys_debounce_step(&ctx, GOLDEN_BOUNCE_TARGET,
                                GOLDEN_BOUNCE_NOW1_US, GOLDEN_BOUNCE_US));
    TEST_ASSERT_EQUAL(GOLDEN_BOUNCE_STEP2,
        wink_phys_debounce_step(&ctx, GOLDEN_BOUNCE_TARGET,
                                GOLDEN_BOUNCE_NOW2_US, GOLDEN_BOUNCE_US));
    TEST_ASSERT_EQUAL(GOLDEN_BOUNCE_STEP3,
        wink_phys_debounce_step(&ctx, GOLDEN_BOUNCE_TARGET,
                                GOLDEN_BOUNCE_NOW3_US, GOLDEN_BOUNCE_US));
    TEST_ASSERT_TRUE(ctx.in_bounce);
}

/* ─────────────────────────────────────────────────────────
 * (4) Bounds check — JS-supplied pin must not OOB the BSS array
 * ─────────────────────────────────────────────────────────
 * Returns:
 *   pin <  WASM_SIM_MAX_PINS → non-NULL pointer into static array.
 *   pin >= WASM_SIM_MAX_PINS → NULL (HAL middleware treats as "no degradation").
 *
 * Verifying the implementation defends against the failure mode "JS passes
 * pin=0xFFFF and crashes WASM with heap corruption".
 */
void test_debounce_ctx_in_bounds(void) {
    TEST_ASSERT_NOT_NULL(pal_wasm_get_debounce_ctx(0));
    TEST_ASSERT_NOT_NULL(pal_wasm_get_debounce_ctx(127));  /* WASM_SIM_MAX_PINS - 1 */
}

void test_debounce_ctx_out_of_bounds_returns_null(void) {
    TEST_ASSERT_NULL(pal_wasm_get_debounce_ctx(128));      /* WASM_SIM_MAX_PINS */
    TEST_ASSERT_NULL(pal_wasm_get_debounce_ctx(65535));    /* UINT16_MAX */
}

/* Distinct pins must yield distinct ctx pointers — proves the array is
 * actually used as an array, not a single shared scalar. */
void test_debounce_ctx_distinct_pins_distinct_pointers(void) {
    wink_phys_debounce_ctx_t *a = pal_wasm_get_debounce_ctx(0);
    wink_phys_debounce_ctx_t *b = pal_wasm_get_debounce_ctx(1);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_EQUAL(a, b);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_prng_golden_matches_host);
    RUN_TEST(test_fault_config_setget_loopback);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_debounce_forced_alternation_golden);
    RUN_TEST(test_debounce_ctx_in_bounds);
    RUN_TEST(test_debounce_ctx_out_of_bounds_returns_null);
    RUN_TEST(test_debounce_ctx_distinct_pins_distinct_pointers);
    return UNITY_END();
}
