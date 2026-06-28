#include "unity.h"
#include "wink_sim_physical.h"

void setUp(void) {}
void tearDown(void) {}

void test_prng_is_deterministic_and_matches_golden(void) {
    uint32_t s1 = 1, s2 = 1;
    /* golden: seed=1 → 1103527590，返回 ≈0.5138 */
    float r1 = wink_phys_prng_next(&s1);
    TEST_ASSERT_EQUAL_UINT32(1103527590u, s1);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.5138f, r1);
    /* 可复现：同种子同序列 */
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
    /* target=true, stable=false → 抖动。强制交替（bounce_flip 初值 false） */
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    uint32_t bounce = 30000;
    TEST_ASSERT_TRUE (wink_phys_debounce_step(&ctx, true, 1000, bounce));  /* flip false→true → target=true */
    TEST_ASSERT_FALSE(wink_phys_debounce_step(&ctx, true, 2000, bounce));  /* flip true→false → !target=false */
    TEST_ASSERT_TRUE (wink_phys_debounce_step(&ctx, true, 3000, bounce));  /* flip→true */
    TEST_ASSERT_FALSE(wink_phys_debounce_step(&ctx, true, 4000, bounce));  /* flip→false */
    /* 仍在窗内（bounce_start=1000，窗 [1000,31000)） */
    TEST_ASSERT_TRUE(ctx.in_bounce);
}

void test_debounce_settles_after_window(void) {
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    uint32_t bounce = 30000;
    wink_phys_debounce_step(&ctx, true, 1000, bounce);   /* 进入抖动 */
    wink_phys_debounce_step(&ctx, true, 5000, bounce);
    /* now-bounce_start = 30000 >= bounce_us → 出窗稳定 */
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 31000, bounce));
    TEST_ASSERT_TRUE(ctx.stable_level);
    TEST_ASSERT_FALSE(ctx.in_bounce);
    /* 之后 target==stable → 直接返稳定值 */
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 50000, bounce));
}

void test_debounce_disabled_when_bounce_zero(void) {
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 0, 0));  /* 禁用 → 直接 target */
    TEST_ASSERT_TRUE(ctx.stable_level);
}

void test_debounce_null_ctx_returns_target(void) {
    TEST_ASSERT_TRUE(wink_phys_debounce_step(NULL, true, 0, 30000));  /* 降级 */
}

void test_debounce_time_regression_resets_gracefully(void) {
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    uint32_t bounce = 30000;
    /* 正常启动抖动 */
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 5000, bounce));   /* flip→true → target */
    TEST_ASSERT_TRUE(ctx.in_bounce);
    TEST_ASSERT_EQUAL_UINT64(5000, ctx.bounce_start_us);
    /* 时钟回拨：bounce_start 重置为 now，抖动窗顺延（不无限抖）；flip 继续翻转 */
    TEST_ASSERT_FALSE(wink_phys_debounce_step(&ctx, true, 0, bounce));     /* flip→false → !target */
    TEST_ASSERT_EQUAL_UINT64(0, ctx.bounce_start_us);
    TEST_ASSERT_TRUE(ctx.in_bounce);
}

void test_rc_lowpass_first_step_golden(void) {
    wink_phys_rc_ctx_t rc = { 0.0f, 0, true };
    /* current=0, target=1.0, last=0, now=1000us, tau=0.05s → dt=0.001s, alpha=0.02 → 0.02 */
    float v = wink_phys_rc_lowpass(&rc, 1.0f, 1000, 0.05f, 0.0f, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.02f, v);
}

void test_rc_lowpass_converges_to_target(void) {
    wink_phys_rc_ctx_t rc = { 0.0f, 0, true };
    uint64_t now = 0;
    for (int i = 0; i < 500; i++) { now += 10000; wink_phys_rc_lowpass(&rc, 1.0f, now, 0.05f, 0.0f, NULL); }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, rc.current);  /* 收敛到 target */
}

