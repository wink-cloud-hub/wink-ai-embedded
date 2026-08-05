// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_adc_wasm.c
 * @brief Wasm-side PAL ADC end-to-end tests (ADR-0057 / Plan 00.5).
 *
 * Covers what host tests structurally cannot:
 *   - js_pal_adc_read_norm bridge (EM_JS fixture → normalized [0,1])
 *   - C-side norm → raw/mv quantization
 *   - warmup BUSY / sample-interval TIMEOUT semantics
 *   - the has_sample cache contract (zero-value must not retrigger sampling)
 *   - NaN/Infinity clamp on the JS→C float boundary
 *   - per-channel independence (different pins, different RC state)
 *
 * Build wiring: test/wasm/pal_adc/add_wink_wasm_adc_test.cmake (registered
 * from test/CMakeLists.txt when emcc + node are on PATH). Runs under Node
 * via `ctest -L wasm` or `python wink-tools/wink.py test`.
 */
#include "unity.h"
#include "hal/pal_adc.h"
#include "pal_resource.h"
#include "wink_sim_physical.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* ADC read_* are WINK_BLOCKING (µs oneshot); these unit tests call them directly
 * from a task context. Suppress the cooperative-runtime deprecation warning. */
#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* ─────────────────────────────────────────────────────────────
 * JS-side fixture: a single float slot per pin emulating PinArbiter.
 * ───────────────────────────────────────────────────────────── */

EM_JS(void, adc_fixture_reset, (void), {
    globalThis.__wink_adc_norm = {};
});

EM_JS(void, adc_fixture_set, (uint16_t pin, float value), {
    if (!globalThis.__wink_adc_norm) globalThis.__wink_adc_norm = {};
    globalThis.__wink_adc_norm[String(pin)] = value;
});

/* The actual bridge import pal_wasm_adc.c calls. EM_JS supplies the
 * definition at link time, overriding the extern in wasm_bridge.h. */
EM_JS(float, js_pal_adc_read_norm, (uint16_t pin), {
    var tbl = globalThis.__wink_adc_norm;
    if (!tbl) return 0.0;
    var v = tbl[String(pin)];
    if (typeof v !== 'number') return 0.0;
    if (!Number.isFinite(v)) return 0.0;
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
});

/* ─────────────────────────────────────────────────────────────
 * Test-only virtual clock + fault config (implemented in link_stubs.c).
 * The real pal_osal_wasm.c is NOT linked — these tests don't exercise
 * Asyncify / the cooperative scheduler.
 * ───────────────────────────────────────────────────────────── */
extern void test_clock_reset(void);
extern void test_clock_advance_us(uint64_t us);
extern void test_faults_reset(void);
extern void test_faults_set_warmup_us(uint32_t us);
extern void test_faults_set_sample_interval_us(uint32_t us);
extern void test_faults_set_rc_tau_s(float tau_s);
extern void test_faults_set_adc_noise_v(float noise_v);

#define PIN_A 34
#define PIN_B 35
#define CH_A 0
#define CH_B 1

void setUp(void) {
    adc_fixture_reset();
    for (uint8_t i = 0; i < 16; i++) pal_adc_deinit(i);
    pal_resource_reset();
    test_clock_reset();
    test_faults_reset();
}

void tearDown(void) {}

/* 1) norm=1.0 → raw max, mv=full_scale; norm=0 → 0. */
void test_norm_to_raw_full_scale(void) {
    pal_adc_config_t cfg = { .pin = PIN_A, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(CH_A, &cfg));

    adc_fixture_set(PIN_A, 1.0f);
    uint16_t raw = 0, mv = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_mv(CH_A, &mv));
    TEST_ASSERT_EQUAL_UINT16(4095, raw);
    TEST_ASSERT_EQUAL_UINT16(3300, mv);

    adc_fixture_set(PIN_A, 0.0f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
    TEST_ASSERT_EQUAL_UINT16(0, raw);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_mv(CH_A, &mv));
    TEST_ASSERT_EQUAL_UINT16(0, mv);
}

