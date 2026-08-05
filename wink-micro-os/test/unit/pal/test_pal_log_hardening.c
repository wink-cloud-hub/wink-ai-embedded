// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_log_hardening.c
 * @brief Host target PAL log hardening and ISR safety unit tests.
 */
#define LOG_TAG "hrdtest"

#include "unity.h"
#include "pal_log.h"
#include "pal_osal.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {
    pal_os_set_sim_isr_context(false);
}

void test_log_shorthand_with_tag(void)
{
    LOG_E("hardening test error: code=%d", -1);
    LOG_W("hardening test warn: threshold=%u", 42u);
    LOG_I("hardening test info: %s", "hello");
    LOG_D("hardening test debug: x=%d", 7);
    TEST_PASS();
}

void test_log_explicit_tag_entrypoints(void)
{
    pal_log_e("explicit", "explicit-tag e: %d", 1);
    pal_log_w("explicit", "explicit-tag w");
    pal_log_i("explicit", "explicit-tag i: %s", "ok");
    pal_log_d("explicit", "explicit-tag d");
    TEST_PASS();
}

void test_pal_log_in_isr_tracks_sim_flag(void)
{
    pal_os_set_sim_isr_context(false);
    TEST_ASSERT_FALSE(pal_log_in_isr());

    pal_os_set_sim_isr_context(true);
    TEST_ASSERT_TRUE(pal_log_in_isr());

    pal_os_set_sim_isr_context(false);
    TEST_ASSERT_FALSE(pal_log_in_isr());
}

void test_log_in_isr_context_does_not_crash(void)
{
    pal_os_set_sim_isr_context(true);
    TEST_ASSERT_TRUE(pal_log_in_isr());

    LOG_E("isr error: rc=%d", -12);
    LOG_W("isr warning: pin=%u", 13u);

    LOG_I("isr info — should be dropped");
    LOG_D("isr debug — should be dropped");

    pal_os_set_sim_isr_context(false);
    TEST_ASSERT_FALSE(pal_log_in_isr());

    LOG_I("post-isr info OK");
    TEST_PASS();
}

void test_log_explicit_tag_in_isr(void)
{
    pal_os_set_sim_isr_context(true);
    pal_log_e("isr_explicit", "isr explicit e");
    pal_log_w("isr_explicit", "isr explicit w");
    pal_log_i("isr_explicit", "isr explicit i — dropped");
    pal_log_d("isr_explicit", "isr explicit d — dropped");
    pal_os_set_sim_isr_context(false);
    TEST_PASS();
}

void test_log_sys_fallback_callable(void)
{
    pal_log_i("SYS", "sys-fallback smoke %d", 1);
    TEST_PASS();
}

extern void test_pal_log_sys_fallback_run(void);

extern void test_pal_log_level_contract(void);
void test_pal_log_level_contract(void)
{
    TEST_ASSERT_EQUAL_INT(0, PAL_LOG_LEVEL_NONE);
    TEST_ASSERT_EQUAL_INT(1, PAL_LOG_LEVEL_ERROR);
    TEST_ASSERT_EQUAL_INT(2, PAL_LOG_LEVEL_WARN);
    TEST_ASSERT_EQUAL_INT(3, PAL_LOG_LEVEL_INFO);
    TEST_ASSERT_EQUAL_INT(4, PAL_LOG_LEVEL_DEBUG);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_log_shorthand_with_tag);
    RUN_TEST(test_log_explicit_tag_entrypoints);
    RUN_TEST(test_pal_log_in_isr_tracks_sim_flag);
    RUN_TEST(test_log_in_isr_context_does_not_crash);
    RUN_TEST(test_log_explicit_tag_in_isr);
    RUN_TEST(test_log_sys_fallback_callable);
    RUN_TEST(test_pal_log_sys_fallback_run);
    RUN_TEST(test_pal_log_level_contract);
    return UNITY_END();
}
