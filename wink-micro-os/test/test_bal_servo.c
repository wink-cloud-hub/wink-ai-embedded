/**
 * @file test_bal_servo.c
 * @brief Unit tests for BAL wink_servo_helper (host).
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "tst_servo"

#include "unity.h"
#include "actuator/wink_servo_helper.h"
#include "wink_tasks.h"
#include "wink_status.h"
#include "dal_servo.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "host_test_ctrl.h"

#include <string.h>

static dal_servo_t s_servo1;
static dal_servo_t s_servo2;

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "wink_sim_scheduler.h"
#include "wink_soft_timer.h"

void setUp(void) {
    wink_servo_helper_reset();
    sim_scheduler_reset(0);
    WINK_IGNORE_RESULT(wink_soft_timer_init());
    pal_resource_reset();
    sim_clear_gpio_ideal();
    sim_reset_time();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, wink_periodic_active_count(),
        "leaked periodic handle from previous test");

    memset(&s_servo1, 0, sizeof(s_servo1));
    const dal_servo_config_t cfg1 = {
        .owner = "test_servo1",
        .pwm_channel = 0,
        .min_pulse_ms = 0.5f,
        .max_pulse_ms = 2.5f,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_init(&s_servo1, &cfg1));

    memset(&s_servo2, 0, sizeof(s_servo2));
    const dal_servo_config_t cfg2 = {
        .owner = "test_servo2",
        .pwm_channel = 1,
        .min_pulse_ms = 0.5f,
        .max_pulse_ms = 2.5f,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_init(&s_servo2, &cfg2));
}

void tearDown(void) {
    WINK_IGNORE_RESULT(dal_servo_deinit(&s_servo1));
    WINK_IGNORE_RESULT(dal_servo_deinit(&s_servo2));
    sim_clear_gpio_ideal();
}

/* 1. Argument verification contract */
void test_servo_helper_invalid_args(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_servo_sweep_start(NULL, 10.0f, 170.0f, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_servo_sweep_start(&s_servo1, 10.0f, 170.0f, 0));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_servo_sweep_start(&s_servo1, 170.0f, 10.0f, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_servo_helper_set_period(NULL, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_servo_helper_set_period(&s_servo1, 0));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_servo_helper_stop(NULL));
    TEST_ASSERT_FALSE(wink_servo_helper_is_running(NULL));
}

/* 2. Normal lifecycle (start -> check is_running -> stop) */
void test_servo_helper_lifecycle(void) {
    TEST_ASSERT_FALSE(wink_servo_helper_is_running(&s_servo1));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_sweep_start(&s_servo1, 10.0f, 170.0f, 100));
    TEST_ASSERT_TRUE(wink_servo_helper_is_running(&s_servo1));
    TEST_ASSERT_EQUAL_UINT32(1, wink_periodic_active_count());

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_helper_stop(&s_servo1));
    TEST_ASSERT_FALSE(wink_servo_helper_is_running(&s_servo1));
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* 3. Duplicate start guard */
void test_servo_helper_duplicate_start_rejected(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_sweep_start(&s_servo1, 10.0f, 170.0f, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_servo_sweep_start(&s_servo1, 10.0f, 170.0f, 50));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_helper_stop(&s_servo1));
}

/* 4. Slot pool exhaustion */
void test_servo_helper_pool_exhaustion(void) {
    /* If WINK_SERVO_HELPER_MAX is 4, we need 5 mock slots to exhaust the pool. */
    dal_servo_t mocks[WINK_SERVO_HELPER_MAX + 1];
    memset(mocks, 0, sizeof(mocks));

    int count = 0;
    for (int i = 0; i < WINK_SERVO_HELPER_MAX + 1; i++) {
        dal_servo_config_t cfg = {
            .owner = "mock_servo",
            .pwm_channel = (uint8_t)(2 + i),
            .min_pulse_ms = 0.5f,
            .max_pulse_ms = 2.5f,
        };
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_init(&mocks[i], &cfg));

        wink_status_t st = wink_servo_sweep_start(&mocks[i], 10.0f, 170.0f, 100);
        if (st == WINK_OK) {
            count++;
        } else {
            TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, st);
            /* Clean up the one that failed to start since it was still inited */
            TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_deinit(&mocks[i]));
            break;
        }
    }
    TEST_ASSERT_EQUAL_INT(WINK_SERVO_HELPER_MAX, count);

    // Clean up
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_helper_stop(&mocks[i]));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_servo_deinit(&mocks[i]));
    }
}

/* 5. REGRESSION: start/stop 100 times must NOT exhaust slots */
void test_servo_helper_start_stop_reclamation(void) {
    wink_helper_opts_t opts = WINK_HELPER_OPTS_DEFAULT;
    opts.flags = WINK_PERIODIC_LIGHT;
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_sweep_start_ex(&s_servo1, 10.0f, 170.0f, 100, &opts));
        TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_helper_stop(&s_servo1));
    }
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* 6. Period change check */
void test_servo_helper_set_period(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_servo_helper_set_period(&s_servo1, 200));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_sweep_start(&s_servo1, 10.0f, 170.0f, 100));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_helper_set_period(&s_servo1, 200));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_helper_stop(&s_servo1));
}

/* 7. Oneshot angle setting check */
void test_servo_helper_set_angle_oneshot(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_servo_set_angle(&s_servo1, 90.0f));
    TEST_ASSERT_EQUAL_FLOAT(90.0f, s_servo1.current_angle);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_servo_helper_invalid_args);
    RUN_TEST(test_servo_helper_lifecycle);
    RUN_TEST(test_servo_helper_duplicate_start_rejected);
    RUN_TEST(test_servo_helper_pool_exhaustion);
    RUN_TEST(test_servo_helper_start_stop_reclamation);
    RUN_TEST(test_servo_helper_set_period);
    RUN_TEST(test_servo_helper_set_angle_oneshot);
    return UNITY_END();
}
