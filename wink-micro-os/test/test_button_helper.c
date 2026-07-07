/**
 * @file test_button_helper.c
 * @brief Unit tests for wink_button_helper (host, virtual time).
 *
 * Verifies:
 *   - Argument contract (NULL / poll_ms=0).
 *   - Idempotent stop.
 *   - Duplicate-start rejection.
 *   - End-to-end PRESS/RELEASE via soft_timer dispatch (no manual poll).
 *   - Long-press event dispatched via soft_timer.
 *   - Stop() halts further events.
 *   - poll_ms period is honoured (event does not fire until enough dispatch
 *     ticks pass * period).
 */
#define LOG_TAG "tst_btn_helper"

#include "unity.h"
#include "wink_button_helper.h"
#include "wink_soft_timer.h"
#include "wink_tasks.h"
#include "wink_status.h"
#include "dal_button.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_log.h"
#include "host_test_ctrl.h"

#include <string.h>

/* Host sim clock advance (declared in targets/host/pal_osal_host.c). */
extern void host_sim_advance_to(uint64_t us);

/* ── Test state ───────────────────────────────────────────────── */
static dal_button_t s_btn;
static volatile int s_press_count;
static volatile int s_release_count;
static volatile int s_long_count;

static const uint16_t BTN_PIN = 0;

static void evt_cb(dal_button_event_t evt, void *ctx) {
    (void)ctx;
    switch (evt) {
        case DAL_BUTTON_EVT_PRESS:      s_press_count++;   break;
        case DAL_BUTTON_EVT_RELEASE:    s_release_count++; break;
        case DAL_BUTTON_EVT_LONG_PRESS: s_long_count++;    break;
        default: break;
    }
}

/* Advance virtual time by one 10 ms runtime tick and dispatch soft_timers.
 * Long-press detection reads pal_os_get_ms() from the sim clock, so both
 * time advance and dispatch are necessary each tick. */
static void tick_once(void) {
    uint64_t now = pal_os_get_us();
    host_sim_advance_to(now + 10000u);   /* +10 ms */
    wink_soft_timer_dispatch();
}

static void tick_n(int n) {
    for (int i = 0; i < n; i++) { tick_once(); }
}

static void press_button(void)   { sim_set_gpio_ideal(BTN_PIN, false); } /* active_low LOW=press */
static void release_button(void) { sim_set_gpio_ideal(BTN_PIN, true);  } /* active_low HIGH=idle */

/* ── setUp / tearDown ─────────────────────────────────────────── */
void setUp(void) {
    /* Test isolation: drop any lingering helper slot before re-init soft_timer. */
    WINK_IGNORE_RESULT(wink_button_helper_stop(&s_btn));

    /* dal_button_init claims a pin via pal_resource; must reset between tests. */
    pal_resource_reset();
    sim_clear_gpio_ideal();
    sim_reset_time();

    /* Fresh soft_timer pool for each test. */
    WINK_IGNORE_RESULT(wink_soft_timer_init());

    s_press_count = 0;
    s_release_count = 0;
    s_long_count = 0;

    memset(&s_btn, 0, sizeof(s_btn));
    /* Register released level BEFORE init so first poll starts on stable HIGH
     * (sim_set_gpio_ideal §2.3 red line 6: first call = power-on, no bounce). */
    release_button();

    const dal_button_config_t cfg = {
        .owner = "test_btn_helper",
        .pin = BTN_PIN,
        .active_low = true,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&s_btn, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_on_event(&s_btn, evt_cb, NULL));
}

void tearDown(void) {
    WINK_IGNORE_RESULT(wink_button_helper_stop(&s_btn));
    WINK_IGNORE_RESULT(dal_button_deinit(&s_btn));
    sim_clear_gpio_ideal();
}

/* ── Tests ────────────────────────────────────────────────────── */

/* Contract: NULL btn, zero poll_ms rejected; NULL btn on stop is no-op. */
void test_null_args_rejected(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        wink_button_helper_start(NULL, 10u));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
        wink_button_helper_start(&s_btn, 0u));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_stop(NULL));
}

