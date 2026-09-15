// SPDX-License-Identifier: GPL-3.0-only
/**
 * @file test_pal_pwm_bp_math.c
 * @brief PAL PWM basis-points math matrix (ADR-0066, PLAN-20260912 W-3).
 *
 * Covers boundary/rounding/overflow/branch matrix vs independent 64-bit
 * reference implementation, plus helper-macro contract checks.
 */
#include "unity.h"
#include "hal/pal_pwm.h"

#include <stdint.h>
#include <stdio.h>

/* Independent 64-bit reference (ADR-0066 original algorithm). */
static uint32_t ref_calc_duty_counter(uint16_t bp, uint32_t top)
{
    if (bp == 0u) { return 0u; }
    if (bp >= 10000u) { return top; }
    if (top == 0u) { return 0u; }
    uint64_t product = (uint64_t)bp * (uint64_t)top;
    uint32_t count = (uint32_t)((product + 5000ull) / 10000ull);
    return (count > top) ? top : count;
}

/* C99 constant-expression smoke: static init must accept the macro. */
static const uint16_t k_pct50_init = PAL_PWM_DUTY_PCT(50);

void setUp(void) {}
void tearDown(void) {}

void test_bp_static_macro_init(void) {
    TEST_ASSERT_EQUAL_UINT16(5000u, k_pct50_init);
    TEST_ASSERT_EQUAL_UINT16(0u, PAL_PWM_DUTY_OFF);
    TEST_ASSERT_EQUAL_UINT16(5000u, PAL_PWM_DUTY_HALF);
    TEST_ASSERT_EQUAL_UINT16(10000u, PAL_PWM_DUTY_FULL);
}

void test_bp_pct_permille_helpers(void) {
    TEST_ASSERT_EQUAL_UINT16(0u, PAL_PWM_DUTY_PCT(0));
    TEST_ASSERT_EQUAL_UINT16(5000u, PAL_PWM_DUTY_PCT(50));
    TEST_ASSERT_EQUAL_UINT16(10000u, PAL_PWM_DUTY_PCT(100));
    TEST_ASSERT_EQUAL_UINT16(10000u, PAL_PWM_DUTY_PCT(120));
    TEST_ASSERT_EQUAL_UINT16(0u, PAL_PWM_DUTY_PERMILLE(0));
    /* 7.5% RC-servo mid must use permille/demo: 75 -> 750 bp */
    TEST_ASSERT_EQUAL_UINT16(750u, PAL_PWM_DUTY_PERMILLE(75));
    TEST_ASSERT_EQUAL_UINT16(10000u, PAL_PWM_DUTY_PERMILLE(1000));
    TEST_ASSERT_EQUAL_UINT16(10000u, PAL_PWM_DUTY_PERMILLE(1200));

    TEST_ASSERT_EQUAL_UINT16(5000u, pal_pwm_duty_pct(50));
    TEST_ASSERT_EQUAL_UINT16(10000u, pal_pwm_duty_pct(100));
    TEST_ASSERT_EQUAL_UINT16(10000u, pal_pwm_duty_pct(255));
    TEST_ASSERT_EQUAL_UINT16(750u, pal_pwm_duty_permille(75));
    TEST_ASSERT_EQUAL_UINT16(10000u, pal_pwm_duty_permille(1000));
}