void test_rc_noise_bounded(void) {
    wink_phys_rc_ctx_t rc = { 0.5f, 0, true };
    uint32_t seed = 7;
    for (int i = 0; i < 100; i++) {
        float v = wink_phys_rc_lowpass(&rc, 0.5f, (uint64_t)i * 1000, 0.05f, 0.02f, &seed);
        TEST_ASSERT_TRUE(v >= 0.5f - 0.05f && v <= 0.5f + 0.05f);  /* ±0.02 噪声余量 */
    }
}

void test_rc_null_ctx_returns_target(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.7f, wink_phys_rc_lowpass(NULL, 0.7f, 0, 0.05f, 0.0f, NULL));
}

void test_rc_lowpass_uninitialized_auto_sets_target(void) {
    wink_phys_rc_ctx_t rc = { 0 }; // is_initialized = false
    float v = wink_phys_rc_lowpass(&rc, 1.5f, 1000, 0.05f, 0.0f, NULL);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, v); // 首次运行直接设置为 target
    TEST_ASSERT_TRUE(rc.is_initialized);
    TEST_ASSERT_EQUAL_UINT64(1000, rc.last_us);
}

void test_rc_lowpass_time_regression_resets_gracefully(void) {
    wink_phys_rc_ctx_t rc = { 0.5f, 2000, true };
    // 时钟回拨
    float v = wink_phys_rc_lowpass(&rc, 1.0f, 0, 0.05f, 0.0f, NULL);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, v); // 回拨时直接复位为 target
    TEST_ASSERT_EQUAL_UINT64(0, rc.last_us);
}

void test_warmup_busy_then_timeout_then_ok(void) {
    uint64_t last = 0;
    TEST_ASSERT_EQUAL(WINK_ERR_BUSY, wink_phys_warmup_check(500000, 0, 1000000, 2000000, &last));  /* 预热内 */
    TEST_ASSERT_EQUAL(WINK_ERR_TIMEOUT, wink_phys_warmup_check(1500000, 0, 1000000, 2000000, &last)); /* 间隔不足，last 不变 */
    TEST_ASSERT_EQUAL_UINT64(0, last);
    TEST_ASSERT_EQUAL(WINK_OK, wink_phys_warmup_check(2500000, 0, 1000000, 2000000, &last));  /* OK，last 更新 */
    TEST_ASSERT_EQUAL_UINT64(2500000, last);
}

void test_warmup_time_regression_resets_gracefully(void) {
    uint64_t last = 50000;
    // 时钟回拨，强制复位并允许读取
    TEST_ASSERT_EQUAL(WINK_OK, wink_phys_warmup_check(1000, 0, 0, 2000, &last));
    TEST_ASSERT_EQUAL_UINT64(1000, last);
}

void test_bus_drop_boundary_and_deterministic(void) {
    uint32_t s0 = 0, s1000 = 0, s500a = 1, s500b = 1;
    TEST_ASSERT_FALSE(wink_phys_bus_drop(0, &s0));      /* 0‰ 永不丢 */
    TEST_ASSERT_TRUE(wink_phys_bus_drop(1000, &s1000)); /* 1000‰ 总丢 */
    /* 500‰ 确定性可复现 */
    bool a = wink_phys_bus_drop(500, &s500a);
    bool b = wink_phys_bus_drop(500, &s500b);
    TEST_ASSERT_EQUAL(a, b);
}

void test_bus_drop_null_seed_never_drops(void) {
    TEST_ASSERT_FALSE(wink_phys_bus_drop(500, NULL));  /* NULL seed → 永不丢，即使 drop_permil > 0 */
}

/* ADR-0009 Wave1 Task 6：GPIO 理想注入 + pal_gpio_read 抖动退化端到端测试 */
#include "host_test_ctrl.h"
#include "pal_hal.h"

