// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_wasm_completion.c
 * @brief Unit tests for Wasm asynchronous completion pull-model scheduler.
 */
#include "unity.h"
#include "pal_wasm_completion.h"
#include "pal_osal.h"
#include <stdint.h>
#include <stdbool.h>

static int s_cb1_count = 0;
static int s_cb2_count = 0;
static int s_cb3_count = 0;
static wink_status_t s_last_status = WINK_OK;

static void cb1(void *arg, wink_status_t res) {
    (void)arg;
    s_cb1_count++;
    s_last_status = res;
}

static void cb2(void *arg, wink_status_t res) {
    (void)arg;
    s_cb2_count++;
    s_last_status = res;
}

static void cb3(void *arg, wink_status_t res) {
    (void)arg;
    s_cb3_count++;
    s_last_status = res;
}

void setUp(void) {
    pal_wasm_reset_completions();
    s_cb1_count = 0;
    s_cb2_count = 0;
    s_cb3_count = 0;
    s_last_status = WINK_OK;
}

void tearDown(void) {
    pal_wasm_reset_completions();
}

void test_completion_schedule_and_drain(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_wasm_schedule_complete_us(1000, cb1, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_wasm_schedule_complete_us(2000, cb2, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_wasm_schedule_complete_with_result(3000, cb3, NULL, WINK_ERR_TIMEOUT));

    TEST_ASSERT_EQUAL_UINT32(3, pal_wasm_get_pending_completions_count());

    /* Busy wait 1200us on host clock */
    pal_os_busy_wait_us(1200);
    pal_wasm_drain_completions();
    TEST_ASSERT_EQUAL_INT(1, s_cb1_count);
    TEST_ASSERT_EQUAL_INT(0, s_cb2_count);
    TEST_ASSERT_EQUAL_INT(0, s_cb3_count);
    TEST_ASSERT_EQUAL_UINT32(2, pal_wasm_get_pending_completions_count());

    /* Busy wait another 1000us (total >2200us) */
    pal_os_busy_wait_us(1000);
    pal_wasm_drain_completions();
    TEST_ASSERT_EQUAL_INT(1, s_cb1_count);
    TEST_ASSERT_EQUAL_INT(1, s_cb2_count);
    TEST_ASSERT_EQUAL_INT(0, s_cb3_count);

    /* Busy wait another 1000us (total >3200us) */
    pal_os_busy_wait_us(1000);
    pal_wasm_drain_completions();
    TEST_ASSERT_EQUAL_INT(1, s_cb1_count);
    TEST_ASSERT_EQUAL_INT(1, s_cb2_count);
    TEST_ASSERT_EQUAL_INT(1, s_cb3_count);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, s_last_status);
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_pending_completions_count());
}

void test_completion_overflow_reject(void) {
    for (int i = 0; i < PAL_WASM_MAX_PENDING_COMPLETIONS; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_wasm_schedule_complete_us(100000, cb1, NULL));
    }
    TEST_ASSERT_EQUAL_UINT32(PAL_WASM_MAX_PENDING_COMPLETIONS, pal_wasm_get_pending_completions_count());

    /* 33rd item must be rejected with WINK_ERR_RESOURCE_EXHAUSTED */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, pal_wasm_schedule_complete_us(100000, cb1, NULL));

    pal_wasm_reset_completions();
    TEST_ASSERT_EQUAL_UINT32(0, pal_wasm_get_pending_completions_count());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_completion_schedule_and_drain);
    RUN_TEST(test_completion_overflow_reject);
    return UNITY_END();
}
