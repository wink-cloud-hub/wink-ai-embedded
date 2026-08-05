// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_sim_physical.c
 * @brief Physical simulation engine unit tests.
 */
#include "unity.h"
#include "wink_sim_physical.h"

void setUp(void) {}
void tearDown(void) {}

void test_prng_is_deterministic_and_matches_golden(void) {
    uint32_t s1 = 1, s2 = 1;
    float r1 = wink_phys_prng_next(&s1);
    TEST_ASSERT_EQUAL_UINT32(1103527590u, s1);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5138f, r1);
    float r2 = wink_phys_prng_next(&s2);
    TEST_ASSERT_EQUAL_UINT32(s1, s2);
    TEST_ASSERT_EQUAL_FLOAT(r1, r2);
}

void test_prng_in_unit_range(void) {
    uint32_t s = 42;
    for (int i = 0; i < 1000; i++) {
        float r = wink_phys_prng_next(&s);
        TEST_ASSERT_TRUE(r >= 0.0f && r < 1.0f);
    }
}

void test_debounce_forced_alternation_within_window(void) {
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    uint32_t bounce = 30000;
    TEST_ASSERT_TRUE (wink_phys_debounce_step(&ctx, true, 1000, bounce));
    TEST_ASSERT_FALSE(wink_phys_debounce_step(&ctx, true, 2000, bounce));
    TEST_ASSERT_TRUE (wink_phys_debounce_step(&ctx, true, 3000, bounce));
    TEST_ASSERT_FALSE(wink_phys_debounce_step(&ctx, true, 4000, bounce));
    TEST_ASSERT_TRUE(ctx.in_bounce);
}

void test_debounce_settles_after_window(void) {
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    uint32_t bounce = 30000;
    wink_phys_debounce_step(&ctx, true, 1000, bounce);
    wink_phys_debounce_step(&ctx, true, 5000, bounce);
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 31000, bounce));
    TEST_ASSERT_TRUE(ctx.stable_level);
    TEST_ASSERT_FALSE(ctx.in_bounce);
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 50000, bounce));
}

void test_debounce_disabled_when_bounce_zero(void) {
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 0, 0));
    TEST_ASSERT_TRUE(ctx.stable_level);
}

void test_debounce_null_ctx_returns_target(void) {
    TEST_ASSERT_TRUE(wink_phys_debounce_step(NULL, true, 0, 30000));
}

void test_debounce_time_regression_resets_gracefully(void) {
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    uint32_t bounce = 30000;
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 5000, bounce));
    TEST_ASSERT_TRUE(ctx.in_bounce);
    TEST_ASSERT_EQUAL_UINT64(5000, ctx.bounce_start_us);
    TEST_ASSERT_FALSE(wink_phys_debounce_step(&ctx, true, 0, bounce));
    TEST_ASSERT_EQUAL_UINT64(0, ctx.bounce_start_us);
    TEST_ASSERT_TRUE(ctx.in_bounce);
}

void test_rc_lowpass_first_step_golden(void) {
    wink_phys_rc_ctx_t rc = { 0.0f, 0, true };
    float v = wink_phys_rc_lowpass(&rc, 1.0f, 1000, 0.05f, 0.0f, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.02f, v);
}

void test_rc_lowpass_converges_to_target(void) {
    wink_phys_rc_ctx_t rc = { 0.0f, 0, true };
    uint64_t now = 0;
    for (int i = 0; i < 500; i++) { now += 10000; wink_phys_rc_lowpass(&rc, 1.0f, now, 0.05f, 0.0f, NULL); }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, rc.current);
}

void test_rc_noise_bounded(void) {
    wink_phys_rc_ctx_t rc = { 0.5f, 0, true };
    uint32_t seed = 7;
    for (int i = 0; i < 100; i++) {
        float v = wink_phys_rc_lowpass(&rc, 0.5f, (uint64_t)i * 1000, 0.05f, 0.02f, &seed);
        TEST_ASSERT_TRUE(v >= 0.5f - 0.05f && v <= 0.5f + 0.05f);
    }
}

void test_rc_null_ctx_returns_target(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.7f, wink_phys_rc_lowpass(NULL, 0.7f, 0, 0.05f, 0.0f, NULL));
}

void test_rc_lowpass_uninitialized_auto_sets_target(void) {
    wink_phys_rc_ctx_t rc = { 0 };
    float v = wink_phys_rc_lowpass(&rc, 1.5f, 1000, 0.05f, 0.0f, NULL);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, v);
    TEST_ASSERT_TRUE(rc.is_initialized);
    TEST_ASSERT_EQUAL_UINT64(1000, rc.last_us);
}

void test_rc_lowpass_tau_zero_passthrough_each_step(void) {
    wink_phys_rc_ctx_t rc = { 0 };
    TEST_ASSERT_EQUAL_FLOAT(1.0f, wink_phys_rc_lowpass(&rc, 1.0f, 1000, 0.0f, 0.0f, NULL));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, wink_phys_rc_lowpass(&rc, 0.0f, 2000, 0.0f, 0.0f, NULL));
    TEST_ASSERT_EQUAL_FLOAT(0.75f, wink_phys_rc_lowpass(&rc, 0.75f, 3000, -1.0f, 0.0f, NULL));
}

