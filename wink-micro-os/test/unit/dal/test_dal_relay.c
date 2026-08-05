#include "unity.h"
#include "wink_status.h"
#include "output/dal_relay.h"
#include "pal_resource.h"

static const char *const OWNER = "test_dal_relay";

void setUp(void) { pal_resource_reset(); }
void tearDown(void) {}

/* ---- Null and uninitialized error handling ---- */
void test_relay_init_null_returns_invalid_arg(void) {
    const dal_relay_config_t cfg = { .owner = OWNER, .pin = 2 };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_init(NULL, NULL));
}

void test_relay_ops_before_init_returns_not_initialized(void) {
    dal_relay_t dev = {0};
    bool is_on = false;

    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_turn_on(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_turn_off(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_toggle(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_set_state(&dev, true));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_is_on(&dev, &is_on));
}

void test_relay_ops_null_returns_invalid_arg(void) {
    bool is_on = false;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_turn_on(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_turn_off(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_toggle(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_set_state(NULL, true));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_is_on(NULL, &is_on));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_is_on((const dal_relay_t *)1, NULL));
}

/* ---- Direct GPIO variant tests ---- */
void test_relay_direct_gpio_init_and_state(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER,
        .pin = 12,
        .reset_pin = -1,
        .variant = DAL_RELAY_VARIANT_DIRECT_GPIO,
        .active_low = false,
        .initial_state = false
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_FALSE(dev.is_on);

    bool current_state = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_is_on(&dev, &current_state));
    TEST_ASSERT_FALSE(current_state);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_turn_on(&dev));
    TEST_ASSERT_TRUE(dev.is_on);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_is_on(&dev, &current_state));
    TEST_ASSERT_TRUE(current_state);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_toggle(&dev));
    TEST_ASSERT_FALSE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
}

/* ---- SSR variant tests ---- */
void test_relay_ssr_variant(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER,
        .pin = 14,
        .reset_pin = -1,
        .variant = DAL_RELAY_VARIANT_SSR,
        .active_low = true,
        .initial_state = true
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_turn_off(&dev));
    TEST_ASSERT_FALSE(dev.is_on);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
}

/* ---- Latching relay variant tests ---- */
void test_relay_latching_dual_pin(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER,
        .pin = 15,
        .reset_pin = 16,
        .pulse_duration_ms = 30,
        .variant = DAL_RELAY_VARIANT_LATCHING_DUAL_PIN,
        .active_low = false,
        .initial_state = false
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.pulse_active);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_turn_on(&dev));
    TEST_ASSERT_TRUE(dev.is_on);
    TEST_ASSERT_TRUE(dev.pulse_active);

    /* Poll should eventually clear pulse */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_poll(&dev));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
}

void test_relay_latching_missing_reset_pin_fails(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER,
        .pin = 15,
        .reset_pin = -1,
        .variant = DAL_RELAY_VARIANT_LATCHING_DUAL_PIN
    };

    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_init(&dev, &cfg));
}

/* ---- Double init and resource conflict ---- */
void test_relay_double_init_returns_already_initialized(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER,
        .pin = 18,
        .reset_pin = -1,
        .variant = DAL_RELAY_VARIANT_DIRECT_GPIO
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_ALREADY_INITIALIZED, dal_relay_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
}

void test_relay_gpio_conflict_returns_busy(void) {
    dal_relay_t dev1 = {0};
    dal_relay_t dev2 = {0};
    const dal_relay_config_t cfg1 = { .owner = "dev1", .pin = 21, .reset_pin = -1 };
    const dal_relay_config_t cfg2 = { .owner = "dev2", .pin = 21, .reset_pin = -1 };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev1, &cfg1));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, dal_relay_init(&dev2, &cfg2));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev1));
}
