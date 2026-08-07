// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_buzzer.c
 * @brief DAL buzzer driver unit tests (PASSIVE_PWM & ACTIVE_GPIO variants).
 */
#include "unity.h"
#include "wink_status.h"
#include "output/dal_buzzer.h"
#include "pal_resource.h"
#include "pal_pwm_router.h"

static const char *const OWNER = "test_dal_buzzer";

void setUp(void) {
    pal_resource_reset();
    pal_pwm_router_reset();
}

void tearDown(void) {}

void test_init_null_returns_invalid_arg(void) {
    const dal_buzzer_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 0,
        .variant = DAL_BUZZER_VARIANT_PASSIVE_PWM
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_buzzer_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_buzzer_init(NULL, NULL));
}

void test_uninitialized_returns_not_initialized(void) {
    dal_buzzer_t dev = {0};
    bool is_on = false;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_buzzer_on(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_buzzer_off(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_buzzer_toggle(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_buzzer_play_tone(&dev, 1000u));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_buzzer_is_on(&dev, &is_on));
}

void test_passive_pwm_init_and_play_tone(void) {
    dal_buzzer_t dev = {0};
    const dal_buzzer_config_t cfg = {
        .owner = OWNER,
        .default_freq_hz = 2000u,
        .pwm_channel = 1,
        .variant = DAL_BUZZER_VARIANT_PASSIVE_PWM,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_FALSE(dev.is_on);
    TEST_ASSERT_EQUAL_UINT32(0u, dev.current_freq_hz);

    /* Play tone at 440 Hz */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_play_tone(&dev, 440u));
    TEST_ASSERT_TRUE(dev.is_on);
    TEST_ASSERT_EQUAL_UINT32(440u, dev.current_freq_hz);

    /* Turn off */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_off(&dev));
    TEST_ASSERT_FALSE(dev.is_on);
    TEST_ASSERT_EQUAL_UINT32(0u, dev.current_freq_hz);

    /* Turn on using default freq */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_on(&dev));
    TEST_ASSERT_TRUE(dev.is_on);
    TEST_ASSERT_EQUAL_UINT32(2000u, dev.current_freq_hz);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
}

void test_passive_pwm_freq_bounds(void) {
    dal_buzzer_t dev = {0};
    const dal_buzzer_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 0,
        .variant = DAL_BUZZER_VARIANT_PASSIVE_PWM,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_init(&dev, &cfg));

    /* Frequency 0 -> quiet (stop_tone) */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_play_tone(&dev, 0u));
    TEST_ASSERT_FALSE(dev.is_on);

    /* Below minimum 20 Hz */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_OUT_OF_RANGE, dal_buzzer_play_tone(&dev, 10u));

    /* Above maximum 8000 Hz */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_OUT_OF_RANGE, dal_buzzer_play_tone(&dev, 10000u));

    /* Valid frequency 1000 Hz */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_play_tone(&dev, 1000u));
    TEST_ASSERT_TRUE(dev.is_on);

    dal_buzzer_deinit(&dev);
}

void test_active_gpio_init_and_control(void) {
    dal_buzzer_t dev = {0};
    const dal_buzzer_config_t cfg = {
        .owner = OWNER,
        .pin = 4,
        .active_high = true,
        .variant = DAL_BUZZER_VARIANT_ACTIVE_GPIO,
        .default_freq_hz = 2000u,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_FALSE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_on(&dev));
    TEST_ASSERT_TRUE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_toggle(&dev));
    TEST_ASSERT_FALSE(dev.is_on);

    /* Active buzzer play_tone degraded behavior */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_play_tone(&dev, 500u));
    TEST_ASSERT_TRUE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_deinit(&dev));
}

void test_enable_pin_support(void) {
    dal_buzzer_t dev = {0};
    const dal_buzzer_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 2,
        .enable_pin = 5,
        .enable_active_high = true,
        .variant = DAL_BUZZER_VARIANT_PASSIVE_PWM,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_safe_off(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_deinit(&dev));
}

void test_resource_conflict_detection(void) {
    dal_buzzer_t dev1 = {0};
    dal_buzzer_t dev2 = {0};
    const dal_buzzer_config_t cfg1 = {
        .owner = "owner1",
        .pwm_channel = 3,
        .variant = DAL_BUZZER_VARIANT_PASSIVE_PWM,
    };
    const dal_buzzer_config_t cfg2 = {
        .owner = "owner2",
        .pwm_channel = 3,
        .variant = DAL_BUZZER_VARIANT_PASSIVE_PWM,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_init(&dev1, &cfg1));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_buzzer_init(&dev2, &cfg2));

    dal_buzzer_deinit(&dev1);
}

void test_safe_off_uninitialized_returns_ok(void) {
    dal_buzzer_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_buzzer_safe_off(&dev));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_null_returns_invalid_arg);
    RUN_TEST(test_uninitialized_returns_not_initialized);
    RUN_TEST(test_passive_pwm_init_and_play_tone);
    RUN_TEST(test_passive_pwm_freq_bounds);
    RUN_TEST(test_active_gpio_init_and_control);
    RUN_TEST(test_enable_pin_support);
    RUN_TEST(test_resource_conflict_detection);
    RUN_TEST(test_safe_off_uninitialized_returns_ok);
    return UNITY_END();
}