/* stop() on a button that was never started is a no-op returning WINK_OK. */
void test_not_started_stop_is_noop(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_stop(&s_btn));
    /* And stop again is still a no-op. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_stop(&s_btn));
}

/* Double-start on the same button → WINK_ERR_INVALID_STATE. */
void test_duplicate_start_rejected(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_start(&s_btn, 10u));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE,
        wink_button_helper_start(&s_btn, 10u));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_stop(&s_btn));
    /* After stop, start again should succeed. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_start(&s_btn, 10u));
}

/* PRESS then RELEASE flow driven ONLY by soft_timer dispatch (no manual poll). */
void test_press_release_event_via_helper(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_start(&s_btn, 10u));

    /* Drive a couple of ticks in the released state to settle debounce. */
    tick_n(DAL_BUTTON_DEBOUNCE_THRESHOLD + 2);
    TEST_ASSERT_EQUAL_INT(0, s_press_count);
    TEST_ASSERT_EQUAL_INT(0, s_release_count);

    /* Press: raw goes LOW → debounce completes after DEBOUNCE_THRESHOLD ticks. */
    press_button();
    tick_n(DAL_BUTTON_DEBOUNCE_THRESHOLD + 2);
    TEST_ASSERT_EQUAL_INT(1, s_press_count);
    TEST_ASSERT_EQUAL_INT(0, s_release_count);

    /* Release: raw back to HIGH → RELEASE after debounce. */
    release_button();
    tick_n(DAL_BUTTON_DEBOUNCE_THRESHOLD + 2);
    TEST_ASSERT_EQUAL_INT(1, s_press_count);
    TEST_ASSERT_EQUAL_INT(1, s_release_count);
    TEST_ASSERT_EQUAL_INT(0, s_long_count);
}

/* LONG_PRESS event dispatched after held longer than long_press_ms. */
void test_long_press_detected(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        dal_button_set_long_press_ms(&s_btn, 100u));   /* 100 ms threshold */
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_start(&s_btn, 10u));

    /* Settle in released state. */
    tick_n(DAL_BUTTON_DEBOUNCE_THRESHOLD + 2);

    press_button();
    /* Hold for ≥ 100 ms (long_press) + debounce headroom.  20 ticks = 200 ms. */
    tick_n(20);
    TEST_ASSERT_EQUAL_INT(1, s_press_count);
    TEST_ASSERT_EQUAL_INT(1, s_long_count);

    release_button();
    tick_n(DAL_BUTTON_DEBOUNCE_THRESHOLD + 2);
    TEST_ASSERT_EQUAL_INT(1, s_release_count);
    /* LONG_PRESS should have fired exactly once (no repeat during hold). */
    TEST_ASSERT_EQUAL_INT(1, s_long_count);
}

/* After stop(), no further events are dispatched from the soft_timer. */
void test_stop_halts_further_events(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_start(&s_btn, 10u));
    tick_n(DAL_BUTTON_DEBOUNCE_THRESHOLD + 2);

    press_button();
    tick_n(DAL_BUTTON_DEBOUNCE_THRESHOLD + 2);
    TEST_ASSERT_EQUAL_INT(1, s_press_count);

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_stop(&s_btn));

    /* After stop, dispatching further ticks must not poll the button any more,
     * so no RELEASE event should fire even though we drove the pin HIGH. */
    release_button();
    tick_n(10);
    TEST_ASSERT_EQUAL_INT(0, s_release_count);
}

/* poll_ms > tick: the soft_timer only fires every N dispatches, so the debounce
 * takes proportionally longer to complete. */