/* 2) Half-scale quantization. */
void test_norm_half_scale_midpoint(void) {
    pal_adc_config_t cfg = { .pin = PIN_A, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(CH_A, &cfg));
    adc_fixture_set(PIN_A, 0.5f);

    uint16_t raw = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
    /* 0.5 * 4095 + 0.5 round-half-up = 2048 */
    TEST_ASSERT_EQUAL_UINT16(2048, raw);
}

/* 3) Regression: zero-valued cache must not retrigger warmup/interval check.
 *    Setup a sample_interval then read raw=0, then immediately read_mv —
 *    without has_sample this would return WINK_ERR_TIMEOUT on the second call. */
void test_zero_value_cache_does_not_retrigger_sampling(void) {
    pal_adc_config_t cfg = { .pin = PIN_A, .full_scale_mv = 3300, .resolution_bits = 12 };
    test_faults_set_sample_interval_us(10000);   /* require 10ms between samples */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(CH_A, &cfg));

    /* Advance past sample_interval before first sample (last_sample_us=0 is epoch
     * per wink_phys_warmup_check semantics). */
    test_clock_advance_us(10001);
    adc_fixture_set(PIN_A, 0.0f);                /* true value is zero */
    uint16_t raw = 0xFFFF, mv = 0xFFFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
    TEST_ASSERT_EQUAL_UINT16(0, raw);

    /* Same microsecond — must return cached 0, not TIMEOUT. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_mv(CH_A, &mv));
    TEST_ASSERT_EQUAL_UINT16(0, mv);
}

/* 4) First read_mv with no prior read_raw still returns a sample (triggers
 *    one conversion internally). This is the "moment consistency" contract. */
void test_first_read_mv_triggers_one_sample(void) {
    pal_adc_config_t cfg = { .pin = PIN_A, .full_scale_mv = 3100, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(CH_A, &cfg));
    adc_fixture_set(PIN_A, 0.25f);

    uint16_t mv = 0xFFFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_mv(CH_A, &mv));
    /* 0.25 * 3100 ≈ 775 (round-half-up) */
    TEST_ASSERT_EQUAL_UINT16(775, mv);
}

/* 5) Warmup: reads within the warmup window return BUSY. */
void test_warmup_returns_busy(void) {
    pal_adc_config_t cfg = { .pin = PIN_A, .full_scale_mv = 3300, .resolution_bits = 12 };
    test_faults_set_warmup_us(100000);  /* 100ms warmup */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(CH_A, &cfg));
    adc_fixture_set(PIN_A, 0.5f);

    uint16_t raw = 0;
    /* init latches power_on_us = 0; clock still 0 → in warmup */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_adc_read_raw(CH_A, &raw));

    test_clock_advance_us(50000);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_adc_read_raw(CH_A, &raw));

    test_clock_advance_us(60000);   /* past 100ms total */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
}

/* 6) Sample interval: reads too close together return TIMEOUT. */
void test_sample_interval_returns_timeout(void) {
    pal_adc_config_t cfg = { .pin = PIN_A, .full_scale_mv = 3300, .resolution_bits = 12 };
    test_faults_set_sample_interval_us(20000);  /* 20ms between samples */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(CH_A, &cfg));
    adc_fixture_set(PIN_A, 0.5f);

    /* Advance past interval from epoch (last_sample_us=0) for the first OK. */
    test_clock_advance_us(20001);
    uint16_t raw = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));

    /* Only 10ms elapsed → TIMEOUT, last_sample_us unchanged */
    test_clock_advance_us(10000);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, pal_adc_read_raw(CH_A, &raw));

    /* Wait the remainder → OK */
    test_clock_advance_us(15000);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
}

/* 7) Per-channel isolation: A and B at different pins have independent RC
 *    state and independent sample-interval bookkeeping. */
