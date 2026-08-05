// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_pwm_config.c
 * @brief PAL PWM configuration and profile unit tests.
 */
#include "unity.h"
#include "pal_hal.h"
#include "pal_pwm_router.h"
#include "host_test_ctrl.h"

void setUp(void) {
    pal_pwm_router_reset();
}

void tearDown(void) {}

void test_pwm_init_ex_default_profile(void) {
    pal_pwm_config_t cfg = { .freq_hz = 50u };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init_ex(0, &cfg));
    TEST_ASSERT_TRUE(pal_pwm_router_channel_ready(0));
    pal_pwm_deinit(0);
}

void test_pwm_init_ex_stable_required_unsupported_on_host(void) {
    pal_pwm_config_t cfg = {
        .freq_hz = 50u,
        .clock_requirement = PAL_PWM_CLOCK_STABLE_REQUIRED,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, pal_pwm_init_ex(0, &cfg));
    TEST_ASSERT_FALSE(pal_pwm_router_channel_ready(0));
}

void test_pwm_init_legacy_wraps_init_ex(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(1, 1000u));
    TEST_ASSERT_TRUE(pal_pwm_router_channel_ready(1));
    pal_pwm_deinit(1);
}

void test_pwm_set_duty_percent_on_host(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_init(2, 50u));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_pwm_set_duty(2, 37.5f));
    TEST_ASSERT_EQUAL_FLOAT(37.5f, sim_last_pwm_duty(2));
    pal_pwm_deinit(2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pwm_init_ex_default_profile);
    RUN_TEST(test_pwm_init_ex_stable_required_unsupported_on_host);
    RUN_TEST(test_pwm_init_legacy_wraps_init_ex);
    RUN_TEST(test_pwm_set_duty_percent_on_host);
    return UNITY_END();
}
