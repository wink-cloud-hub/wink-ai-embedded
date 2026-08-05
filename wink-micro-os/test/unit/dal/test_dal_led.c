// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_led.c
 * @brief DAL LED driver unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "dal_led.h"
#include "pal_resource.h"

static const char *const OWNER = "test_dal_led";

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

void test_init_null_returns_invalid_arg(void) {
    const dal_led_config_t cfg = { .owner = OWNER, .pin = 2, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_led_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_led_init(NULL, NULL));
}

void test_set_before_init_returns_not_initialized(void) {
    dal_led_t dev = { .config.pin = 2, .config.active_high = true, .is_on = false,
                      .initialized = false };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_led_on(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_led_off(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_led_toggle(&dev));
}

void test_set_null_returns_invalid_arg(void) {
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_led_on(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_led_off(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_led_set(NULL, true));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_led_toggle(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_led_safe_off(NULL));
}

void test_init_leaves_led_off_active_high(void) {
    dal_led_t dev = {0};
    const dal_led_config_t cfg = { .owner = OWNER, .pin = 6, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.is_on);
}

void test_init_leaves_led_off_active_low(void) {
    dal_led_t dev = {0};
    const dal_led_config_t cfg = { .owner = OWNER, .pin = 7, .active_high = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.is_on);
}

void test_safe_off_uninitialized_returns_ok(void) {
    dal_led_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_safe_off(&dev));
}

void test_safe_off_initialized_turns_off(void) {
    dal_led_t dev = {0};
    const dal_led_config_t cfg = { .owner = OWNER, .pin = 8, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_on(&dev));
    TEST_ASSERT_TRUE(dev.is_on);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_safe_off(&dev));
    TEST_ASSERT_FALSE(dev.is_on);
}

void test_active_high_on_off(void) {
    dal_led_t dev = {0};
    const dal_led_config_t cfg = { .owner = OWNER, .pin = 2, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_on(&dev));
    TEST_ASSERT_TRUE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_off(&dev));
    TEST_ASSERT_FALSE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_set(&dev, true));
    TEST_ASSERT_TRUE(dev.is_on);
}

void test_active_low_on_off(void) {
    dal_led_t dev = {0};
    const dal_led_config_t cfg = { .owner = OWNER, .pin = 3, .active_high = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev, &cfg));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_on(&dev));
    TEST_ASSERT_TRUE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_off(&dev));
    TEST_ASSERT_FALSE(dev.is_on);
}

void test_toggle_flips_state(void) {
    dal_led_t dev = {0};
    const dal_led_config_t cfg = { .owner = OWNER, .pin = 4, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev, &cfg));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_toggle(&dev));
    TEST_ASSERT_TRUE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_toggle(&dev));
    TEST_ASSERT_FALSE(dev.is_on);
}

void test_deinit_hardening(void) {
    dal_led_t dev = {0};
    const dal_led_config_t cfg = { .owner = OWNER, .pin = 2, .active_high = true };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_led_deinit(NULL));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_deinit(&dev));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_deinit(&dev));

    dal_led_t dev2 = {0};
    const dal_led_config_t cfg2 = { .owner = "another_owner", .pin = 2, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev2, &cfg2));
    TEST_ASSERT_TRUE(dev2.initialized);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_deinit(&dev2));
}

void test_deinit_loop_no_resource_leak(void) {
    dal_led_t dev = {0};
    const dal_led_config_t cfg = { .owner = OWNER, .pin = 5, .active_high = true };

    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 5));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_on(&dev));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_off(&dev));
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 5));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_null_returns_invalid_arg);
    RUN_TEST(test_set_before_init_returns_not_initialized);
    RUN_TEST(test_set_null_returns_invalid_arg);
    RUN_TEST(test_init_leaves_led_off_active_high);
    RUN_TEST(test_init_leaves_led_off_active_low);
    RUN_TEST(test_safe_off_uninitialized_returns_ok);
    RUN_TEST(test_safe_off_initialized_turns_off);
    RUN_TEST(test_active_high_on_off);
    RUN_TEST(test_active_low_on_off);
    RUN_TEST(test_toggle_flips_state);
    RUN_TEST(test_deinit_hardening);
    RUN_TEST(test_deinit_loop_no_resource_leak);
    return UNITY_END();
}
