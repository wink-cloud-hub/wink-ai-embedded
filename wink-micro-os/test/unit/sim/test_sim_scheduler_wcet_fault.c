// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_sim_scheduler_wcet_fault.c
 * @brief WCET 8002 fault trigger unit tests for simulation scheduler.
 */
#include "unity.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include "host_wall_clock.h"
#include <stdint.h>
#include <stdlib.h>

#include "wink_app.h"

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

static bool s_fault_fired = false;
static uint32_t s_fault_code = 0;
static bool s_app_on_fault_called = false;
static const wink_app_callbacks_t* s_captured_cb = NULL;

void wink_runtime_fault(const struct wink_app_callbacks* cb, uint32_t code) {
    s_fault_fired = true;
    s_fault_code = code;
    s_captured_cb = cb;
    if (cb && cb->on_fault) {
        cb->on_fault(code);
    }
}

static void test_app_on_fault(uint32_t code) {
    (void)code;
    s_app_on_fault_called = true;
}

static const wink_app_callbacks_t s_test_callbacks = {
    .init = NULL,
    .loop = NULL,
    .on_fault = test_app_on_fault,
};

static void cpu_hog_task(void* arg) {
    (void)arg;
    uint64_t start = host_wall_clock_us();
    while ((host_wall_clock_us() - start) < 15000ULL) {
    }
    pal_os_task_delete(NULL);
}

static void busy_wait_task(void* arg) {
    (void)arg;
    pal_os_busy_wait_us(50000);
    pal_os_task_delete(NULL);
}

void setUp(void) {
    s_fault_fired = false;
    s_fault_code = 0;
    s_app_on_fault_called = false;
    s_captured_cb = NULL;
    _putenv("CI=");
    _putenv("WINK_SIM_BYPASS_WCET=");
    wink_sim_set_mode(WINK_SIM_MODE_INTERACTIVE);
}

void tearDown(void) {
    sim_scheduler_reset(0);
}

void test_cpu_hog_triggers_8002(void) {
    sim_scheduler_reset(42);

    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(
        cpu_hog_task, "hog", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &h));

    (void)pal_sim_scheduler_run(&s_test_callbacks, SIM_SCHED_NO_READY, 100);

    TEST_ASSERT_TRUE_MESSAGE(s_fault_fired,
        "WCET fault must fire when task busy-loops > threshold");
    TEST_ASSERT_EQUAL_UINT32(8002, s_fault_code);

    TEST_ASSERT_NOT_NULL_MESSAGE(s_captured_cb,
        "wink_runtime_fault must receive a non-NULL cb pointer");
    TEST_ASSERT_EQUAL_PTR_MESSAGE(&s_test_callbacks, s_captured_cb,
        "wink_runtime_fault cb must match the callbacks passed to pal_sim_scheduler_run");

    TEST_ASSERT_TRUE_MESSAGE(s_app_on_fault_called,
        "App on_fault must be invoked with WCET fault code");
}

void test_busy_wait_us_does_not_trigger(void) {
    sim_scheduler_reset(42);

    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_task_create(
        busy_wait_task, "bw", 32*1024, NULL, 5, PAL_OS_CORE_ANY, &h));

    (void)pal_sim_scheduler_run(&s_test_callbacks, SIM_SCHED_NO_READY, 100);

    TEST_ASSERT_FALSE_MESSAGE(s_fault_fired,
        "pal_os_busy_wait_us must NOT trigger 8002 (virtual clock only)");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cpu_hog_triggers_8002);
    RUN_TEST(test_busy_wait_us_does_not_trigger);
    return UNITY_END();
}
