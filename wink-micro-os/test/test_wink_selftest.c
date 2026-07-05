/**
 * @file test_wink_selftest.c
 * @brief Selftest 框架单元测试：
 *   - count() 与注册表一致
 *   - 全量 "*" 运行：不崩，返回码符合预期
 *   - glob 前缀/后缀/精确匹配筛选正确
 *   - metric / note 非 NULL
 *   - 各 selftest 在 host 上的预期状态：
 *       pwm_router.freq_isolation : PASS（host PWM router 纯内存状态机）
 *       i2c.bus_scan             : PASS（pal_i2c_scan 在 host 上是 stub）
 *       smp.resource_stress      : PASS（单线程循环 + 尝试 spawn）
 *       gpio.isr_roundtrip       : PASS（注册路径 OK，fired 可能为 0 或 2）
 *       rmt.self_loopback        : SKIP（host RMT 返回 UNSUPPORTED）
 */
#include "unity.h"
#include "wink_selftest.h"
#include "wink_status.h"
#include "wink_log.h"

/* test/stubs 提供 host_test_ctrl，包含 sim_reset_time() 等重置 helper */
#include "host_test_ctrl.h"

void setUp(void)
{
    sim_reset_time();
}

void tearDown(void)
{
    sim_reset_time();
}

/* ─────────────────────────────────────────────────────────
 * Case 1: 注册表条目数正确
 * ───────────────────────────────────────────────────────── */
void test_selftest_count_matches_registry(void)
{
    size_t n = wink_selftest_count();
    /* 本 wave 注册 5 项：pwm_router / i2c / smp / gpio_isr / rmt */
    TEST_ASSERT_EQUAL_UINT32(5u, (uint32_t)n);
}

/* ─────────────────────────────────────────────────────────
 * Case 2: "*" 跑所有条目，返回第一个 FAIL（这里期望全 PASS 或一个 SKIP）
 * ───────────────────────────────────────────────────────── */
void test_selftest_run_all_does_not_crash(void)
{
    wink_selftest_result_t results[8];
    size_t n = 0;
    wink_status_t st = wink_selftest_run("*", results, 8, &n);
    /* 不允许出现真正的失败（非 OK/UNSUPPORTED）。
     * st == WINK_OK：所有非 SKIP 条目都 PASS。
     * st == WINK_ERR_UNSUPPORTED：某条目返 SKIP——但我们只有 RMT 可能 SKIP，
     * 其他都应 PASS；SKIP 不进入 first_fail（selftest_core 里 UNSUPPORTED 不算 fail）。
     * 所以 st 应该是 WINK_OK。*/
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(5u, (uint32_t)n);

    /* 所有条目有名字，metric 合理，status 不是硬错 */
    for (size_t i = 0; i < n; i++) {
        TEST_ASSERT_NOT_NULL(results[i].name);
        TEST_ASSERT_TRUE(results[i].status == WINK_OK
                         || results[i].status == WINK_ERR_UNSUPPORTED);
    }
}

/* ─────────────────────────────────────────────────────────
 * Case 3: glob 前缀匹配 "pwm*" 只选出 pwm_router 一条
 * ───────────────────────────────────────────────────────── */
void test_selftest_glob_prefix(void)
{
    wink_selftest_result_t results[4];
    size_t n = 0;
    wink_status_t st = wink_selftest_run("pwm*", results, 4, &n);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)n);
    TEST_ASSERT_EQUAL_STRING("pwm_router.freq_isolation", results[0].name);
    TEST_ASSERT_EQUAL_INT(WINK_OK, results[0].status);
}

/* ─────────────────────────────────────────────────────────
 * Case 4: glob 后缀匹配 "*loopback" 只选出 rmt.self_loopback
 * ───────────────────────────────────────────────────────── */
void test_selftest_glob_suffix(void)
{
    wink_selftest_result_t results[4];
    size_t n = 0;
    WINK_IGNORE_RESULT(wink_selftest_run("*loopback", results, 4, &n));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)n);
    TEST_ASSERT_EQUAL_STRING("rmt.self_loopback", results[0].name);
    /* host 上 RMT 通过软件仿真完整实现 → PASS（pulse captured）。
     * 不支持 RMT 的 target 才会返 WINK_ERR_UNSUPPORTED（SKIP）。*/
    TEST_ASSERT_TRUE(results[0].status == WINK_OK
                     || results[0].status == WINK_ERR_UNSUPPORTED);
    if (results[0].status == WINK_OK) {
        /* host 仿真应捕获到 90..110us */
        TEST_ASSERT_TRUE(results[0].metric >= 90u && results[0].metric <= 110u);
    }
}

/* ─────────────────────────────────────────────────────────
 * Case 5: 精确匹配 i2c.bus_scan → PASS
 * ───────────────────────────────────────────────────────── */
void test_selftest_exact_i2c(void)
{
    wink_selftest_result_t r;
    size_t n = 0;
    wink_status_t st = wink_selftest_run("i2c.bus_scan", &r, 1, &n);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)n);
    TEST_ASSERT_EQUAL_INT(WINK_OK, r.status);
}

/* ─────────────────────────────────────────────────────────
 * Case 6: NULL glob → INVALID_ARG
 * ───────────────────────────────────────────────────────── */
void test_selftest_null_glob_returns_invalid_arg(void)
{
    size_t n = 0;
    wink_status_t st = wink_selftest_run(NULL, NULL, 0, &n);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, st);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)n);
}

/* ─────────────────────────────────────────────────────────
 * Case 7: 不匹配的 glob → 0 个条目，返回 OK（没有失败）
 * ───────────────────────────────────────────────────────── */
void test_selftest_no_match_returns_ok_with_zero(void)
{
    wink_selftest_result_t r;
    size_t n = 0xDEADu;
    wink_status_t st = wink_selftest_run("nonexistent.test", &r, 1, &n);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)n);
}

/* ─────────────────────────────────────────────────────────
 * Case 8: cap=0 + results=NULL → 只计数，不写
 * ───────────────────────────────────────────────────────── */
void test_selftest_count_only_mode(void)
{
    size_t n = 0;
    wink_status_t st = wink_selftest_run("*", NULL, 0, &n);
    /* count-only 模式也应返回 OK（所有非 SKIP 条目 PASS） */
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(5u, (uint32_t)n);
}

/* ─────────────────────────────────────────────────────────
 * Case 9: 溢出 cap → 只填前 cap 项，out_count 是匹配总数
 * ───────────────────────────────────────────────────────── */
void test_selftest_cap_small_still_counts_all(void)
{
    wink_selftest_result_t r[2];
    size_t n = 0;
    WINK_IGNORE_RESULT(wink_selftest_run("*", r, 2, &n));
    TEST_ASSERT_EQUAL_UINT32(5u, (uint32_t)n);
}

/* ─────────────────────────────────────────────────────────
 * main
 * ───────────────────────────────────────────────────────── */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_selftest_count_matches_registry);
    RUN_TEST(test_selftest_run_all_does_not_crash);
    RUN_TEST(test_selftest_glob_prefix);
    RUN_TEST(test_selftest_glob_suffix);
    RUN_TEST(test_selftest_exact_i2c);
    RUN_TEST(test_selftest_null_glob_returns_invalid_arg);
    RUN_TEST(test_selftest_no_match_returns_ok_with_zero);
    RUN_TEST(test_selftest_count_only_mode);
    RUN_TEST(test_selftest_cap_small_still_counts_all);
    return UNITY_END();
}
