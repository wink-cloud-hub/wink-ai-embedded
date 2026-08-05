// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_runtime.c
 * @brief Wink cooperative runtime scheduler unit tests.
 */
#include "unity.h"
#include "wink_runtime.h"
#include "wink_tasks.h"
#include "wink_trace.h"
#include "wink_actuator_registry.h"
#include "pal_osal.h"
#include "host_test_ctrl.h"

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

static int s_init_calls = 0;
static int s_loop_calls = 0;
static int s_fault_calls = 0;
static uint32_t s_last_fault = 0;

static int s_safe_off_calls = 0;
static int s_safe_off_calls_at_fault = -1;

static void mock_init(void) { s_init_calls++; }
static void mock_loop(void) {
    s_loop_calls++;
    if (s_loop_calls == 3) {
        wink_trace_fault(7001);
    }
}
static void mock_on_fault(uint32_t code) {
    s_fault_calls++; s_last_fault = code;
    s_safe_off_calls_at_fault = s_safe_off_calls;
}
static wink_status_t mock_actuator_off(void *ctx) { (void)ctx; s_safe_off_calls++; return WINK_OK; }

void setUp(void) {
    s_init_calls = s_loop_calls = s_fault_calls = 0;
    s_last_fault = 0;
    s_safe_off_calls = 0;
    s_safe_off_calls_at_fault = -1;
    wink_trace_reset();
    wink_actuator_registry_reset();
    sim_reset_time();
}
void tearDown(void) {}

void test_run_calls_init_once_then_loops_n_times(void) {
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, 5);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_INT(1, s_init_calls);
    TEST_ASSERT_EQUAL_INT(5, s_loop_calls);
}

void test_run_null_callbacks_returns_invalid_arg(void) {
    wink_status_t s = wink_runtime_run(NULL, 5);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, s);
}

void test_null_init_callback_treated_as_ok(void) {
    wink_app_callbacks_t cb = { NULL, NULL, NULL };
    wink_status_t s = wink_runtime_run(&cb, 3);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
}

void test_fault_path_safe_off_before_on_fault(void) {
    wink_app_callbacks_t cb = { NULL, NULL, mock_on_fault };
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_actuator_off, NULL));
    wink_runtime_fault(&cb, 7500);
    TEST_ASSERT_EQUAL_INT(1, s_safe_off_calls);
    TEST_ASSERT_EQUAL_INT(1, s_safe_off_calls_at_fault);
    TEST_ASSERT_EQUAL_INT(1, s_fault_calls);
    TEST_ASSERT_EQUAL_UINT32(7500, s_last_fault);
}

void test_boot_safe_lock_after_threshold_consecutive_abnormal(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_actuator_off, NULL));
    pal_os_set_abnormal_boot_count(WINK_BOOT_LOCK_THRESHOLD - 1);
    sim_set_reset_reason(PAL_OS_RESET_REASON_WATCHDOG);
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_LOCKED, s);
    TEST_ASSERT_EQUAL_INT(0, s_init_calls);
    TEST_ASSERT_EQUAL_INT(1, s_safe_off_calls);
    TEST_ASSERT_EQUAL_UINT32(WINK_FAULT_BOOT_AFTER_RESET, wink_trace_last());
    TEST_ASSERT_EQUAL_UINT32(1, wink_trace_count());
}

void test_boot_single_watchdog_recovers(void) {
    pal_os_set_abnormal_boot_count(0);
    sim_set_reset_reason(PAL_OS_RESET_REASON_WATCHDOG);
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, 5);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_INT(1, s_init_calls);
    TEST_ASSERT_EQUAL_INT(0, s_safe_off_calls);
    TEST_ASSERT_EQUAL_UINT32(1, pal_os_get_abnormal_boot_count());
}

void test_boot_count_clears_after_healthy_milestone(void) {
    pal_os_set_abnormal_boot_count(1);
    sim_set_reset_reason(PAL_OS_RESET_REASON_WATCHDOG);
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, WINK_BOOT_HEALTHY_TICKS + 5);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_UINT32(0, pal_os_get_abnormal_boot_count());
}

void test_boot_no_safe_lock_on_power_on_reset(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_actuator_register(mock_actuator_off, NULL));
    sim_set_reset_reason(PAL_OS_RESET_REASON_POWER_ON);
    wink_app_callbacks_t cb = { mock_init, mock_loop, mock_on_fault };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_INT(0, s_safe_off_calls);
}

static void mock_loop_wcet_exceeded(void) {
    pal_os_busy_wait_us(6000);
}

void test_wcet_exceeded_logs_warning_in_trace(void) {
    wink_app_callbacks_t cb = { NULL, mock_loop_wcet_exceeded, NULL };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_UINT32(0u, wink_trace_last());
    TEST_ASSERT_EQUAL_UINT32(0u, wink_trace_count());
    TEST_ASSERT_TRUE(wink_warn_count() >= 1u);
}

