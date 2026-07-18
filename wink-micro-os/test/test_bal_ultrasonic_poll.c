/**
 * @file test_bal_sonar.c
 * @brief Unit tests for BAL wink_ultrasonic_poll (host).
 *
 * Copyright (c) 2026 Wink-AI.
 */
#define LOG_TAG "tst_sonar"

#include "unity.h"
#include "sensor/wink_ultrasonic_poll.h"
#include "wink_tasks.h"
#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "host_test_ctrl.h"

#include <string.h>

static dal_ultrasonic_t s_ultrasonic1;
static dal_ultrasonic_t s_ultrasonic2;

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "wink_sim_scheduler.h"
#include "wink_soft_timer.h"

void setUp(void) {
    wink_ultrasonic_poll_reset();
    sim_scheduler_reset(0);
    WINK_IGNORE_RESULT(wink_soft_timer_init());
    pal_resource_reset();
    sim_clear_gpio_ideal();
    sim_reset_time();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, wink_periodic_active_count(),
        "leaked periodic handle from previous test");

    memset(&s_ultrasonic1, 0, sizeof(s_ultrasonic1));
    const dal_ultrasonic_config_t cfg1 = {
        .owner = "test_sonar1",
        .trig_pin = 4,
        .echo_pin = 5,
        .use_rmt = false,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&s_ultrasonic1, &cfg1));

    memset(&s_ultrasonic2, 0, sizeof(s_ultrasonic2));
    const dal_ultrasonic_config_t cfg2 = {
        .owner = "test_sonar2",
        .trig_pin = 6,
        .echo_pin = 7,
        .use_rmt = false,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&s_ultrasonic2, &cfg2));
}

void tearDown(void) {
    WINK_IGNORE_RESULT(dal_ultrasonic_deinit(&s_ultrasonic1));
    WINK_IGNORE_RESULT(dal_ultrasonic_deinit(&s_ultrasonic2));
    sim_clear_gpio_ideal();
}

/* 1. Argument verification contract */
void test_sonar_helper_invalid_args(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_ultrasonic_poll_start(NULL, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_ultrasonic_poll_start(&s_ultrasonic1, 0));
    /* MIN_PERIOD=50ms floor (2026-07-07 hardening Task 3.1): values < 50 rejected */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_ultrasonic_poll_start(&s_ultrasonic1, 49));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_ultrasonic_poll_start(&s_ultrasonic1, 1));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_ultrasonic_poll_set_period(NULL, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_ultrasonic_poll_set_period(&s_ultrasonic1, 0));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_ultrasonic_poll_stop(NULL));
    TEST_ASSERT_FALSE(wink_ultrasonic_poll_is_running(NULL));
}

/* 1b. MIN_PERIOD boundary: period==50 accepted (lower bound) */
void test_sonar_helper_min_period_accepted(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_start(&s_ultrasonic1, 50));
    TEST_ASSERT_TRUE(wink_ultrasonic_poll_is_running(&s_ultrasonic1));
    /* set_period also enforces the floor */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, wink_ultrasonic_poll_set_period(&s_ultrasonic1, 49));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_set_period(&s_ultrasonic1, 60));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_stop(&s_ultrasonic1));
}

/* 2. Normal lifecycle (start -> check is_running -> stop) */
void test_sonar_helper_lifecycle(void) {
    TEST_ASSERT_FALSE(wink_ultrasonic_poll_is_running(&s_ultrasonic1));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_start(&s_ultrasonic1, 100));
    TEST_ASSERT_TRUE(wink_ultrasonic_poll_is_running(&s_ultrasonic1));
    TEST_ASSERT_EQUAL_UINT32(1, wink_periodic_active_count());

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_stop(&s_ultrasonic1));
    TEST_ASSERT_FALSE(wink_ultrasonic_poll_is_running(&s_ultrasonic1));
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* 3. Duplicate start guard */
void test_sonar_helper_duplicate_start_rejected(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_start(&s_ultrasonic1, 100));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_ultrasonic_poll_start(&s_ultrasonic1, 50));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_stop(&s_ultrasonic1));
}

/* 4. Slot pool exhaustion */
void test_sonar_helper_pool_exhaustion(void) {
    /* If WINK_ULTRASONIC_POLL_MAX is 4, we need 5 mock slots to exhaust the pool. */
    dal_ultrasonic_t mocks[WINK_ULTRASONIC_POLL_MAX + 1];
    memset(mocks, 0, sizeof(mocks));

    int count = 0;
    for (int i = 0; i < WINK_ULTRASONIC_POLL_MAX + 1; i++) {
        dal_ultrasonic_config_t cfg = {
            .owner = "mock_sonar",
            .trig_pin = (uint16_t)(8 + i * 2),
            .echo_pin = (uint16_t)(9 + i * 2),
            .use_rmt = false,
        };
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&mocks[i], &cfg));

        wink_status_t st = wink_ultrasonic_poll_start(&mocks[i], 100);
        if (st == WINK_OK) {
            count++;
        } else {
            TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, st);
            /* Clean up the one that failed to start since it was still inited */
            TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_deinit(&mocks[i]));
            break;
        }
    }
    TEST_ASSERT_EQUAL_INT(WINK_ULTRASONIC_POLL_MAX, count);

    // Clean up
    for (int i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_stop(&mocks[i]));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_deinit(&mocks[i]));
    }
}

/* 5. REGRESSION (LIFO bug): start/stop 100 times on same ultrasonic must NOT exhaust slots */
void test_sonar_helper_start_stop_reclamation(void) {
    wink_bal_opts_t opts = WINK_BAL_OPTS_DEFAULT;
    opts.flags = WINK_PERIODIC_LIGHT;
    for (int i = 0; i < 100; i++) {
        wink_status_t st = wink_ultrasonic_poll_start_ex(&s_ultrasonic1, 100, &opts);
        if (st != WINK_OK) {
            printf("Failed at iteration %d\n", i);
            TEST_ASSERT_EQUAL_INT(WINK_OK, st);
        }
        TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_stop(&s_ultrasonic1));
    }
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* 6. Period change check */
void test_sonar_helper_set_period(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_ultrasonic_poll_set_period(&s_ultrasonic1, 200));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_start(&s_ultrasonic1, 100));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_set_period(&s_ultrasonic1, 200));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_stop(&s_ultrasonic1));
}

/* 7. Preflight: starting against a zeroed (un-inited) ultrasonic must return
 *    NOT_INITIALIZED and must NOT arm a periodic (anti-"blinking in void"). */
void test_sonar_helper_uninit_rejected(void) {
    dal_ultrasonic_t uninit;
    memset(&uninit, 0, sizeof(uninit));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
        wink_ultrasonic_poll_start(&uninit, 100));
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
    TEST_ASSERT_FALSE(wink_ultrasonic_poll_is_running(&uninit));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sonar_helper_invalid_args);
    RUN_TEST(test_sonar_helper_min_period_accepted);
    RUN_TEST(test_sonar_helper_lifecycle);
    RUN_TEST(test_sonar_helper_duplicate_start_rejected);
    RUN_TEST(test_sonar_helper_pool_exhaustion);
    RUN_TEST(test_sonar_helper_start_stop_reclamation);
    RUN_TEST(test_sonar_helper_set_period);
    RUN_TEST(test_sonar_helper_uninit_rejected);
    return UNITY_END();
}
