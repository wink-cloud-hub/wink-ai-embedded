// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_sim_scheduler_stack_clamp.c
 * @brief Unit tests for simulation scheduler stack size clamping behavior.
 */
#include "unity.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include <stdint.h>

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

static volatile bool s_task_ran = false;

static void tiny_stack_task(void* arg) {
    (void)arg;
    volatile uint8_t buf[4096];
    buf[0] = 0xAA;
    buf[4095] = 0x55;
    if (buf[0] == 0xAA && buf[4095] == 0x55) {
        s_task_ran = true;
    }
    pal_os_task_delete(NULL);
}

void setUp(void) { s_task_ran = false; }
void tearDown(void) { sim_scheduler_reset(0); }

void test_1024_stack_is_clamped_and_task_runs(void) {
    sim_scheduler_reset(42);

    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(
        tiny_stack_task, "tiny", 1024, NULL, 5, PAL_OS_CORE_ANY, &h));

    (void)pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);

    TEST_ASSERT_TRUE_MESSAGE(s_task_ran,
        "task with clamped 1KB->32KB stack must run to completion");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_1024_stack_is_clamped_and_task_runs);
    return UNITY_END();
}
