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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_prng_is_deterministic_and_matches_golden);
    RUN_TEST(test_prng_in_unit_range);
    RUN_TEST(test_debounce_forced_alternation_within_window);
    RUN_TEST(test_debounce_settles_after_window);
    RUN_TEST(test_debounce_disabled_when_bounce_zero);
    RUN_TEST(test_debounce_null_ctx_returns_target);
    RUN_TEST(test_debounce_time_regression_resets_gracefully);
    return UNITY_END();
}
