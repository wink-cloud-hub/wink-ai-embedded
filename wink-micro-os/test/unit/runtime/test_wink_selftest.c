// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_wink_selftest.c
 * @brief Selftest framework unit tests.
 */
#include "unity.h"
#include "wink_selftest.h"
#include "wink_status.h"
#include "wink_log.h"
#include "host_test_ctrl.h"

void setUp(void)
{
    sim_reset_time();
}

void tearDown(void)
{
    sim_reset_time();
}

void test_selftest_count_matches_registry(void)
{
    size_t n = wink_selftest_count();
    TEST_ASSERT_EQUAL_UINT32(5u, (uint32_t)n);
}

void test_selftest_run_all_does_not_crash(void)
{
    wink_selftest_result_t results[8];
    size_t n = 0;
    wink_status_t st = wink_selftest_run("*", results, 8, &n);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(5u, (uint32_t)n);

    for (size_t i = 0; i < n; i++) {
        TEST_ASSERT_NOT_NULL(results[i].name);
        TEST_ASSERT_TRUE(results[i].status == WINK_OK
                         || results[i].status == WINK_ERR_UNSUPPORTED);
    }
}

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

void test_selftest_glob_suffix(void)
{
    wink_selftest_result_t results[4];
    size_t n = 0;
    WINK_IGNORE_RESULT(wink_selftest_run("*loopback", results, 4, &n));
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)n);
    TEST_ASSERT_EQUAL_STRING("rmt.self_loopback", results[0].name);
    TEST_ASSERT_TRUE(results[0].status == WINK_OK
                     || results[0].status == WINK_ERR_UNSUPPORTED);
    if (results[0].status == WINK_OK) {
        TEST_ASSERT_TRUE(results[0].metric >= 90u && results[0].metric <= 110u);
    }
}

void test_selftest_exact_i2c(void)
{
    wink_selftest_result_t r;
    size_t n = 0;
    wink_status_t st = wink_selftest_run("i2c.bus_scan", &r, 1, &n);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)n);
    TEST_ASSERT_EQUAL_INT(WINK_OK, r.status);
}

void test_selftest_null_glob_returns_invalid_arg(void)
{
    size_t n = 0;
    wink_status_t st = wink_selftest_run(NULL, NULL, 0, &n);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, st);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)n);
}

void test_selftest_no_match_returns_ok_with_zero(void)
{
    wink_selftest_result_t r;
    size_t n = 0xDEADu;
    wink_status_t st = wink_selftest_run("nonexistent.test", &r, 1, &n);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)n);
}

void test_selftest_count_only_mode(void)
{
    size_t n = 0;
    wink_status_t st = wink_selftest_run("*", NULL, 0, &n);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_EQUAL_UINT32(5u, (uint32_t)n);
}

void test_selftest_cap_small_still_counts_all(void)
{
    wink_selftest_result_t r[2];
    size_t n = 0;
    WINK_IGNORE_RESULT(wink_selftest_run("*", r, 2, &n));
    TEST_ASSERT_EQUAL_UINT32(5u, (uint32_t)n);
}

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