void test_poll_ms_period_honoured(void) {
    /* poll_ms = 50 ms → soft_timer fires once every 5 dispatches. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_start(&s_btn, 50u));

    /* Establish released baseline (need enough dispatches for a few polls). */
    tick_n(30);

    press_button();
    /* 4 dispatches = 40 ms virtual time → poll has fired 0 times (period=5 ticks),
     * so no debounce sample has run.  No PRESS event yet. */
    tick_n(4);
    TEST_ASSERT_EQUAL_INT(0, s_press_count);

    /* Drive enough ticks for at least DEBOUNCE_THRESHOLD polls to fire.
     * 3 polls at 5 ticks/poll = 15 dispatches minimum; add slack. */
    tick_n(20);
    TEST_ASSERT_EQUAL_INT(1, s_press_count);
}

/* REGRESSION for timer-slot leak (old samples/common helper called only
 * wink_soft_timer_stop without destroy, leaking a soft_timer slot per
 * start/stop cycle).  Cycling start/stop 100 times must keep succeeding
 * and must leave zero active periodic handles. */
void test_start_stop_loop_100_does_not_leak(void) {
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK,
            wink_button_helper_start(&s_btn, 10u));
        tick_once();   /* exercise one poll callback */
        TEST_ASSERT_EQUAL_INT(WINK_OK,
            wink_button_helper_stop(&s_btn));
    }
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* Pool saturation: starting WINK_BUTTON_HELPER_MAX concurrent buttons
 * succeeds; a (MAX+1)-th returns RESOURCE_EXHAUSTED; stopping one frees
 * a slot so another can be started. */
void test_pool_exhaustion_and_reclaim(void) {
    /* s_btn is already initialised by setUp on pin BTN_PIN=0.
     * We need 3 more distinct button instances on unique pins. */
    dal_button_t btns[3];
    const uint16_t extra_pins[3] = { 1, 2, 3 };
    for (int i = 0; i < 3; i++) {
        memset(&btns[i], 0, sizeof(btns[i]));
        sim_set_gpio_ideal(extra_pins[i], true);  /* idle HIGH for active_low */
        const dal_button_config_t cfg = {
            .owner = "test_btn_pool",
            .pin = extra_pins[i],
            .active_low = true,
        };
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&btns[i], &cfg));
    }

    /* Start all 4 (s_btn + 3 extra). */
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_start(&s_btn, 10u));
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK,
            wink_button_helper_start(&btns[i], 10u));
    }

    /* 5th button on yet another pin must fail (start returns EXHAUSTED
     * before any poll fires, so we don't need sim_set_gpio_ideal for it —
     * we have only SIM_GPIO_IDEAL_SLOTS=4 ideal slots, and pins 0-3 already
     * occupy them). */
    dal_button_t btn5;
    memset(&btn5, 0, sizeof(btn5));
    const dal_button_config_t cfg5 = {
        .owner = "test_btn_pool", .pin = 4, .active_low = true,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_button_init(&btn5, &cfg5));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED,
        wink_button_helper_start(&btn5, 10u));

    /* Stop one; retry — slot should be reclaimed. */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        wink_button_helper_stop(&btns[1]));
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        wink_button_helper_start(&btn5, 10u));

    /* Tear down. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_stop(&s_btn));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_stop(&btns[0]));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_stop(&btns[2]));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_button_helper_stop(&btn5));
    for (int i = 0; i < 3; i++) {
        WINK_IGNORE_RESULT(dal_button_deinit(&btns[i]));
    }
    WINK_IGNORE_RESULT(dal_button_deinit(&btn5));

    /* Final leak check. */
    TEST_ASSERT_EQUAL_UINT32(0, wink_periodic_active_count());
}

/* ── Runner ───────────────────────────────────────────────────── */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_null_args_rejected);
    RUN_TEST(test_not_started_stop_is_noop);
    RUN_TEST(test_duplicate_start_rejected);
    RUN_TEST(test_press_release_event_via_helper);
    RUN_TEST(test_long_press_detected);
    RUN_TEST(test_stop_halts_further_events);
    RUN_TEST(test_poll_ms_period_honoured);
    RUN_TEST(test_start_stop_loop_100_does_not_leak);
    RUN_TEST(test_pool_exhaustion_and_reclaim);
    return UNITY_END();
}
