/**
 * @file test_blink_helper.c
 * @brief Unit tests for BAL wink_led_blink (host, virtual time).
 *
 * Verifies (Task 2.1, ADR-0023 Stage 2):
 *   - Argument contract (NULL led / period_ms=0).
 *   - stop() on invalid / zero handle is no-op, idempotent.
 *   - End-to-end toggle via soft_timer dispatch (LIGHT path).
 *   - Stop() halts further toggles.
 *   - Multiple concurrent LEDs up to pool size.
 *   - REGRESSION (LIFO bug): start/stop 100 cycles on the same LED must NOT
 *     return WINK_ERR_RESOURCE_EXHAUSTED �?slots must be recycled on stop.
 *   - No leaked wink_periodic handles after stop (active_count returns to 0).
 */
#define LOG_TAG "tst_blink"

#include "unity.h"
#include "output/wink_led_blink.h"
#include "wink_tasks.h"
#include "wink_status.h"
#include "wink_soft_timer.h"
#include "dal_led.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "host_test_ctrl.h"

#include <string.h>

/* ── Test state ──────────────────────────────────────────────── */
static dal_led_t s_led1;
static dal_led_t s_led2;
static const uint16_t LED1_PIN = 10;
static const uint16_t LED2_PIN = 11;

/* Host sim clock advance (declared in targets/host/pal_osal_host.c). */
extern void host_sim_advance_to(uint64_t us);

/* Advance virtual time by one 10 ms runtime tick and dispatch soft_timers.
 * blink helper uses the LIGHT (soft_timer) path, so manual dispatch drives
 * the toggles deterministically.  soft_timer compares against pal_os_get_us(),
 * so we must advance the sim clock as well as call dispatch. */
static void tick_once(void) {
    uint64_t now = pal_os_get_us();
    host_sim_advance_to(now + 10000u);   /* +10 ms */
    wink_soft_timer_dispatch();
}

static void tick_n(int n) {
    for (int i = 0; i < n; i++) { tick_once(); }
}

static void init_led(dal_led_t *led, uint16_t pin) {
    memset(led, 0, sizeof(*led));
    const dal_led_config_t cfg = {
        .owner = "test_blink",
        .pin = pin,
        .active_high = true,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(led, &cfg));
}

/* ── setUp / tearDown ────────────────────────────────────────── */
void setUp(void) {
    /* Stop any leftover blink first (in case prior test crashed mid-way). */
    wink_led_blink_stop((int32_t)WINK_PERIODIC_INVALID);
    pal_resource_reset();
    sim_clear_gpio_ideal();
    sim_reset_time();
    WINK_IGNORE_RESULT(wink_soft_timer_init());

    init_led(&s_led1, LED1_PIN);
    init_led(&s_led2, LED2_PIN);
}

void tearDown(void) {
    /* Tests are responsible for stopping their own handles; setUp's
     * WINK_PERIODIC_INVALID stop is a belt-and-suspenders safety net. */
    WINK_IGNORE_RESULT(dal_led_deinit(&s_led1));
    WINK_IGNORE_RESULT(dal_led_deinit(&s_led2));
    sim_clear_gpio_ideal();
}

/* ── Tests ───────────────────────────────────────────────────── */

/* Contract: NULL led or zero period_ms rejected with INVALID_ARG. */
void test_null_args_rejected(void) {
    int32_t h = wink_led_blink_start(NULL, 100);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, (wink_status_t)h);
    h = wink_led_blink_start(&s_led1, 0u);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, (wink_status_t)h);
}

/* stop() on INVALID (0) and negative (error passthrough) handles is a no-op
 * �?must not crash, must not corrupt state. */
void test_stop_invalid_handle_is_noop(void) {
    wink_led_blink_stop((int32_t)WINK_PERIODIC_INVALID);
    wink_led_blink_stop(-1);
    wink_led_blink_stop(-999);
    /* no crash = PASS */
}

/* Stop on a freshly-started handle: idempotent (double stop is safe). */
void test_double_stop_is_idempotent(void) {
    int32_t h = wink_led_blink_start(&s_led1, 100);
    TEST_ASSERT(h >= 1);
    wink_led_blink_stop(h);
    wink_led_blink_stop(h);   /* second stop on recycled slot must no-op */
    /* no crash = PASS */
}

/* End-to-end: blink at 20 ms period �?half-period 10 ms = one tick per toggle.
 * After 1 toggle tick, LED should be off; after 2, on again.
 * We check is_on via the public dal_led_t field (matches test_dal_led pattern). */
void test_blink_toggles_led_each_half_period(void) {
    /* 20 ms full period = 10 ms half period = 1 tick per toggle. */
    int32_t h = wink_led_blink_start(&s_led1, 20);
    TEST_ASSERT(h >= 1);

    /* Start() turns LED on per the documented "start with LED on" contract. */
    TEST_ASSERT_TRUE(s_led1.is_on);

    tick_once(); /* first half-period �?off */
    TEST_ASSERT_FALSE(s_led1.is_on);

    tick_once(); /* second half-period �?on */
    TEST_ASSERT_TRUE(s_led1.is_on);

    tick_once(); /* third �?off */
    TEST_ASSERT_FALSE(s_led1.is_on);

    wink_led_blink_stop(h);
}