void test_channels_are_independent(void) {
    pal_adc_config_t ca = { .pin = PIN_A, .full_scale_mv = 3300, .resolution_bits = 12 };
    pal_adc_config_t cb = { .pin = PIN_B, .full_scale_mv = 3300, .resolution_bits = 12 };
    test_faults_set_sample_interval_us(50000);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(CH_A, &ca));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(CH_B, &cb));

    /* Advance past interval from epoch. */
    test_clock_advance_us(50001);
    adc_fixture_set(PIN_A, 0.1f);
    adc_fixture_set(PIN_B, 0.9f);

    uint16_t ra = 0, rb = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &ra));
    /* B has its own last_sample_us; not blocked by A's read. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_B, &rb));
    TEST_ASSERT_TRUE(rb > ra);

    /* A at 25us → TIMEOUT; B at same 25us, also <50us → TIMEOUT (separate clocks) */
    test_clock_advance_us(25000);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, pal_adc_read_raw(CH_A, &ra));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, pal_adc_read_raw(CH_B, &rb));
}

/* 8) NaN / Infinity / out-of-range on the JS import must not produce NaN raw
 *    or roll past max_raw. The bridge contract (Plan 00.5 §4.2, TS safeWrap
 *    `Number.isFinite(v) ? clamp(v,0,1) : 0.0`) maps non-finite to 0.0;
 *    out-of-range floats clamp to the nearest rail. */
void test_js_bridge_clamps_non_finite_and_oob(void) {
    pal_adc_config_t cfg = { .pin = PIN_A, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(CH_A, &cfg));

    uint16_t raw = 0xFFFF;
    /* NaN → safe fallback 0.0 (per ABI contract) */
    adc_fixture_set(PIN_A, NAN);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
    TEST_ASSERT_EQUAL_UINT16(0, raw);

    /* Infinity → safe fallback 0.0 (non-finite treated as missing/invalid) */
    adc_fixture_set(PIN_A, INFINITY);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
    TEST_ASSERT_EQUAL_UINT16(0, raw);

    /* Over-range finite float → clamp to 1.0 rail */
    adc_fixture_set(PIN_A, 5.0f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
    TEST_ASSERT_EQUAL_UINT16(4095, raw);

    /* Under-range finite float → clamp to 0.0 rail */
    adc_fixture_set(PIN_A, -1.0f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
    TEST_ASSERT_EQUAL_UINT16(0, raw);

    /* Normal mid-range still works after the clamp sequence. */
    adc_fixture_set(PIN_A, 0.75f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(CH_A, &raw));
    /* (uint16_t)(0.75f * 4095 + 0.5f) = (uint16_t)(3071.75) = 3071 */
    TEST_ASSERT_EQUAL_UINT16(3071, raw);
}

/* 9) Error path coverage. */
void test_init_and_read_arg_validation(void) {
    pal_adc_config_t good = { .pin = PIN_A, .full_scale_mv = 3300, .resolution_bits = 12 };

    /* Out-of-range channel */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        pal_adc_init(PAL_ADC_CHANNELS, &good));
    /* NULL cfg / negative pin */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_adc_init(0, NULL));
    pal_adc_config_t bad_pin = { .pin = -1, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_adc_init(0, &bad_pin));

    /* Uninitialized read */
    uint16_t v = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, pal_adc_read_raw(CH_B, &v));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, pal_adc_read_mv(CH_B, &v));

    /* Duplicate init */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(CH_A, &good));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_ALREADY_INITIALIZED, pal_adc_init(CH_A, &good));

    /* pin_channel lookup */
    pal_adc_channel_t ch;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(PIN_A, &ch));
    TEST_ASSERT_EQUAL_UINT8(CH_A, ch);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_FOUND, pal_adc_pin_channel(99, &ch));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_norm_to_raw_full_scale);
    RUN_TEST(test_norm_half_scale_midpoint);
    RUN_TEST(test_zero_value_cache_does_not_retrigger_sampling);
    RUN_TEST(test_first_read_mv_triggers_one_sample);
    RUN_TEST(test_warmup_returns_busy);
    RUN_TEST(test_sample_interval_returns_timeout);
    RUN_TEST(test_channels_are_independent);
    RUN_TEST(test_js_bridge_clamps_non_finite_and_oob);
    RUN_TEST(test_init_and_read_arg_validation);
    return UNITY_END();
}
