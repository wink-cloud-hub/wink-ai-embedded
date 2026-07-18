/**
 * @file test_bal_telemetry.c
 * @brief Unit tests for wink_telemetry_helper (host, virtual time).
 *
 * Telemetry defaults to the MAY_BLOCK path (dedicated task), which on host
 * creates a fiber.  Those fibers are GC'd inside pal_sim_scheduler_run
 * (wink_runtime_run's main loop); outside the scheduler, repeated start/
 * stop can exhaust the host fiber pool because zombie fibers accumulate.
 * Strategy:
 *   - All default-path (MAY_BLOCK) lifecycle tests run at most 1 start/stop
 *     per test �?setUp/tearDown guarantee a clean slot state between tests.
 *   - The 100-cycle slot-leak regression uses WINK_PERIODIC_LIGHT via
 *     _start_ex.  LIGHT paths don't allocate fibers; the callback body
 *     never fires (soft_timer is not dispatched), so we exercise only the
 *     BAL slot bookkeeping �?which is exactly what that regression guards.
 *
 * Verifies:
 *   - NULL sonar/btn are both valid (runtime-stats-only telemetry).
 *   - is_running() lifecycle.
 *   - Duplicate-start �?WINK_ERR_INVALID_STATE.
 *   - Stop is idempotent (including before any start).
 *   - 100 LIGHT start/stop cycles don't leak BAL slots or periodic handles.
 *   - _start_ex with explicit MAY_BLOCK opts starts successfully.
 */
#define LOG_TAG "tst_telem_helper"

#include "unity.h"
#include "wink_telemetry_helper.h"
#include "wink_bal_opts.h"
#include "wink_soft_timer.h"
#include "wink_tasks.h"
#include "wink_status.h"
#include "wink_trace.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "host_test_ctrl.h"

#include <stdbool.h>
#include <string.h>

#include "dal_ultrasonic.h"
#include "dal_button.h"

/* ── setUp / tearDown ─────────────────────────────────────────── */
void setUp(void) {
    /* Ensure no telemetry from a previous test is left running. */
    wink_telemetry_default_stop();

    WINK_IGNORE_RESULT(wink_soft_timer_init());
    pal_resource_reset();
    sim_reset_time();
    wink_trace_reset();

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, wink_periodic_active_count(),
        "leaked periodic handle from previous test");
}

void tearDown(void) {
    wink_telemetry_default_stop();
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* ── Tests ────────────────────────────────────────────────────── */

void test_not_running_initially(void) {
    TEST_ASSERT_FALSE(wink_telemetry_default_is_running());
}

void test_stop_without_start_is_noop(void) {
    wink_telemetry_default_stop();
    TEST_ASSERT_FALSE(wink_telemetry_default_is_running());
    wink_telemetry_default_stop();
    TEST_ASSERT_FALSE(wink_telemetry_default_is_running());
}

void test_start_with_null_devices_succeeds(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        wink_telemetry_default_start(NULL, NULL));
    TEST_ASSERT_TRUE(wink_telemetry_default_is_running());
}

void test_start_stop_lifecycle(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        wink_telemetry_default_start(NULL, NULL));
    TEST_ASSERT_TRUE(wink_telemetry_default_is_running());

    wink_telemetry_default_stop();
    TEST_ASSERT_FALSE(wink_telemetry_default_is_running());
}

void test_duplicate_start_rejected(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        wink_telemetry_default_start(NULL, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE,
        wink_telemetry_default_start(NULL, NULL));
}

void test_double_stop_is_idempotent(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        wink_telemetry_default_start(NULL, NULL));
    wink_telemetry_default_stop();
    wink_telemetry_default_stop();
    TEST_ASSERT_FALSE(wink_telemetry_default_is_running());
}

/* REGRESSION for BAL slot/handle leak �?100 LIGHT start/stop cycles.
 * LIGHT path avoids host fiber allocation, letting us prove slot
 * recycling in the helper itself without depending on scheduler GC. */
void test_start_stop_loop_100_light_does_not_leak(void) {
    wink_bal_opts_t opts = WINK_BAL_OPTS_DEFAULT;
    opts.flags = WINK_PERIODIC_LIGHT;

    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK,
            wink_telemetry_default_start_ex(NULL, NULL, &opts));
        TEST_ASSERT_TRUE(wink_telemetry_default_is_running());
        wink_telemetry_default_stop();
        TEST_ASSERT_FALSE(wink_telemetry_default_is_running());
    }
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* _start_ex with explicit MAY_BLOCK opts (one start per test �?host
 * fiber is GC'd by tearDown via the scheduler, see file-level note). */
void test_start_ex_mayblock_explicit_opts_succeeds(void) {
    wink_bal_opts_t opts = WINK_BAL_OPTS_DEFAULT;
    opts.stack_bytes = 2048u;
    opts.priority    = 1;
    opts.core_id     = WINK_BAL_CORE_ANY;
    opts.flags       = WINK_PERIODIC_MAY_BLOCK;

    TEST_ASSERT_EQUAL_INT(WINK_OK,
        wink_telemetry_default_start_ex(NULL, NULL, &opts));
    TEST_ASSERT_TRUE(wink_telemetry_default_is_running());
}

/* Preflight: passing a non-NULL but un-inited sonar or button must fail
 * with NOT_INITIALIZED and must NOT arm a periodic (anti-"blinking in
 * void"). NULL-NULL is already valid and is covered above. */
void test_start_with_uninitialized_sonar_rejected(void) {
    dal_ultrasonic_t uninit_sonar;
    memset(&uninit_sonar, 0, sizeof(uninit_sonar));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
        wink_telemetry_default_start(&uninit_sonar, NULL));
    TEST_ASSERT_FALSE(wink_telemetry_default_is_running());
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

void test_start_with_uninitialized_button_rejected(void) {
    dal_button_t uninit_btn;
    memset(&uninit_btn, 0, sizeof(uninit_btn));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
        wink_telemetry_default_start(NULL, &uninit_btn));
    TEST_ASSERT_FALSE(wink_telemetry_default_is_running());
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* ── Runner ───────────────────────────────────────────────────── */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_not_running_initially);
    RUN_TEST(test_stop_without_start_is_noop);
    RUN_TEST(test_start_with_null_devices_succeeds);
    RUN_TEST(test_start_stop_lifecycle);
    RUN_TEST(test_duplicate_start_rejected);
    RUN_TEST(test_double_stop_is_idempotent);
    RUN_TEST(test_start_stop_loop_100_light_does_not_leak);
    RUN_TEST(test_start_ex_mayblock_explicit_opts_succeeds);
    RUN_TEST(test_start_with_uninitialized_sonar_rejected);
    RUN_TEST(test_start_with_uninitialized_button_rejected);
    return UNITY_END();
}
