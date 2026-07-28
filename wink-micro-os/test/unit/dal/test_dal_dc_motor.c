#include "unity.h"
#include "wink_status.h"
#include "actuator/dal_dc_motor.h"
#include "pal_resource.h"

static const char *const OWNER = "test_dal_dc_motor";

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

void test_dc_motor_init_null_returns_invalid_arg(void)
{
    const dal_dc_motor_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 0,
        .dir_pin_a = 1,
        .dir_pin_b = 2,
        .pwm_freq_hz = 20000,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_dc_motor_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_dc_motor_init(NULL, NULL));
}

void test_dc_motor_set_speed_before_init_returns_not_initialized(void)
{
    dal_dc_motor_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
                          dal_dc_motor_set_speed(&dev, 0.5f));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
                          dal_dc_motor_safe_off(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
                          dal_dc_motor_brake(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED,
                          dal_dc_motor_coast(&dev));
}

void test_dc_motor_basic_speed_control(void)
{
    dal_dc_motor_t dev = {0};
    const dal_dc_motor_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 1,
        .dir_pin_a = 2,
        .dir_pin_b = 3,
        .pwm_freq_hz = 20000,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 1));
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 3));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, 0.75f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.75f, dev.current_speed);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, -0.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -0.5f, dev.current_speed);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, 2.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, dev.current_speed);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, -2.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, dev.current_speed);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 1));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 3));
}

void test_dc_motor_single_direction_pin(void)
{
    dal_dc_motor_t dev = {0};
    const dal_dc_motor_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 2,
        .dir_pin_a = 4,
        .dir_pin_b = -1,
        .pwm_freq_hz = 10000,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 2));
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 4));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN,
                                              (uint32_t)-1));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, 0.8f));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_deinit(&dev));
}

void test_dc_motor_coast_zeros_speed(void)
{
    dal_dc_motor_t dev = {0};
    const dal_dc_motor_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 1,
        .dir_pin_a = 2,
        .dir_pin_b = 3,
        .pwm_freq_hz = 20000,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, 0.6f));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_coast(&dev));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, dev.current_speed);

    /* set_speed(0) must match coast (freewheel). */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, -0.4f));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, dev.current_speed);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_deinit(&dev));
}

void test_dc_motor_brake_dual_pin_ok(void)
{
    dal_dc_motor_t dev = {0};
    const dal_dc_motor_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 1,
        .dir_pin_a = 2,
        .dir_pin_b = 3,
        .pwm_freq_hz = 20000,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, 0.9f));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_brake(&dev));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, dev.current_speed);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_deinit(&dev));
}

void test_dc_motor_brake_single_pin_unsupported(void)
{
    dal_dc_motor_t dev = {0};
    const dal_dc_motor_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 2,
        .dir_pin_a = 4,
        .dir_pin_b = -1,
        .pwm_freq_hz = 10000,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, 0.5f));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_dc_motor_brake(&dev));
    /* Must not silently coast: speed left unchanged on unsupported brake. */
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, dev.current_speed);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_deinit(&dev));
}

void test_dc_motor_safe_off_binds_brake(void)
{
    dal_dc_motor_t dual = {0};
    const dal_dc_motor_config_t dual_cfg = {
        .owner = OWNER,
        .pwm_channel = 1,
        .dir_pin_a = 2,
        .dir_pin_b = 3,
        .pwm_freq_hz = 20000,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_init(&dual, &dual_cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dual, 0.7f));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_safe_off(&dual));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, dual.current_speed);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_deinit(&dual));

    dal_dc_motor_t single = {0};
    const dal_dc_motor_config_t single_cfg = {
        .owner = OWNER,
        .pwm_channel = 2,
        .dir_pin_a = 4,
        .dir_pin_b = -1,
        .pwm_freq_hz = 10000,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_init(&single, &single_cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&single, 0.3f));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED,
                          dal_dc_motor_safe_off(&single));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.3f, single.current_speed);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_deinit(&single));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dc_motor_init_null_returns_invalid_arg);
    RUN_TEST(test_dc_motor_set_speed_before_init_returns_not_initialized);
    RUN_TEST(test_dc_motor_basic_speed_control);
    RUN_TEST(test_dc_motor_single_direction_pin);
    RUN_TEST(test_dc_motor_coast_zeros_speed);
    RUN_TEST(test_dc_motor_brake_dual_pin_ok);
    RUN_TEST(test_dc_motor_brake_single_pin_unsupported);
    RUN_TEST(test_dc_motor_safe_off_binds_brake);
    return UNITY_END();
}
