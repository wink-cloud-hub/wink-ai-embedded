/**
 * @file test_ultrasonic_distance_events.c
 * @brief Host unit tests for wink_ultrasonic_enable_distance_events (ADR-0033).
 */
#include "unity.h"
#include "sensor/wink_ultrasonic_distance_events.h"
#include "sensor/wink_ultrasonic_poll.h"
#include "wink_soft_timer.h"
#include "wink_event.h"
#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "pal_resource.h"
#include "pal_osal.h"
#include "host_test_ctrl.h"

#include <string.h>

extern void host_sim_advance_to(uint64_t us);

static dal_ultrasonic_t s_us;
static const uint16_t TRIG = 4;
static const uint16_t ECHO = 5;

static void tick_once(void)
{
    uint64_t now = pal_os_get_us();
    host_sim_advance_to(now + 10000u); /* +10 ms runtime tick */
    wink_soft_timer_dispatch();
}

static void arm_echo(uint32_t high_us)
{
    /* sim_reset_time clears echo pin — re-arm after every reset. */
    sim_set_echo_pin(ECHO);
    sim_set_echo_timing(100, high_us);
}

void setUp(void)
{
    wink_ultrasonic_distance_events_reset();
    WINK_IGNORE_RESULT(wink_ultrasonic_poll_stop(&s_us));
    wink_event_queue_deinit();
    pal_resource_reset();
    sim_reset_time();
    WINK_IGNORE_RESULT(wink_soft_timer_init());
    WINK_IGNORE_RESULT(wink_event_queue_init(16));

    memset(&s_us, 0, sizeof(s_us));
    const dal_ultrasonic_config_t cfg = {
        .owner = "tst_us_dist",
        .trig_pin = TRIG,
        .echo_pin = ECHO,
        .use_rmt = false,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&s_us, &cfg));
}

void tearDown(void)
{
    wink_ultrasonic_disable_distance_events(&s_us);
    wink_ultrasonic_distance_events_reset();
    WINK_IGNORE_RESULT(wink_ultrasonic_poll_stop(&s_us));
    WINK_IGNORE_RESULT(dal_ultrasonic_deinit(&s_us));
    wink_event_queue_deinit();
}

void test_null_and_period_rejected(void)
{
    const wink_ultrasonic_distance_event_config_t ok = { .period_ms = 50u };
    const wink_ultrasonic_distance_event_config_t bad = { .period_ms = 49u };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          wink_ultrasonic_enable_distance_events(NULL, &ok));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          wink_ultrasonic_enable_distance_events(&s_us, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG,
                          wink_ultrasonic_enable_distance_events(&s_us, &bad));
}

void test_duplicate_enable_rejected(void)
{
    const wink_ultrasonic_distance_event_config_t cfg = { .period_ms = 50u };
    arm_echo(5882);
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_enable_distance_events(&s_us, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE,
                          wink_ultrasonic_enable_distance_events(&s_us, &cfg));
}

void test_mutex_with_sonar_helper(void)
{
    const wink_ultrasonic_distance_event_config_t cfg = { .period_ms = 50u };
    arm_echo(5882);
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_start(&s_us, 50u));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE,
                          wink_ultrasonic_enable_distance_events(&s_us, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_poll_stop(&s_us));

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_enable_distance_events(&s_us, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, wink_ultrasonic_poll_start(&s_us, 50u));
}

void test_distance_ready_posted(void)
{
    const wink_ultrasonic_distance_event_config_t cfg = { .period_ms = 50u };
    wink_event_t ev;
    int got = 0;

    arm_echo(588); /* ≈10 cm */

    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_enable_distance_events(&s_us, &cfg));

    /* period 50ms / tick 10ms → need ≥5 dispatches; re-arm echo each tick. */
    for (int i = 0; i < 12 && got == 0; i++) {
        arm_echo(588);
        tick_once();
        if (wink_event_pend(&ev, 0) == WINK_OK) {
            got = 1;
            break;
        }
    }

    TEST_ASSERT_EQUAL_INT(1, got);
    TEST_ASSERT_EQUAL_UINT32(WINK_EVENT_DISTANCE_READY, ev.type);
    TEST_ASSERT_EQUAL_PTR(&s_us, ev.device);
    TEST_ASSERT_TRUE(ev.param >= 80u && ev.param <= 120u);
}

void test_disable_stops_events(void)
{
    const wink_ultrasonic_distance_event_config_t cfg = { .period_ms = 50u };
    wink_event_t ev;

    arm_echo(5882);
    TEST_ASSERT_EQUAL_INT(WINK_OK, wink_ultrasonic_enable_distance_events(&s_us, &cfg));
    for (int i = 0; i < 10; i++) {
        arm_echo(5882);
        tick_once();
    }
    while (wink_event_pend(&ev, 0) == WINK_OK) {
        /* drain */
    }

    wink_ultrasonic_disable_distance_events(&s_us);
    TEST_ASSERT_FALSE(wink_ultrasonic_distance_events_is_enabled(&s_us));

    for (int i = 0; i < 15; i++) {
        arm_echo(5882);
        tick_once();
    }
    TEST_ASSERT_EQUAL_INT(WINK_ERR_EMPTY, wink_event_pend(&ev, 0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_null_and_period_rejected);
    RUN_TEST(test_duplicate_enable_rejected);
    RUN_TEST(test_mutex_with_sonar_helper);
    RUN_TEST(test_distance_ready_posted);
    RUN_TEST(test_disable_stops_events);
    return UNITY_END();
}
