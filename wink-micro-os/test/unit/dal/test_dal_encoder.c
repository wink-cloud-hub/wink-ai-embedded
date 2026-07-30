#include "unity.h"
#include "wink_status.h"
#include "sensor/dal_encoder.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"

static const char *const OWNER = "test_dal_encoder";

void setUp(void)
{
    pal_resource_reset();
    sim_clear_gpio_ideal();
    pal_host_reset_isr_stats();
}
void tearDown(void) {}

void test_encoder_init_null_returns_invalid_arg(void)
{
    const dal_encoder_config_t cfg = { .owner = OWNER, .pin_a = 1, .pin_b = 2, .pull = PAL_GPIO_INPUT_PULLUP };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_encoder_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_encoder_init(NULL, NULL));
}

void test_encoder_get_count_before_init_returns_not_initialized(void)
{
    dal_encoder_t dev = {0};
    int32_t count = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_encoder_reset(&dev));
}

void test_encoder_unidirectional(void)
{
    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg = { .owner = OWNER, .pin_a = 1, .pin_b = -1, .pull = PAL_GPIO_INPUT_PULLUP };
    
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 1));

    int32_t count = 100;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(0, count);

    // Simulate 3 rising edges on Pin A
    pal_host_trigger_gpio_interrupt(1);
    pal_host_trigger_gpio_interrupt(1);
    pal_host_trigger_gpio_interrupt(1);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(3, count);

    // Reset
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_reset(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(0, count);

    // Deinit
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 1));
}

void test_encoder_bidirectional(void)
{
    dal_encoder_t dev = {0};
    /* Pin A = 2, Pin B = 3; zero-init variant/invert = x1 / false */
    const dal_encoder_config_t cfg = {
        .owner = OWNER,
        .pin_a = 2,
        .pin_b = 3,
        .pull = PAL_GPIO_INPUT_PULLUP,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 2));
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 3));

    int32_t count = 0;

    /* x1: A rising samples B; B high -> ++ */
    sim_set_gpio_ideal(3, true);
    pal_host_trigger_gpio_interrupt(2);
    pal_host_trigger_gpio_interrupt(2);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(2, count);

    /* x1: B low -> -- */
    sim_set_gpio_ideal(3, false);
    pal_host_trigger_gpio_interrupt(2);
    pal_host_trigger_gpio_interrupt(2);
    pal_host_trigger_gpio_interrupt(2);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(-1, count);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev));
}

void test_encoder_x1_explicit_variant(void)
{
    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg = {
        .owner = OWNER,
        .pin_a = 4,
        .pin_b = 5,
        .pull = PAL_GPIO_INPUT_PULLUP,
        .variant = DAL_ENCODER_VARIANT_X1_RISING,
        .invert = false,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&dev, &cfg));

    int32_t count = 0;
    sim_set_gpio_ideal(5, true);
    pal_host_trigger_gpio_interrupt(4);
    sim_set_gpio_ideal(5, false);
    pal_host_trigger_gpio_interrupt(4);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(0, count);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev));
}

void test_encoder_invert_swaps_direction(void)
{
    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg = {
        .owner = OWNER,
        .pin_a = 6,
        .pin_b = 7,
        .pull = PAL_GPIO_INPUT_PULLUP,
        .variant = DAL_ENCODER_VARIANT_X1_RISING,
        .invert = true,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&dev, &cfg));

    int32_t count = 0;

    /* invert swaps phase polarity: B high -> -- (not ++) */
    sim_set_gpio_ideal(7, true);
    pal_host_trigger_gpio_interrupt(6);
    pal_host_trigger_gpio_interrupt(6);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(-2, count);

    /* B low -> ++ */
    sim_set_gpio_ideal(7, false);
    pal_host_trigger_gpio_interrupt(6);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(-1, count);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev));
}

void test_encoder_decode_x2_x4_unsupported(void)
{
    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg_x2 = {
        .owner = OWNER,
        .pin_a = 8,
        .pin_b = 9,
        .pull = PAL_GPIO_INPUT_PULLUP,
        .variant = DAL_ENCODER_VARIANT_X2,
    };
    const dal_encoder_config_t cfg_x4 = {
        .owner = OWNER,
        .pin_a = 10,
        .pin_b = 11,
        .pull = PAL_GPIO_INPUT_PULLUP,
        .variant = DAL_ENCODER_VARIANT_X4,
    };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_encoder_init(&dev, &cfg_x2));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 8));

    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_encoder_init(&dev, &cfg_x4));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 10));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encoder_init_null_returns_invalid_arg);
    RUN_TEST(test_encoder_get_count_before_init_returns_not_initialized);
    RUN_TEST(test_encoder_unidirectional);
    RUN_TEST(test_encoder_bidirectional);
    RUN_TEST(test_encoder_x1_explicit_variant);
    RUN_TEST(test_encoder_invert_swaps_direction);
    RUN_TEST(test_encoder_decode_x2_x4_unsupported);
    return UNITY_END();
}
