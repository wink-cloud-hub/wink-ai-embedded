#include "unity.h"
#include "wink_status.h"
#include "actuator/dal_motor.h"
#include "pal_resource.h"

static const char *const OWNER = "test_dal_motor";

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

void test_motor_init_null_returns_invalid_arg(void)
{
    const dal_motor_config_t cfg = { .owner = OWNER, .pwm_channel = 0, .dir_pin_a = 1, .dir_pin_b = 2, .pwm_freq_hz = 20000 };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_motor_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_motor_init(NULL, NULL));
}

void test_motor_set_speed_before_init_returns_not_initialized(void)
{
    dal_motor_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_motor_set_speed(&dev, 0.5f));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_motor_safe_off(&dev));
}

void test_motor_basic_speed_control(void)
{
    dal_motor_t dev = {0};
    const dal_motor_config_t cfg = { .owner = OWNER, .pwm_channel = 1, .dir_pin_a = 2, .dir_pin_b = 3, .pwm_freq_hz = 20000 };
    
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 1));
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 3));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_set_speed(&dev, 0.75f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.75f, dev.current_speed);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_set_speed(&dev, -0.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -0.5f, dev.current_speed);

    // Test clipping
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_set_speed(&dev, 2.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, dev.current_speed);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_set_speed(&dev, -2.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, dev.current_speed);

    // Deinit
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 1));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 3));
}

void test_motor_single_direction_pin(void)
{
    dal_motor_t dev = {0};
    const dal_motor_config_t cfg = { .owner = OWNER, .pwm_channel = 2, .dir_pin_a = 4, .dir_pin_b = -1, .pwm_freq_hz = 10000 };
    
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 2));
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 4));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)-1));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_set_speed(&dev, 0.8f));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_motor_deinit(&dev));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_motor_init_null_returns_invalid_arg);
    RUN_TEST(test_motor_set_speed_before_init_returns_not_initialized);
    RUN_TEST(test_motor_basic_speed_control);
    RUN_TEST(test_motor_single_direction_pin);
    return UNITY_END();
}