/* After stop(), dispatching more ticks must NOT toggle the LED further. */
void test_stop_halts_toggles(void) {
    int32_t h = wink_led_blink_start(&s_led1, 20);
    TEST_ASSERT(h >= 1);
    tick_once(); /* now off */
    TEST_ASSERT_FALSE(s_led1.is_on);

    wink_led_blink_stop(h);

    /* Drive many more ticks �?LED must stay at its last level (off). */
    tick_n(20);
    TEST_ASSERT_FALSE(s_led1.is_on);
}

/* Two concurrent LEDs on different pins must toggle independently. */
void test_two_concurrent_leds(void) {
    int32_t h1 = wink_led_blink_start(&s_led1, 20);
    int32_t h2 = wink_led_blink_start(&s_led2, 40); /* 20 ms half = every 2 ticks */
    TEST_ASSERT(h1 >= 1);
    TEST_ASSERT(h2 >= 1);
    TEST_ASSERT(h1 != h2);

    /* Both start ON. */
    TEST_ASSERT_TRUE(s_led1.is_on);
    TEST_ASSERT_TRUE(s_led2.is_on);

    tick_once();
    /* led1 toggled (20ms period �?1 tick), led2 NOT yet (40ms �?needs 2 ticks). */
    TEST_ASSERT_FALSE(s_led1.is_on);
    TEST_ASSERT_TRUE(s_led2.is_on);

    tick_once();
    /* led1 toggle again; led2 first toggle. */
    TEST_ASSERT_TRUE(s_led1.is_on);
    TEST_ASSERT_FALSE(s_led2.is_on);

    wink_led_blink_stop(h1);
    wink_led_blink_stop(h2);
}

/* REGRESSION for the LIFO/s_next bug: cycling start/stop 100 times must keep
 * succeeding.  The old buggy code used a monotonically-incremented s_next
 * cursor that was only decremented on soft_timer_create failure �?after 4
 * successful start/stop cycles the pool was permanently exhausted. */
void test_start_stop_loop_100_does_not_exhaust(void) {
    for (int i = 0; i < 100; i++) {
        int32_t h = wink_led_blink_start(&s_led1, 20);
        TEST_ASSERT_GREATER_OR_EQUAL_INT32_MESSAGE(1, h,
            "blink slot leaked �?start/stop cycle exhausted pool");
        tick_once(); /* drive one toggle just to exercise the callback */
        wink_led_blink_stop(h);
    }
    /* After all cycles, no leaked periodic handles. */
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* Sanity: starting 4 concurrent blinks (default pool size) succeeds, then
 * starting a 5th returns RESOURCE_EXHAUSTED, and stopping one frees a slot
 * so another can be started. */
void test_pool_exhaustion_and_reclaim(void) {
    /* s_led1 + s_led2 + two fake LEDs on different pins to fill pool. */
    dal_led_t led3;
    dal_led_t led4;
    dal_led_t led5;
    init_led(&led3, 12);
    init_led(&led4, 13);
    init_led(&led5, 14);

    int32_t h1 = wink_led_blink_start(&s_led1, 20);
    int32_t h2 = wink_led_blink_start(&s_led2, 20);
    int32_t h3 = wink_led_blink_start(&led3, 20);
    int32_t h4 = wink_led_blink_start(&led4, 20);
    TEST_ASSERT(h1 >= 1);
    TEST_ASSERT(h2 >= 1);
    TEST_ASSERT(h3 >= 1);
    TEST_ASSERT(h4 >= 1);

    int32_t h5 = wink_led_blink_start(&led5, 20);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, (wink_status_t)h5);

    /* Stop one; then starting again should succeed (slot reclaimed). */
    wink_led_blink_stop(h2);
    int32_t h5b = wink_led_blink_start(&led5, 20);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32_MESSAGE(1, h5b,
        "slot should be reclaimable after stop");

    wink_led_blink_stop(h1);
    wink_led_blink_stop(h3);
    wink_led_blink_stop(h4);
    wink_led_blink_stop(h5b);

    WINK_IGNORE_RESULT(dal_led_deinit(&led3));
    WINK_IGNORE_RESULT(dal_led_deinit(&led4));
    WINK_IGNORE_RESULT(dal_led_deinit(&led5));

    /* Final leak check. */
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* ── Runner ──────────────────────────────────────────────────── */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_null_args_rejected);
    RUN_TEST(test_stop_invalid_handle_is_noop);
    RUN_TEST(test_double_stop_is_idempotent);
    RUN_TEST(test_blink_toggles_led_each_half_period);
    RUN_TEST(test_stop_halts_toggles);
    RUN_TEST(test_two_concurrent_leds);
    RUN_TEST(test_start_stop_loop_100_does_not_exhaust);
    RUN_TEST(test_pool_exhaustion_and_reclaim);
    return UNITY_END();
}
