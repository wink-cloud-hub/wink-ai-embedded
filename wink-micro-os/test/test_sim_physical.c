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
    RUN_TEST(test_rc_lowpass_time_regression_resets_gracefully);
    return UNITY_END();
}
