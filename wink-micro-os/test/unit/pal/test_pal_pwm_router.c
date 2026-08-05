// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_pwm_router.c
 * @brief PAL PWM router timer allocation and sharing unit tests.
 */
#include "unity.h"
#include "pal_pwm_router.h"
#include "pal_resource.h"

void setUp(void) {
    pal_pwm_router_reset();
    pal_resource_reset();
}
void tearDown(void) {}

static uint8_t acquire_ok(uint8_t ch, uint32_t freq) {
    uint8_t t = 0xFF;
    pal_pwm_timer_profile_t prof = pal_pwm_timer_profile_default(freq);
    TEST_ASSERT_EQUAL_INT_MESSAGE(WINK_OK, pal_pwm_router_acquire(ch, &prof, &t), "acquire should succeed");
    return t;
}

static wink_status_t acquire_profile(uint8_t ch, const pal_pwm_timer_profile_t *prof, uint8_t *t) {
    return pal_pwm_router_acquire(ch, prof, t);
}

void test_router_acquire_release_basic(void) {
    uint8_t t = acquire_ok(0, 1000);
    TEST_ASSERT_TRUE(t < PAL_PWM_TIMERS);
    TEST_ASSERT_TRUE(pal_pwm_router_channel_ready(0));
    TEST_ASSERT_EQUAL_UINT8(t, pal_pwm_router_channel_timer(0));

    pal_pwm_router_release(0);
    TEST_ASSERT_FALSE(pal_pwm_router_channel_ready(0));
    TEST_ASSERT_EQUAL_UINT8(0xFF, pal_pwm_router_channel_timer(0));
}

void test_router_same_freq_reuses_timer(void) {
    uint8_t t0 = acquire_ok(0, 50);
    uint8_t t1 = acquire_ok(1, 50);
    TEST_ASSERT_EQUAL_UINT8(t0, t1);
}

void test_router_same_freq_diff_bits_no_share(void) {
    pal_pwm_timer_profile_t p13 = pal_pwm_timer_profile_default(50);
    pal_pwm_timer_profile_t p10 = pal_pwm_timer_profile_default(50);
    p10.resolution_bits = 10u;

    uint8_t t0 = 0xFF;
    uint8_t t1 = 0xFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, acquire_profile(0, &p13, &t0));
    TEST_ASSERT_EQUAL_INT(WINK_OK, acquire_profile(1, &p10, &t1));
    TEST_ASSERT_NOT_EQUAL(t0, t1);
}

void test_router_diff_freq_diff_timer(void) {
    uint8_t t0 = acquire_ok(0, 50);
    uint8_t t1 = acquire_ok(1, 1000);
    TEST_ASSERT_NOT_EQUAL(t0, t1);
}

void test_router_idempotent_and_busy(void) {
    uint8_t t = acquire_ok(0, 50);
    uint8_t t2 = 0xFF;
    pal_pwm_timer_profile_t prof50 = pal_pwm_timer_profile_default(50);
    pal_pwm_timer_profile_t prof1k = pal_pwm_timer_profile_default(1000);

    TEST_ASSERT_EQUAL_INT(WINK_OK, acquire_profile(0, &prof50, &t2));
    TEST_ASSERT_EQUAL_UINT8(t, t2);

    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, acquire_profile(0, &prof1k, &t2));
    TEST_ASSERT_EQUAL_INT(WINK_OK, acquire_profile(0, &prof50, &t2));
    TEST_ASSERT_EQUAL_UINT8(t, t2);
}

void test_router_exhausted_after_four_distinct_freqs(void) {
    uint8_t t;
    uint32_t freqs[PAL_PWM_TIMERS] = {50, 200, 1000, 5000};
    for (uint8_t i = 0; i < PAL_PWM_TIMERS; i++) {
        pal_pwm_timer_profile_t prof = pal_pwm_timer_profile_default(freqs[i]);
        TEST_ASSERT_EQUAL_INT(WINK_OK, acquire_profile(i, &prof, &t));
    }
    pal_pwm_timer_profile_t prof25k = pal_pwm_timer_profile_default(25000);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_RESOURCE_EXHAUSTED, acquire_profile(4, &prof25k, &t));

    pal_pwm_timer_profile_t prof50 = pal_pwm_timer_profile_default(50);
    TEST_ASSERT_EQUAL_INT(WINK_OK, acquire_profile(4, &prof50, &t));
}

void test_router_release_recycles_timer(void) {
    uint8_t ta = acquire_ok(0, 50);
    (void)acquire_ok(1, 50);
    pal_pwm_router_release(0);
    pal_pwm_router_release(1);
    pal_pwm_timer_profile_t prof50 = pal_pwm_timer_profile_default(50);
    TEST_ASSERT_EQUAL_INT(WINK_OK, acquire_profile(2, &prof50, &ta));
}

void test_router_invalid_args(void) {
    uint8_t t;
    pal_pwm_timer_profile_t prof = pal_pwm_timer_profile_default(50);

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, acquire_profile(PAL_PWM_CHANNELS, &prof, &t));

    prof.freq_hz = 0u;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, acquire_profile(0, &prof, &t));

    prof = pal_pwm_timer_profile_default(50);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, acquire_profile(0, &prof, NULL));

    prof.resolution_bits = 0u;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, acquire_profile(0, &prof, &t));

    pal_pwm_router_release(PAL_PWM_CHANNELS);
    pal_pwm_router_release(0);
    TEST_PASS();
}

void test_pwm_percent_to_raw(void) {
    TEST_ASSERT_EQUAL_UINT32(8191u, pal_pwm_percent_to_raw(100.0f, 13u));
    TEST_ASSERT_EQUAL_UINT32(0u, pal_pwm_percent_to_raw(0.0f, 13u));
    TEST_ASSERT_EQUAL_UINT32(511u, pal_pwm_percent_to_raw(50.0f, 10u));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_router_acquire_release_basic);
    RUN_TEST(test_router_same_freq_reuses_timer);
    RUN_TEST(test_router_same_freq_diff_bits_no_share);
    RUN_TEST(test_router_diff_freq_diff_timer);
    RUN_TEST(test_router_idempotent_and_busy);
    RUN_TEST(test_router_exhausted_after_four_distinct_freqs);
    RUN_TEST(test_router_release_recycles_timer);
    RUN_TEST(test_router_invalid_args);
    RUN_TEST(test_pwm_percent_to_raw);
    return UNITY_END();
}
