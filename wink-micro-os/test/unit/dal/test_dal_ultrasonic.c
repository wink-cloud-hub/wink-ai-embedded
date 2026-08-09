// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_ultrasonic.c
 * @brief DAL ultrasonic distance sensor driver unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "dal_ultrasonic.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"
#include <time.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4996)
#endif

static const char *const OWNER = "test_dal_ultrasonic";

void setUp(void) { sim_reset_time(); pal_resource_reset(); }
void tearDown(void) {}

void test_ultrasonic_init_null_returns_invalid_arg(void) {
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_init(NULL, NULL));
}

void test_ultrasonic_init_rejects_same_pin(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 5, .echo_pin = 5, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_init(&dev, &cfg));
}

void test_ultrasonic_read_before_init_returns_not_initialized(void) {
    dal_ultrasonic_t dev = { .config.trig_pin = 4, .config.echo_pin = 5, .last_distance = 0.0f };
    float dist = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_ultrasonic_read(&dev, &dist));
}

void test_read_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_read(NULL, (float[]){0}));
}

void test_read_null_out_returns_invalid_arg(void) {
    dal_ultrasonic_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_read(&dev, NULL));
}

extern float dal_pulse_us_to_cm(uint32_t pulse_us);

void test_pulse_to_cm_100cm(void) {
    TEST_ASSERT_EQUAL_FLOAT(99.994f, dal_pulse_us_to_cm(5882));
}

void test_ultrasonic_init_then_read_real_measure_pulse(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100, 5882);
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_read(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 99.994f, dist);
}

void test_ultrasonic_init_then_read_real_timeout(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100000, 1000);
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_read(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, s);
}

void test_nonblocking_get_cached_before_request_returns_empty(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    float dist = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_EMPTY, dal_ultrasonic_get_cached_distance(&dev, &dist));
}

void test_nonblocking_request_then_get_cached_returns_distance(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100, 5882);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_request_measurement(&dev));
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_get_cached_distance(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_OK, s);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 99.994f, dist);
}

void test_nonblocking_request_timeout_returns_error_status(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100000, 1000);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_request_measurement(&dev));
    float dist = 0.0f;
    wink_status_t s = dal_ultrasonic_get_cached_distance(&dev, &dist);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_TIMEOUT, s);
}

void test_nonblocking_single_tick_wallclock_is_small(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = OWNER, .trig_pin = 4, .echo_pin = 5, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    sim_set_echo_pin(5);
    sim_set_echo_timing(100, 5882);
    clock_t t0 = clock();
    for (int i = 0; i < 1000; i++) {
        wink_status_t rq = dal_ultrasonic_request_measurement(&dev); (void)rq;
        float dist = 0.0f;
        wink_status_t gc = dal_ultrasonic_get_cached_distance(&dev, &dist); (void)gc;
    }
    clock_t dt = clock() - t0;
    TEST_ASSERT(dt < (clock_t)(CLOCKS_PER_SEC / 10));
}

static void build_radar_params(uint8_t *p, uint16_t trig, uint16_t echo) {
    memset(p, 0, 16);
    memcpy(p + 0, &trig, 2);
    memcpy(p + 2, &echo, 2);
}

void test_apply_override_writes_pins(void) {
    dal_ultrasonic_t u = { .config.trig_pin = 4, .config.echo_pin = 5 };
    uint8_t p[16];
    build_radar_params(p, 6, 7);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_apply_override(&u, p, sizeof p));
    TEST_ASSERT_EQUAL_UINT16(6, u.config.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(7, u.config.echo_pin);
}

void test_apply_override_rejects_same_pin(void) {
    dal_ultrasonic_t u = { .config.trig_pin = 4, .config.echo_pin = 5 };
    uint8_t p[16];
    build_radar_params(p, 8, 8);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_apply_override(&u, p, sizeof p));
    TEST_ASSERT_EQUAL_UINT16(4, u.config.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(5, u.config.echo_pin);
}

void test_apply_override_null_returns_invalid_arg(void) {
    uint8_t p[16] = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_apply_override(NULL, p, sizeof p));
    dal_ultrasonic_t u = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_apply_override(&u, NULL, sizeof p));
}

void test_apply_override_v1_writes_pins(void) {
    dal_ultrasonic_t u = { .config.trig_pin = 4, .config.echo_pin = 5 };
    uint8_t p[16] = {0};
    p[0] = 0x01u;
    uint16_t trig = 6, echo = 7;
    memcpy(p + 1, &trig, 2);
    memcpy(p + 3, &echo, 2);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_apply_override(&u, p, sizeof p));
    TEST_ASSERT_EQUAL_UINT16(6, u.config.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(7, u.config.echo_pin);
}

void test_apply_override_too_short_rejected(void) {
    dal_ultrasonic_t u = { .config.trig_pin = 4, .config.echo_pin = 5 };
    uint8_t p[3] = {0x06, 0x00, 0x07};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_apply_override(&u, p, sizeof p));
    TEST_ASSERT_EQUAL_UINT16(4, u.config.trig_pin);
    TEST_ASSERT_EQUAL_UINT16(5, u.config.echo_pin);
}

void test_deinit_hardening(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = { .owner = "radar0", .trig_pin = 4, .echo_pin = 5, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ultrasonic_deinit(NULL));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_deinit(&dev));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 4));
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 5));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 4));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 5));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_deinit(&dev));
}

void test_deinit_loop_two_pins_no_resource_leak(void) {
    dal_ultrasonic_t dev = {0};
    const dal_ultrasonic_config_t cfg = {
        .owner = "radar_loop", .trig_pin = 6, .echo_pin = 7, .variant = DAL_ULTRASONIC_VARIANT_HCSR04, .backend = DAL_ULTRASONIC_BACKEND_GPIO_POLL,
    };
    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 6));
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 7));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ultrasonic_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 6));
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 7));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ultrasonic_init_null_returns_invalid_arg);
    RUN_TEST(test_ultrasonic_init_rejects_same_pin);
    RUN_TEST(test_ultrasonic_read_before_init_returns_not_initialized);
    RUN_TEST(test_read_null_returns_invalid_arg);
    RUN_TEST(test_read_null_out_returns_invalid_arg);
    RUN_TEST(test_pulse_to_cm_100cm);
    RUN_TEST(test_ultrasonic_init_then_read_real_measure_pulse);
    RUN_TEST(test_ultrasonic_init_then_read_real_timeout);
    RUN_TEST(test_nonblocking_get_cached_before_request_returns_empty);
    RUN_TEST(test_nonblocking_request_then_get_cached_returns_distance);
    RUN_TEST(test_nonblocking_request_timeout_returns_error_status);
    RUN_TEST(test_nonblocking_single_tick_wallclock_is_small);
    RUN_TEST(test_apply_override_writes_pins);
    RUN_TEST(test_apply_override_rejects_same_pin);
    RUN_TEST(test_apply_override_null_returns_invalid_arg);
    RUN_TEST(test_apply_override_v1_writes_pins);
    RUN_TEST(test_apply_override_too_short_rejected);
    RUN_TEST(test_deinit_hardening);
    RUN_TEST(test_deinit_loop_two_pins_no_resource_leak);
    return UNITY_END();
}

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#  pragma warning(pop)
#endif