void test_rc_lowpass_time_regression_resets_gracefully(void) {
    wink_phys_rc_ctx_t rc = { 0.5f, 2000, true };
    float v = wink_phys_rc_lowpass(&rc, 1.0f, 0, 0.05f, 0.0f, NULL);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, v);
    TEST_ASSERT_EQUAL_UINT64(0, rc.last_us);
}

void test_warmup_busy_then_timeout_then_ok(void) {
    uint64_t last = 0;
    TEST_ASSERT_EQUAL(WINK_ERR_BUSY, wink_phys_warmup_check(500000, 0, 1000000, 2000000, &last));
    TEST_ASSERT_EQUAL(WINK_ERR_TIMEOUT, wink_phys_warmup_check(1500000, 0, 1000000, 2000000, &last));
    TEST_ASSERT_EQUAL_UINT64(0, last);
    TEST_ASSERT_EQUAL(WINK_OK, wink_phys_warmup_check(2500000, 0, 1000000, 2000000, &last));
    TEST_ASSERT_EQUAL_UINT64(2500000, last);
}

void test_warmup_time_regression_resets_gracefully(void) {
    uint64_t last = 50000;
    TEST_ASSERT_EQUAL(WINK_OK, wink_phys_warmup_check(1000, 0, 0, 2000, &last));
    TEST_ASSERT_EQUAL_UINT64(1000, last);
}

void test_bus_drop_boundary_and_deterministic(void) {
    uint32_t s0 = 0, s1000 = 0, s500a = 1, s500b = 1;
    TEST_ASSERT_FALSE(wink_phys_bus_drop(0, &s0));
    TEST_ASSERT_TRUE(wink_phys_bus_drop(1000, &s1000));
    bool a = wink_phys_bus_drop(500, &s500a);
    bool b = wink_phys_bus_drop(500, &s500b);
    TEST_ASSERT_EQUAL(a, b);
}

void test_bus_drop_null_seed_never_drops(void) {
    TEST_ASSERT_FALSE(wink_phys_bus_drop(500, NULL));
}

#include "host_test_ctrl.h"
#include "pal_hal.h"
#include "pal_resource.h"

extern void host_sim_advance_to(uint64_t us);

void test_host_gpio_ideal_transition_triggers_bounce(void) {
    sim_reset_time();
    pal_resource_reset();
    wink_sim_faults_t f = WINK_SIM_FAULTS_IDEAL; f.bounce_us = 30000; f.prng_seed = 1;
    sim_set_faults(&f);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 7, "test"));

    bool lvl = false;
    sim_set_gpio_ideal(7, true);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(7, &lvl));
    TEST_ASSERT_TRUE(lvl);

    host_sim_advance_to(1000);
    sim_set_gpio_ideal(7, false);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(7, &lvl));
    TEST_ASSERT_FALSE(lvl);
    host_sim_advance_to(2000);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(7, &lvl));
    TEST_ASSERT_TRUE(lvl);
    host_sim_advance_to(3000);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(7, &lvl));
    TEST_ASSERT_FALSE(lvl);

    host_sim_advance_to(31000);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(7, &lvl));
    TEST_ASSERT_FALSE(lvl);

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, pal_gpio_read(10, &lvl));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_gpio_read(99, &lvl));
    sim_clear_gpio_ideal();
}

void test_debounce_early_release_resets_in_bounce(void) {
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    uint32_t bounce = 30000;
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 1000, bounce));
    TEST_ASSERT_TRUE(ctx.in_bounce);

    TEST_ASSERT_FALSE(wink_phys_debounce_step(&ctx, false, 5000, bounce));
    TEST_ASSERT_FALSE(ctx.in_bounce);
    TEST_ASSERT_FALSE(ctx.stable_level);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_prng_is_deterministic_and_matches_golden);
    RUN_TEST(test_prng_in_unit_range);
    RUN_TEST(test_debounce_forced_alternation_within_window);
    RUN_TEST(test_debounce_settles_after_window);
    RUN_TEST(test_debounce_disabled_when_bounce_zero);
    RUN_TEST(test_debounce_null_ctx_returns_target);
    RUN_TEST(test_debounce_time_regression_resets_gracefully);
    RUN_TEST(test_rc_lowpass_first_step_golden);
    RUN_TEST(test_rc_lowpass_converges_to_target);
    RUN_TEST(test_rc_noise_bounded);
    RUN_TEST(test_rc_null_ctx_returns_target);
    RUN_TEST(test_rc_lowpass_uninitialized_auto_sets_target);
    RUN_TEST(test_rc_lowpass_tau_zero_passthrough_each_step);
    RUN_TEST(test_rc_lowpass_time_regression_resets_gracefully);
    RUN_TEST(test_warmup_busy_then_timeout_then_ok);
    RUN_TEST(test_warmup_time_regression_resets_gracefully);
    RUN_TEST(test_bus_drop_boundary_and_deterministic);
    RUN_TEST(test_bus_drop_null_seed_never_drops);
    RUN_TEST(test_host_gpio_ideal_transition_triggers_bounce);
    RUN_TEST(test_debounce_early_release_resets_in_bounce);
    return UNITY_END();
}
