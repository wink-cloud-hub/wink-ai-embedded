// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_ultrasonic_sim.c
 * @brief DAL ultrasonic simulation path unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "pal_resource.h"
#include "js_sim_host_stub.h"

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED_BEGIN

static const char *const OWNER = "test_dal_ultrasonic_sim";

extern float dal_pulse_us_to_cm(uint32_t pulse_us);

void setUp(void) { sim_set_echo_pulse_us(0); pal_resource_reset(); }
void tearDown(void) {}

void test_sim_read_uses_shared_conversion(void) {
    sim_set_echo_pulse_us(5882);
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_read(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_EQUAL_FLOAT(dal_pulse_us_to_cm(5882), dist);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 99.994f, dist);
}

void test_sim_read_timeout_when_pulse_exceeds_limit(void) {
    sim_set_echo_pulse_us(61000);
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .use_rmt = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_read(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, s);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sim_read_uses_shared_conversion);
    RUN_TEST(test_sim_read_timeout_when_pulse_exceeds_limit);
    return UNITY_END();
}

WINK_TEST_ALLOW_DEPRECATED_END