static void mock_loop_wcet_normal(void) {
    pal_os_busy_wait_us(2000);
}

void test_wcet_normal_does_not_log_warning_in_trace(void) {
    wink_app_callbacks_t cb = { NULL, mock_loop_wcet_normal, NULL };
    wink_status_t s = wink_runtime_run(&cb, 1);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_UINT32(0u, wink_trace_last());
    TEST_ASSERT_EQUAL_UINT32(0u, wink_warn_count());
}

static volatile uint32_t s_periodic_light_calls = 0;
static wink_periodic_handle_t s_periodic_light_h = WINK_PERIODIC_INVALID;

static void periodic_light_cb_test(void *ctx) {
    (void)ctx;
    s_periodic_light_calls++;
}

static wink_status_t init_start_periodic_light(void) {
    wink_periodic_handle_t h = wink_periodic_start(
        "unit_light", 0u, 5u, periodic_light_cb_test, NULL, WINK_PERIODIC_LIGHT);
    s_periodic_light_h = h;
    if (h <= 0) {
        return (wink_status_t)h;
    }
    return WINK_OK;
}

void test_periodic_start_stop_light(void) {
    s_periodic_light_calls = 0;
    s_periodic_light_h = WINK_PERIODIC_INVALID;

    wink_app_callbacks_t cb;
    memset(&cb, 0, sizeof(cb));
    cb.init_status = init_start_periodic_light;

    wink_status_t s = wink_runtime_run(&cb, 20u);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_TRUE(s_periodic_light_h > 0);
    TEST_ASSERT_TRUE_MESSAGE(s_periodic_light_calls > 0u,
        "LIGHT periodic callback never fired during runtime tick loop");

    uint32_t before_stop = s_periodic_light_calls;
    wink_periodic_stop(s_periodic_light_h);

    wink_app_callbacks_t cb_noinit;
    memset(&cb_noinit, 0, sizeof(cb_noinit));
    wink_status_t s2 = wink_runtime_run(&cb_noinit, 10u);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s2);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(before_stop, s_periodic_light_calls,
        "LIGHT periodic callback still firing after wink_periodic_stop");
}

#if defined(ESP_PLATFORM)
static volatile uint32_t s_periodic_blk_calls = 0;
static wink_periodic_handle_t s_periodic_blk_h = 0;

static void periodic_blk_cb_test(void *ctx) {
    (void)ctx;
    s_periodic_blk_calls++;
}

static wink_status_t init_start_periodic_blk(void) {
    wink_periodic_handle_t h = wink_periodic_start(
        "unit_blk", 2048u, 5u, periodic_blk_cb_test, NULL, WINK_PERIODIC_MAY_BLOCK);
    s_periodic_blk_h = h;
    if (h <= 0) {
        return (wink_status_t)h;
    }
    return WINK_OK;
}

void test_periodic_start_stop_may_block(void) {
    s_periodic_blk_calls = 0;
    s_periodic_blk_h = 0;

    wink_app_callbacks_t cb;
    memset(&cb, 0, sizeof(cb));
    cb.init_status = init_start_periodic_blk;

    wink_status_t s = wink_runtime_run(&cb, 20u);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_TRUE(s_periodic_blk_h > 0);
    TEST_ASSERT_TRUE_MESSAGE(s_periodic_blk_calls > 0u,
        "MAY_BLOCK periodic callback never fired");

    wink_periodic_stop(s_periodic_blk_h);
    uint32_t after_stop = s_periodic_blk_calls;

    wink_app_callbacks_t cb_noinit;
    memset(&cb_noinit, 0, sizeof(cb_noinit));
    wink_status_t s3 = wink_runtime_run(&cb_noinit, 10u);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s3);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(after_stop, s_periodic_blk_calls,
        "MAY_BLOCK periodic callback still firing after wink_periodic_stop");
}
#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_run_calls_init_once_then_loops_n_times);
    RUN_TEST(test_run_null_callbacks_returns_invalid_arg);
    RUN_TEST(test_null_init_callback_treated_as_ok);
    RUN_TEST(test_fault_path_safe_off_before_on_fault);
    RUN_TEST(test_boot_safe_lock_after_threshold_consecutive_abnormal);
    RUN_TEST(test_boot_single_watchdog_recovers);
    RUN_TEST(test_boot_count_clears_after_healthy_milestone);
    RUN_TEST(test_boot_no_safe_lock_on_power_on_reset);
    RUN_TEST(test_wcet_exceeded_logs_warning_in_trace);
    RUN_TEST(test_wcet_normal_does_not_log_warning_in_trace);
    RUN_TEST(test_periodic_start_stop_light);
#if defined(ESP_PLATFORM)
    RUN_TEST(test_periodic_start_stop_may_block);
#endif
    return UNITY_END();
}
