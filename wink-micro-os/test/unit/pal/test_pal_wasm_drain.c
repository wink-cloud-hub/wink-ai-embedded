// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_wasm_drain.c
 * @brief Unit tests for Wasm virtual clock drain, hwtimer catch-up, and completion mechanisms.
 */
#include "unity.h"
#include "pal_wasm_hwtimer.h"
#include "pal_wasm_completion.h"
#include "pal_wasm_fault_types.h"
#include "pal_osal.h"
#include <stdint.h>
#include <stdbool.h>

static int s_timer_fire_count = 0;
static uint64_t s_mock_time_us = 0;

uint64_t pal_os_get_time_us(void) {
    return s_mock_time_us;
}

uint64_t pal_os_get_us(void) {
    return s_mock_time_us;
}

void pal_os_busy_wait_us(uint32_t us) {
    s_mock_time_us += us;
}

void pal_wasm_log_fault(int fault_type) { (void)fault_type; }
void pal_wasm_invoke_fault(int fault_type) { (void)fault_type; }

static void timer_cb(void *arg) {
    (void)arg;
    s_timer_fire_count++;
}

void setUp(void) {
    pal_wasm_reset_completions();
    s_timer_fire_count = 0;
}

void tearDown(void) {
    pal_hwtimer_deinit(0);
    pal_wasm_reset_completions();
}

void test_hwtimer_basic_and_catchup(void) {
    pal_hwtimer_cfg_t cfg = {
        .timer_id = 0,
        .period_us = 1000,
        .auto_start = true,
        .oneshot = false,
        .callback = timer_cb,
        .callback_arg = NULL
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_init(&cfg));

    /* Advance time by 3500us on host clock */
    pal_os_busy_wait_us(3500);
    pal_wasm_hwtimer_drain();

    /* Should catch up 3 periods */
    TEST_ASSERT_EQUAL_INT(3, s_timer_fire_count);
}

void test_hwtimer_change_period_phase_preservation(void) {
    pal_hwtimer_cfg_t cfg = {
        .timer_id = 0,
        .period_us = 1000,
        .auto_start = true,
        .oneshot = false,
        .callback = timer_cb,
        .callback_arg = NULL
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_init(&cfg));

    /* Request period change to 2000us */
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_hwtimer_change_period(0, 2000));

    /* Advance 1500us -> fires at 1000us (first period), new period 2000us applied for next fire at 3000us */
    pal_os_busy_wait_us(1500);
    pal_wasm_hwtimer_drain();
    TEST_ASSERT_EQUAL_INT(1, s_timer_fire_count);

    /* Advance another 1000us (total 2500us) -> should NOT fire yet (next is 3000us) */
    pal_os_busy_wait_us(1000);
    pal_wasm_hwtimer_drain();
    TEST_ASSERT_EQUAL_INT(1, s_timer_fire_count);

    /* Advance another 600us (total 3100us) -> should fire 2nd time */
    pal_os_busy_wait_us(600);
    pal_wasm_hwtimer_drain();
    TEST_ASSERT_EQUAL_INT(2, s_timer_fire_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hwtimer_basic_and_catchup);
    RUN_TEST(test_hwtimer_change_period_phase_preservation);
    return UNITY_END();
}
