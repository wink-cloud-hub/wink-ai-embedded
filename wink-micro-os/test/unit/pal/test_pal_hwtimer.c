// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_hwtimer.c
 * @brief PAL HWTIMER hardware timer fast-loop unit tests.
 */
#include "unity.h"
#include "hal/pal_hwtimer.h"
#include "pal_resource.h"
#include "pal_hwtimer_stub.h"

static volatile uint32_t s_isr_count = 0;

static void test_timer_isr_cb(void *arg) {
    uint32_t *flag = (uint32_t *)arg;
    if (flag != NULL) {
        (*flag)++;
    }
}

void setUp(void) {
    pal_resource_reset();
    for (uint8_t i = 0; i < PAL_HWTIMERS_MAX; i++) {
        pal_hwtimer_deinit(i);
    }
    s_isr_count = 0;
}

void tearDown(void) {
    for (uint8_t i = 0; i < PAL_HWTIMERS_MAX; i++) {
        pal_hwtimer_deinit(i);
    }
    pal_resource_reset();
}

void test_hwtimer_init_deinit(void) {
    pal_hwtimer_cfg_t cfg = {
        .timer_id = 0,
        .period_us = 50,
        .oneshot = false,
        .auto_start = false,
        .core_affinity = PAL_OS_CORE_1,
        .isr_priority = 2,
        .uses_fpu = false,
        .callback = test_timer_isr_cb,
        .callback_arg = (void *)&s_isr_count,
    };

    for (uint8_t i = 0; i < PAL_HWTIMERS_MAX; i++) {
        cfg.timer_id = i;
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_init(&cfg));
    }

    /* Out of bounds timer */
    cfg.timer_id = PAL_HWTIMERS_MAX;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_hwtimer_init(&cfg));

    /* Duplicate init */
    cfg.timer_id = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_hwtimer_init(&cfg));

    /* Deinit and re-init */
    pal_hwtimer_deinit(0);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_init(&cfg));
}

void test_hwtimer_periodic_fire_soft(void) {
    pal_hwtimer_cfg_t cfg = {
        .timer_id = 1,
        .period_us = 50, /* 20 kHz */
        .oneshot = false,
        .auto_start = true,
        .callback = test_timer_isr_cb,
        .callback_arg = (void *)&s_isr_count,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_init(&cfg));
    TEST_ASSERT_TRUE(stub_hwtimer_is_running(1));

    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_fire_soft(1));
    }

    TEST_ASSERT_EQUAL_UINT32(10, s_isr_count);
    TEST_ASSERT_TRUE(stub_hwtimer_is_running(1));

    /* Stop timer */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_stop(1));
    TEST_ASSERT_FALSE(stub_hwtimer_is_running(1));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, pal_hwtimer_fire_soft(1));

    /* Restart timer */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_start(1));
    TEST_ASSERT_TRUE(stub_hwtimer_is_running(1));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_fire_soft(1));
    TEST_ASSERT_EQUAL_UINT32(11, s_isr_count);

    pal_hwtimer_deinit(1);
}

void test_hwtimer_oneshot_fire_soft(void) {
    pal_hwtimer_cfg_t cfg = {
        .timer_id = 2,
        .period_us = 1000,
        .oneshot = true,
        .auto_start = true,
        .callback = test_timer_isr_cb,
        .callback_arg = (void *)&s_isr_count,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_init(&cfg));
    TEST_ASSERT_TRUE(stub_hwtimer_is_running(2));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_fire_soft(2));
    TEST_ASSERT_EQUAL_UINT32(1, s_isr_count);
    TEST_ASSERT_FALSE(stub_hwtimer_is_running(2));

    /* Firing again should fail */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, pal_hwtimer_fire_soft(2));

    pal_hwtimer_deinit(2);
}

void test_hwtimer_change_period(void) {
    pal_hwtimer_cfg_t cfg = {
        .timer_id = 0,
        .period_us = 50,
        .auto_start = false,
        .callback = test_timer_isr_cb,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_init(&cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_change_period(0, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_hwtimer_change_period(0, 0));
    pal_hwtimer_deinit(0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hwtimer_init_deinit);
    RUN_TEST(test_hwtimer_periodic_fire_soft);
    RUN_TEST(test_hwtimer_oneshot_fire_soft);
    RUN_TEST(test_hwtimer_change_period);
    return UNITY_END();
}
