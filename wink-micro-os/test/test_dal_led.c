#include "unity.h"
#include "wink_status.h"
#include "dal_led.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- init 契约 ---- */
void test_init_null_returns_invalid_arg(void) {
    const dal_led_config_t cfg = { .pin = 2, .active_high = true };
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
}

/* ---- init 后 on/off/set/toggle（host pal_gpio_write 无真实电平可校验，校验状态位）---- */
void test_active_high_on_off(void) {
    dal_led_t dev = {0};
    const dal_led_config_t cfg = { .pin = 2, .active_high = true };
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
    const dal_led_config_t cfg = { .pin = 3, .active_high = false };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev, &cfg));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_on(&dev));
    TEST_ASSERT_TRUE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_off(&dev));
    TEST_ASSERT_FALSE(dev.is_on);
}

void test_toggle_flips_state(void) {
    dal_led_t dev = {0};
    const dal_led_config_t cfg = { .pin = 4, .active_high = true };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_init(&dev, &cfg));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_toggle(&dev));
    TEST_ASSERT_TRUE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_led_toggle(&dev));
    TEST_ASSERT_FALSE(dev.is_on);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_null_returns_invalid_arg);
    RUN_TEST(test_set_before_init_returns_not_initialized);
    RUN_TEST(test_set_null_returns_invalid_arg);
    RUN_TEST(test_active_high_on_off);
    RUN_TEST(test_active_low_on_off);
    RUN_TEST(test_toggle_flips_state);
    return UNITY_END();
}
