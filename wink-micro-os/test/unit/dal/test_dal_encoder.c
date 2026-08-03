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
    const dal_encoder_config_t cfg = { .owner = OWNER, .pin_a = 1, .pin_b = 2, .pull = DAL_ENCODER_PULL_UP };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_encoder_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_encoder_init(NULL, NULL));
}

void test_encoder_init_rejects_negative_pin_a(void)
{
    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg = { .owner = OWNER, .pin_a = -1, .pin_b = -1, .pull = DAL_ENCODER_PULL_UP };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_encoder_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);
}

void test_encoder_init_rejects_invalid_pull(void)
{
    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg = {
        .owner = OWNER, .pin_a = 1, .pin_b = -1,
        .pull = (dal_encoder_pull_t)99,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_encoder_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);
    /* Claim must not leak on a rejected config */
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 1));
}

void test_encoder_get_count_before_init_returns_not_initialized(void)
{
    dal_encoder_t dev = {0};
    int32_t count = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_encoder_reset(&dev));
}

void test_encoder_get_count_null_out_returns_invalid_arg(void)
{
    dal_encoder_t dev = {0};
    int32_t count = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_encoder_get_count(NULL, &count));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_encoder_get_count(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_encoder_reset(NULL));
}

void test_encoder_unidirectional(void)
{
    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg = { .owner = OWNER, .pin_a = 1, .pin_b = -1, .pull = DAL_ENCODER_PULL_UP };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_TRUE(dev.isr_registered);
    TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 1));

    int32_t count = 100;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(0, count);

    /* Simulate 3 rising edges on Pin A */
    pal_host_trigger_gpio_interrupt(1);
    pal_host_trigger_gpio_interrupt(1);
    pal_host_trigger_gpio_interrupt(1);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(3, count);

    /* Reset */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_reset(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_get_count(&dev, &count));
    TEST_ASSERT_EQUAL_INT(0, count);

    /* Deinit */
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
        .pull = DAL_ENCODER_PULL_UP,
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
        .pull = DAL_ENCODER_PULL_UP,
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
        .pull = DAL_ENCODER_PULL_UP,
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
        .pull = DAL_ENCODER_PULL_UP,
        .variant = DAL_ENCODER_VARIANT_X2,
    };
    const dal_encoder_config_t cfg_x4 = {
        .owner = OWNER,
        .pin_a = 10,
        .pin_b = 11,
        .pull = DAL_ENCODER_PULL_UP,
        .variant = DAL_ENCODER_VARIANT_X4,
    };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_encoder_init(&dev, &cfg_x2));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 8));

    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_encoder_init(&dev, &cfg_x4));
    TEST_ASSERT_FALSE(dev.initialized);
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 10));
}

/* DAL-L-004: re-init on an already initialized handle must fail-fast */
void test_encoder_double_init_returns_already_initialized(void)
{
    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg = { .owner = OWNER, .pin_a = 12, .pin_b = -1, .pull = DAL_ENCODER_PULL_UP };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_ALREADY_INITIALIZED, dal_encoder_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev));
}

/* DAL-L-008: claiming pin_b after pin_a fails must release pin_a (no leak) */
void test_encoder_init_rollback_releases_pin_a_on_pin_b_conflict(void)
{
    /* Pre-claim pin_b under a different owner so the driver's claim gets BUSY */
    TEST_ASSERT_EQUAL_INT(WINK_OK,
        pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 15, "other_owner"));

    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg = { .owner = OWNER, .pin_a = 14, .pin_b = 15, .pull = DAL_ENCODER_PULL_UP };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_encoder_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.initialized);
    /* pin_a claim must have been rolled back */
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 14));
}

/* ADR-0024 §4 #8: 10-round init->deinit loop must not leak GPIO claims */
void test_encoder_deinit_loop_no_resource_leak(void)
{
    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg = {
        .owner = "encoder_loop", .pin_a = 16, .pin_b = 17, .pull = DAL_ENCODER_PULL_UP,
    };

    for (int round = 0; round < 10; round++) {
        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&dev, &cfg));
        TEST_ASSERT_TRUE(dev.initialized);
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 16));
        TEST_ASSERT_TRUE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 17));

        /* Exercise the ISR while initialized */
        pal_host_trigger_gpio_interrupt(16);

        TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev));
        TEST_ASSERT_FALSE(dev.initialized);
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 16));
        TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 17));
    }
}

/* DAL-L-010: deinit is idempotent on NULL / uninitialized / already-deinited */
void test_encoder_deinit_idempotent(void)
{
    dal_encoder_t dev = {0};
    const dal_encoder_config_t cfg = { .owner = OWNER, .pin_a = 18, .pin_b = -1, .pull = DAL_ENCODER_PULL_UP };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_encoder_deinit(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev));   /* uninitialized */

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev));   /* after deinit */
}

/* PULL_NONE / PULL_DOWN configs must be accepted (enum mapping coverage) */
void test_encoder_pull_variants_accepted(void)
{
    dal_encoder_t dev_none = {0};
    dal_encoder_t dev_down = {0};
    const dal_encoder_config_t cfg_none = { .owner = OWNER, .pin_a = 20, .pin_b = -1, .pull = DAL_ENCODER_PULL_NONE };
    const dal_encoder_config_t cfg_down = { .owner = OWNER, .pin_a = 21, .pin_b = -1, .pull = DAL_ENCODER_PULL_DOWN };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&dev_none, &cfg_none));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_init(&dev_down, &cfg_down));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev_none));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_encoder_deinit(&dev_down));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encoder_init_null_returns_invalid_arg);
    RUN_TEST(test_encoder_init_rejects_negative_pin_a);
    RUN_TEST(test_encoder_init_rejects_invalid_pull);
    RUN_TEST(test_encoder_get_count_before_init_returns_not_initialized);
    RUN_TEST(test_encoder_get_count_null_out_returns_invalid_arg);
    RUN_TEST(test_encoder_unidirectional);
    RUN_TEST(test_encoder_bidirectional);
    RUN_TEST(test_encoder_x1_explicit_variant);
    RUN_TEST(test_encoder_invert_swaps_direction);
    RUN_TEST(test_encoder_decode_x2_x4_unsupported);
    RUN_TEST(test_encoder_double_init_returns_already_initialized);
    RUN_TEST(test_encoder_init_rollback_releases_pin_a_on_pin_b_conflict);
    RUN_TEST(test_encoder_deinit_loop_no_resource_leak);
    RUN_TEST(test_encoder_deinit_idempotent);
    RUN_TEST(test_encoder_pull_variants_accepted);
    return UNITY_END();
}