void test_host_gpio_ideal_transition_triggers_bounce(void) {
    sim_reset_time();
    wink_sim_faults_t f = WINK_SIM_FAULTS_IDEAL; f.bounce_us = 30000; f.prng_seed = 1;
    sim_set_faults(&f);
    extern void host_sim_advance_to(uint64_t us);

    /* ① 上电态：pin7=高(释放) → 注册即上电 → stable=high，无跃变、不抖 */
    sim_set_gpio_ideal(7, true);
    TEST_ASSERT_TRUE(pal_gpio_read(7));       /* target==stable → 直接返 true */

    /* ② 跃变：改为低(按下) → ideal=false，ctx.stable 仍=true → target≠stable → 进入抖动窗（强制交替） */
    host_sim_advance_to(1000);
    sim_set_gpio_ideal(7, false);
    TEST_ASSERT_FALSE(pal_gpio_read(7));      /* flip false→true → target=false */
    host_sim_advance_to(2000);
    TEST_ASSERT_TRUE (pal_gpio_read(7));      /* flip true→false → !target=true */
    host_sim_advance_to(3000);
    TEST_ASSERT_FALSE(pal_gpio_read(7));      /* flip→true → target=false */

    /* ③ 出窗（31000-1000=30000 >= bounce_us）→ 稳定到 target=false */
    host_sim_advance_to(31000);
    TEST_ASSERT_FALSE(pal_gpio_read(7));

    /* ④ 未注入、非 echo pin 仍返 false（现状不变） */
    TEST_ASSERT_FALSE(pal_gpio_read(99));
    sim_clear_gpio_ideal();
}

void test_debounce_early_release_resets_in_bounce(void) {
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    uint32_t bounce = 30000;
    
    // 1. Transition false -> true (starts bounce)
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 1000, bounce));
    TEST_ASSERT_TRUE(ctx.in_bounce);
    
    // 2. Early transition back to false (the stable level) within the window
    TEST_ASSERT_FALSE(wink_phys_debounce_step(&ctx, false, 2000, bounce));
    TEST_ASSERT_FALSE(ctx.in_bounce); // Should be reset!
    
    // 3. Transition to true again (should start a new bounce window from 3000)
    TEST_ASSERT_TRUE(wink_phys_debounce_step(&ctx, true, 3000, bounce));
    TEST_ASSERT_TRUE(ctx.in_bounce);
    TEST_ASSERT_EQUAL_UINT64(3000, ctx.bounce_start_us);
}

void test_debounce_flip_determinism_across_multiple_bounces(void) {
    wink_phys_debounce_ctx_t ctx = { false, false, 0, false };
    uint32_t bounce = 30000;
    
    // First bounce (takes an odd number of samples, say 3 samples)
    TEST_ASSERT_TRUE (wink_phys_debounce_step(&ctx, true, 1000, bounce)); // flip -> true, returns true
    TEST_ASSERT_FALSE(wink_phys_debounce_step(&ctx, true, 2000, bounce)); // flip -> false, returns false
    TEST_ASSERT_TRUE (wink_phys_debounce_step(&ctx, true, 3000, bounce)); // flip -> true, returns true
    
    // Force stabilization after window
    TEST_ASSERT_TRUE (wink_phys_debounce_step(&ctx, true, 35000, bounce)); // stable_level = true, in_bounce = false
    
    // Second bounce: transition true -> false
    // Since in_bounce starts fresh, bounce_flip should be reset to false and flip to true.
    // Returns: flip ? target (false) : !target (true) -> false.
    // If bounce_flip was NOT reset, it would have been true, flipped to false, returning !target (true).
    TEST_ASSERT_FALSE(wink_phys_debounce_step(&ctx, false, 40000, bounce));
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
    RUN_TEST(test_debounce_early_release_resets_in_bounce);
    RUN_TEST(test_debounce_flip_determinism_across_multiple_bounces);
    RUN_TEST(test_rc_lowpass_first_step_golden);
    RUN_TEST(test_rc_lowpass_converges_to_target);
    RUN_TEST(test_rc_noise_bounded);
    RUN_TEST(test_rc_null_ctx_returns_target);
    RUN_TEST(test_rc_lowpass_uninitialized_auto_sets_target);
    RUN_TEST(test_rc_lowpass_time_regression_resets_gracefully);
    RUN_TEST(test_warmup_busy_then_timeout_then_ok);
    RUN_TEST(test_warmup_time_regression_resets_gracefully);
    RUN_TEST(test_bus_drop_boundary_and_deterministic);
    RUN_TEST(test_bus_drop_null_seed_never_drops);
    RUN_TEST(test_host_gpio_ideal_transition_triggers_bounce);
    return UNITY_END();
}
