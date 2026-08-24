// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_deferred.c
 * @brief Unit tests for PAL Deferred-Call Worker (ISR Bottom-Half) subsystem.
 */
#include "unity.h"
#include "osal/pal_deferred.h"
#include <string.h>

void setUp(void) {
    pal_deferred_init(0);
}

void tearDown(void) {
    pal_deferred_deinit();
}

static int s_hi_call_count = 0;
static int s_lo_call_count = 0;
static void *s_last_hi_arg = NULL;
static void *s_last_lo_arg = NULL;

static void dummy_hi_cb(void *arg) {
    s_hi_call_count++;
    s_last_hi_arg = arg;
}

static void dummy_lo_cb(void *arg) {
    s_lo_call_count++;
    s_last_lo_arg = arg;
}

void test_deferred_init_and_post_basic(void) {
    s_hi_call_count = 0;
    s_lo_call_count = 0;
    s_last_hi_arg = NULL;
    s_last_lo_arg = NULL;

    int dummy_val1 = 42;
    int dummy_val2 = 100;

    wink_status_t st = pal_deferred_post(PAL_DEFERRED_HI, PAL_DEFERRED_LOSSY, dummy_hi_cb, &dummy_val1);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL_INT(1, s_hi_call_count);
    TEST_ASSERT_EQUAL_PTR(&dummy_val1, s_last_hi_arg);

    st = pal_deferred_post_from_isr(PAL_DEFERRED_LO, PAL_DEFERRED_LOSSY, dummy_lo_cb, &dummy_val2);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL_INT(1, s_lo_call_count);
    TEST_ASSERT_EQUAL_PTR(&dummy_val2, s_last_lo_arg);
}

void test_deferred_invalid_args(void) {
    /* NULL callback must be rejected */
    wink_status_t st = pal_deferred_post(PAL_DEFERRED_HI, PAL_DEFERRED_LOSSY, NULL, NULL);
    TEST_ASSERT_EQUAL(WINK_ERR_INVALID_ARG, st);

    /* Invalid priority must be rejected */
    st = pal_deferred_post((pal_deferred_pri_t)99, PAL_DEFERRED_LOSSY, dummy_hi_cb, NULL);
    TEST_ASSERT_EQUAL(WINK_ERR_INVALID_ARG, st);
}

void test_deferred_metrics(void) {
    size_t high_water = 0;
    uint32_t dropped = 0;

    pal_deferred_get_metrics(PAL_DEFERRED_HI, &high_water, &dropped);
    TEST_ASSERT_EQUAL_UINT32(0, dropped);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_deferred_init_and_post_basic);
    RUN_TEST(test_deferred_invalid_args);
    RUN_TEST(test_deferred_metrics);
    return UNITY_END();
}
