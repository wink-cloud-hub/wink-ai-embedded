/**
 * @file test_bal_rc_servo_sweep.c
 * @brief Unit tests for BAL wink_rc_servo_sweep (host).
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "tst_servo"

#include "unity.h"
#include "actuator/wink_rc_servo_sweep.h"
#include "wink_tasks.h"
#include "wink_status.h"
#include "dal_rc_servo.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "host_test_ctrl.h"

#include <string.h>

static dal_rc_servo_t s_servo1;
static dal_rc_servo_t s_servo2;

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

#include "wink_sim_scheduler.h"
#include "wink_soft_timer.h"

void setUp(void) {
    wink_rc_servo_sweep_reset();
    sim_scheduler_reset(0);
    WINK_IGNORE_RESULT(wink_soft_timer_init());
    pal_resource_reset();
    sim_clear_gpio_ideal();
    sim_reset_time();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, wink_periodic_active_count(),
        "leaked periodic handle from previous test");

    memset(&s_servo1, 0, sizeof(s_servo1));
    const dal_rc_servo_config_t cfg1 = {
        .owner = "test_servo1",
        .pwm_channel = 0,
        .min_pulse_us = 500,
        .max_pulse_us = 2500,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s_servo1, &cfg1));

    memset(&s_servo2, 0, sizeof(s_servo2));
    const dal_rc_servo_config_t cfg2 = {
        .owner = "test_servo2",
        .pwm_channel = 1,
        .min_pulse_us = 500,
        .max_pulse_us = 2500,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&s_servo2, &cfg2));
}

void tearDown(void) {
    WINK_IGNORE_RESULT(dal_rc_servo_deinit(&s_servo1));
    WINK_IGNORE_RESULT(dal_rc_servo_deinit(&s_servo2));
    sim_clear_gpio_ideal();
}

void test_servo_sweep_invalid_args(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_rc_servo_sweep_start(NULL, 10.0f, 170.0f, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_rc_servo_sweep_start(&s_servo1, 10.0f, 170.0f, 0));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_rc_servo_sweep_start(&s_servo1, 170.0f, 10.0f, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_rc_servo_sweep_set_period(NULL, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_rc_servo_sweep_set_period(&s_servo1, 0));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_rc_servo_sweep_stop(NULL));
    TEST_ASSERT_FALSE(wink_rc_servo_sweep_is_running(NULL));
}

void test_servo_sweep_lifecycle(void) {
    TEST_ASSERT_FALSE(wink_rc_servo_sweep_is_running(&s_servo1));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_sweep_start(&s_servo1, 10.0f, 170.0f, 100));
    TEST_ASSERT_TRUE(wink_rc_servo_sweep_is_running(&s_servo1));
    TEST_ASSERT_EQUAL_UINT32(1, wink_periodic_active_count());

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_sweep_stop(&s_servo1));
    TEST_ASSERT_FALSE(wink_rc_servo_sweep_is_running(&s_servo1));
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

void test_servo_sweep_duplicate_start_rejected(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_sweep_start(&s_servo1, 10.0f, 170.0f, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_rc_servo_sweep_start(&s_servo1, 10.0f, 170.0f, 50));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_sweep_stop(&s_servo1));
}

void test_servo_sweep_pool_exhaustion(void) {
    dal_rc_servo_t mocks[WINK_RC_SERVO_SWEEP_MAX + 1];
    memset(mocks, 0, sizeof(mocks));

    int count = 0;
    for (int i = 0; i < WINK_RC_SERVO_SWEEP_MAX + 1; i++) {
        dal_rc_servo_config_t cfg = {
            .owner = "mock_servo",
            .pwm_channel = (uint8_t)(2 + i),
            .min_pulse_us = 500,
            .max_pulse_us = 2500,
        };
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_init(&mocks[i], &cfg));

        wink_status_t st = wink_rc_servo_sweep_start(&mocks[i], 10.0f, 170.0f, 100);
        if (st == WINK_OK) {
            count++;
        } else {
            TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, st);
            TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&mocks[i]));
            break;
        }
    }
    TEST_ASSERT_EQUAL_INT(WINK_RC_SERVO_SWEEP_MAX, count);

    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_sweep_stop(&mocks[i]));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_rc_servo_deinit(&mocks[i]));
    }
}

void test_servo_sweep_start_stop_reclamation(void) {
    wink_bal_opts_t opts = WINK_BAL_OPTS_DEFAULT;
    opts.flags = WINK_PERIODIC_LIGHT;
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_sweep_start_ex(&s_servo1, 10.0f, 170.0f, 100, &opts));
        TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_sweep_stop(&s_servo1));
    }
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

void test_servo_sweep_set_period(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_rc_servo_sweep_set_period(&s_servo1, 200));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_sweep_start(&s_servo1, 10.0f, 170.0f, 100));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_sweep_set_period(&s_servo1, 200));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_sweep_stop(&s_servo1));
}

void test_servo_sweep_set_angle_oneshot(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_rc_servo_set_angle(&s_servo1, 90.0f));
    TEST_ASSERT_EQUAL_UINT16(900, s_servo1.current_angle_ddeg);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_servo_sweep_invalid_args);
    RUN_TEST(test_servo_sweep_lifecycle);
    RUN_TEST(test_servo_sweep_duplicate_start_rejected);
    RUN_TEST(test_servo_sweep_pool_exhaustion);
    RUN_TEST(test_servo_sweep_start_stop_reclamation);
    RUN_TEST(test_servo_sweep_set_period);
    RUN_TEST(test_servo_sweep_set_angle_oneshot);
    return UNITY_END();
}
