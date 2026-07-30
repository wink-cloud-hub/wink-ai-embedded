#include "unity.h"
#include "wink_status.h"
#include "actuator/dal_dc_motor.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"
#include "internal/pal_test_loopback.h"

static const char *const OWNER = "test_dal_dc_motor";

void setUp(void)
{
    pal_resource_reset();
    sim_clear_gpio_ideal();
}

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
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 20, OWNER));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 21, OWNER));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(2, 20));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(3, 21));
    sim_set_gpio_ideal(20, false);
    sim_set_gpio_ideal(21, false);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, 0.9f));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_brake(&dev));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, dev.current_speed);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, sim_last_pwm_duty(1));

    /* Host loopback: both dir outputs HIGH for short-brake. */
    bool lvl_a = false;
    bool lvl_b = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(20, &lvl_a));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(21, &lvl_b));
    TEST_ASSERT_TRUE(lvl_a);
    TEST_ASSERT_TRUE(lvl_b);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(2, 20));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(3, 21));
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

void test_dc_motor_unimplemented_variant_init_unsupported(void)
{
    dal_dc_motor_t dev = {0};
    const dal_dc_motor_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 0,
        .dir_pin_a = 1,
        .dir_pin_b = 2,
        .pwm_freq_hz = 20000,
        .variant = DAL_DC_MOTOR_VARIANT_PHASE_ENABLE,
    };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_dc_motor_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_PWM_CHANNEL, 0));
}

void test_dc_motor_enable_pin_claimed_and_safe_off_ok(void)
{
    dal_dc_motor_t dev = {0};
    const dal_dc_motor_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 1,
        .dir_pin_a = 4,
        .dir_pin_b = -1,
        .pwm_freq_hz = 20000,
        .enable_pin = 10,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_init(&dev, &cfg));
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 10));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(10, 11));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 11, OWNER));
    sim_set_gpio_ideal(11, false);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, 0.5f));
    bool enable_lvl = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(11, &enable_lvl));
    TEST_ASSERT_TRUE(enable_lvl);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_safe_off(&dev));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, dev.current_speed);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(11, &enable_lvl));
    TEST_ASSERT_FALSE(enable_lvl);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(10, 11));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_deinit(&dev));
}

void test_dc_motor_enable_safe_off_dual_pin_brakes_first(void)
{
    dal_dc_motor_t dev = {0};
    const dal_dc_motor_config_t cfg = {
        .owner = OWNER,
        .pwm_channel = 1,
        .dir_pin_a = 2,
        .dir_pin_b = 3,
        .pwm_freq_hz = 20000,
        .enable_pin = 10,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 20, OWNER));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 21, OWNER));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 11, OWNER));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(2, 20));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(3, 21));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_enable_hardware_loopback(10, 11));
    sim_set_gpio_ideal(20, false);
    sim_set_gpio_ideal(21, false);
    sim_set_gpio_ideal(11, false);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_set_speed(&dev, 0.8f));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_safe_off(&dev));

    bool dir_a = false;
    bool dir_b = false;
    bool enable_lvl = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(20, &dir_a));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(21, &dir_b));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(11, &enable_lvl));
    TEST_ASSERT_TRUE(dir_a);
    TEST_ASSERT_TRUE(dir_b);
    TEST_ASSERT_FALSE(enable_lvl);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(2, 20));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(3, 21));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_test_disable_hardware_loopback(10, 11));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_dc_motor_deinit(&dev));
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
    RUN_TEST(test_dc_motor_unimplemented_variant_init_unsupported);
    RUN_TEST(test_dc_motor_enable_pin_claimed_and_safe_off_ok);
    RUN_TEST(test_dc_motor_enable_safe_off_dual_pin_brakes_first);
    return UNITY_END();
}
