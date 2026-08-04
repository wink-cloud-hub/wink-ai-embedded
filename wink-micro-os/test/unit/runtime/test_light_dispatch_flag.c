/**
 * @file test_light_dispatch_flag.c
 * @brief Unit tests for LIGHT dispatch in-flag and WCET enforcement
 *        (Task 1.5 / ADR-0023 §9 three-line defense).
 *
 * Verifies:
 *   - wink_soft_timer_in_light_dispatch() is false outside dispatch,
 *     true while a LIGHT cb is executing, false after return.
 *   - A callback that exceeds the 100µs soft budget produces a
 *     WINK_WARN_LIGHT_OVERBUDGET trace warn.
 *   - wink_soft_timer_set_name() stores diagnostic name without crash.
 *
 * Note: WINK_ASSERT_NONBLOCKING() under WINK_PT_DEBUG calls assert()
 * which aborts the process — this is the desired hard-fault behavior
 * but is not testable in-process with Unity (no death-test support).
 * The flag wiring is verified here; the abort-on-blocking behavior is
 * enforced at compile/run time on real builds and needs no separate
 * unit test.
 *
 * Copyright (c) 2026 Wink-AI.
 */

#include "unity.h"
#include "wink_soft_timer.h"
#include "wink_pt_debug.h"
#include "wink_trace.h"
#include "wink_fault.h"
#include "pal_osal.h"

/* ADR-0017 层 1 例外：本 TU 合法调用 WINK_BLOCKING API. */
#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

/* ── helpers ─────────────────────────────────────────────── */

static int s_good_cb_count = 0;
static bool s_flag_in_good_cb = false;

static wink_status_t good_cb(void *ctx) {
    (void)ctx;
    s_good_cb_count++;
    s_flag_in_good_cb = wink_soft_timer_in_light_dispatch();
    return WINK_OK;
}

static int s_slow_cb_count = 0;
static uint32_t s_slow_cb_busy_us = 0;
static wink_status_t slow_cb(void *ctx) {
    (void)ctx;
    s_slow_cb_count++;
    /* Busy-wait the requested time to trigger WCET thresholds. */
    pal_os_busy_wait_us(s_slow_cb_busy_us);
    return WINK_OK;
}

void setUp(void) {
    s_good_cb_count = 0;
    s_flag_in_good_cb = false;
    s_slow_cb_count = 0;
    s_slow_cb_busy_us = 0;
    wink_trace_reset();
    WINK_IGNORE_RESULT(wink_soft_timer_init());
}

void tearDown(void) {}

/* ── Tests ──────────────────────────────────────────────── */

void test_flag_is_false_outside_dispatch(void) {
    TEST_ASSERT_FALSE(wink_soft_timer_in_light_dispatch());
}

void test_flag_is_true_during_cb_then_false_after(void) {
    int32_t h = wink_soft_timer_create(good_cb, NULL, WINK_TIMER_PERIODIC, 10);
    TEST_ASSERT(h >= 0);
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_soft_timer_start(h));

    /* Tick 11 times so the period-10 timer fires once. */
    for (int i = 0; i < 11; i++) {
        wink_soft_timer_dispatch();
    }
    TEST_ASSERT(s_good_cb_count >= 1);
    TEST_ASSERT_TRUE(s_flag_in_good_cb);
    /* After dispatch returns flag must be cleared. */
    TEST_ASSERT_FALSE(wink_soft_timer_in_light_dispatch());
    WINK_IGNORE_UNUSED(wink_soft_timer_stop(h));
}

void test_cb_under_budget_produces_no_warn(void) {
    /* 10µs busy wait should be well under the 100µs soft budget. */
    s_slow_cb_busy_us = 10;
    int32_t h = wink_soft_timer_create(slow_cb, NULL, WINK_TIMER_PERIODIC, 10);
    TEST_ASSERT(h >= 0);
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_soft_timer_start(h));

    uint32_t warn_before = wink_warn_count();
    for (int i = 0; i < 11; i++) {
        wink_soft_timer_dispatch();
    }
    TEST_ASSERT(s_slow_cb_count >= 1);
    /* No warn generated. */
    TEST_ASSERT_EQUAL_UINT32(warn_before, wink_warn_count());
    WINK_IGNORE_UNUSED(wink_soft_timer_stop(h));
}

void test_cb_over_soft_budget_produces_warn(void) {
    /* 200µs > 100µs soft budget but < 500µs hard limit → WINK_WARN_LIGHT_OVERBUDGET.
     * Use 300µs to safely clear the soft budget on host even with some
     * measurement slack. */
    s_slow_cb_busy_us = 300;
    int32_t h = wink_soft_timer_create(slow_cb, NULL, WINK_TIMER_PERIODIC, 10);
    TEST_ASSERT(h >= 0);
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_soft_timer_start(h));

    uint32_t warn_before = wink_warn_count();
    uint32_t fault_before = wink_trace_count();
    for (int i = 0; i < 11; i++) {
        wink_soft_timer_dispatch();
    }
    TEST_ASSERT(s_slow_cb_count >= 1);
    /* A warn must have been emitted. */
    TEST_ASSERT(wink_warn_count() > warn_before);
    /* But no fault (single overrun under hard limit). */
    TEST_ASSERT_EQUAL_UINT32(fault_before, wink_trace_count());
    WINK_IGNORE_UNUSED(wink_soft_timer_stop(h));
}

void test_set_name_does_not_crash(void) {
    int32_t h = wink_soft_timer_create(good_cb, NULL, WINK_TIMER_PERIODIC, 10);
    TEST_ASSERT(h >= 0);
    wink_soft_timer_set_name(h, "test_good_cb_timer");
    wink_soft_timer_set_name(h, NULL); /* reset */
    WINK_IGNORE_UNUSED(wink_soft_timer_stop(h));
    /* No crash = PASS (name is used for diagnostic logging on fault). */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_flag_is_false_outside_dispatch);
    RUN_TEST(test_flag_is_true_during_cb_then_false_after);
    RUN_TEST(test_cb_under_budget_produces_no_warn);
    RUN_TEST(test_cb_over_soft_budget_produces_warn);
    RUN_TEST(test_set_name_does_not_crash);
    return UNITY_END();
}