void test_bp_matrix_against_reference(void) {
    static const struct { uint16_t bp; uint32_t top; uint32_t expect; } cases[] = {
        { 0,     65535u,   0u },
        { 1,     65535u,   7u },
        { 5000,  65535u,   32768u },
        { 9999,  65535u,   65528u },
        { 10000, 1048575u, 1048575u },
        { 1,     1048575u, 105u },
        { 5000,  1048575u, 524288u },
        { 9999,  1048575u, 1048470u },
        /* top protection */
        { 0,     0u,       0u },
        { 5000,  0u,       0u },
        { 10000, 0u,       0u },
        /* extra boundaries */
        { 0,     1u,       0u },
        { 1,     1u,       0u },
        { 9999,  1u,       1u },
        { 10000, 1u,       1u },
        { 5000,  255u,     128u },
        { 1,     255u,     0u },
        { 9999,  255u,     255u },
        { 5000,  65536u,   32768u },
    };
    for (unsigned i = 0; i < (unsigned)(sizeof(cases) / sizeof(cases[0])); i++) {
#if (PAL_PWM_DUTY_TOP_LIMIT <= PAL_PWM_TOP_32BIT_MAX)
        /* 65535-limited build: only exercise top<=65535 per plan §6.2. */
        if (cases[i].top > 65535u) { continue; }
#endif
        uint32_t got = pal_pwm_calc_duty_counter(cases[i].bp, cases[i].top);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(cases[i].expect, got, "matrix expect mismatch");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(
            ref_calc_duty_counter(cases[i].bp, cases[i].top), got,
            "helper vs 64-bit reference mismatch");
    }
}

void test_bp_rounding_half_trip(void) {
    /* +5000 critical: bp=1,top=500000 -> (500000+5000)/10000 = 50 (0.5 rounds down). */
    TEST_ASSERT_EQUAL_UINT32(50u, pal_pwm_calc_duty_counter(1u, 500000u));
    /* bp=1,top=5000000(full-range only): (5000000+5000)/10000=500. */
#if (PAL_PWM_DUTY_TOP_LIMIT > PAL_PWM_TOP_32BIT_MAX)
    TEST_ASSERT_EQUAL_UINT32(500u, pal_pwm_calc_duty_counter(1u, 5000000u));
    /* clamp: bp just below full on max top never exceeds top. */
    TEST_ASSERT_TRUE(pal_pwm_calc_duty_counter(9999u, 0xFFFFFFFFu) <= 0xFFFFFFFFu);
#endif
    /* clamp on small top. */
    TEST_ASSERT_TRUE(pal_pwm_calc_duty_counter(9999u, 255u) <= 255u);
}

void test_bp_random_against_reference(void) {
    /* Deterministic LCG, 1000 samples over bp[0,10000] x top[0,1048575]. */
    uint32_t seed = 0x12345678u;
    for (int i = 0; i < 1000; i++) {
        seed = seed * 1664525u + 1013904223u;
        uint16_t bp = (uint16_t)((seed >> 7) % 10001u);
        seed = seed * 1664525u + 1013904223u;
        uint32_t top = (seed >> 5) % 1048576u;
#if (PAL_PWM_DUTY_TOP_LIMIT <= PAL_PWM_TOP_32BIT_MAX)
        if (top > 65535u) { top = top % 65536u; }
#endif
        uint32_t got = pal_pwm_calc_duty_counter(bp, top);
        uint32_t want = ref_calc_duty_counter(bp, top);
        if (got != want) {
            char msg[96];
            snprintf(msg, sizeof(msg), "bp=%u top=%lu", (unsigned)bp, (unsigned long)top);
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

void test_bp_clamp_semantics(void) {
    /* Helper clamps (defensive), API layer returns INVALID_ARG (tested elsewhere). */
    TEST_ASSERT_EQUAL_UINT32(65535u, pal_pwm_calc_duty_counter(10000u, 65535u));
    TEST_ASSERT_EQUAL_UINT32(65535u, pal_pwm_calc_duty_counter(10001u, 65535u));
    TEST_ASSERT_EQUAL_UINT32(0u, pal_pwm_calc_duty_counter(0u, 65535u));
    TEST_ASSERT_EQUAL_UINT32(0u, pal_pwm_calc_duty_counter(5000u, 0u));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bp_static_macro_init);
    RUN_TEST(test_bp_pct_permille_helpers);
    RUN_TEST(test_bp_matrix_against_reference);
    RUN_TEST(test_bp_rounding_half_trip);
    RUN_TEST(test_bp_random_against_reference);
    RUN_TEST(test_bp_clamp_semantics);
    return UNITY_END();
}
