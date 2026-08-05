#include "unity.h"
#include "wink_status.h"
#include "output/dal_relay.h"
#include "pal_resource.h"
#include "pal_osal.h"          /* pal_os_busy_wait_us (host virtual time) */
#include "host_test_ctrl.h"    /* pal_host_get_gpio_level, pal_host_reset_gpio_levels */

static const char *const OWNER = "test_dal_relay";

void setUp(void)
{
    sim_reset_time();
    pal_resource_reset();
    pal_host_reset_gpio_levels();
}
void tearDown(void) {}

/* Read a captured output level; fail the test if the pin was never written. */
static bool assert_level(wink_pin_t pin)
{
    bool level = false;
    if (!pal_host_get_gpio_level(pin, &level)) {
        TEST_FAIL_MESSAGE("pin was never written");
    }
    return level;
}

/* ---- Null and uninitialized error handling ---- */
void test_relay_init_null_returns_invalid_arg(void) {
    const dal_relay_config_t cfg = { .owner = OWNER, .pin = 2 };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_init(NULL, NULL));
}

void test_relay_ops_before_init_returns_not_initialized(void) {
    dal_relay_t dev = {0};
    bool is_on = false;

    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_on(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_off(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_toggle(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_set(&dev, true));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_is_on(&dev, &is_on));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_relay_get_last_status(&dev));
    /* safe_off on uninitialized is success (DAL-L-022), not NOT_INITIALIZED. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_safe_off(&dev));
    /* poll on uninitialized is an idempotent no-op. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_poll(&dev));
}

void test_relay_ops_null_returns_invalid_arg(void) {
    bool is_on = false;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_on(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_off(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_toggle(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_set(NULL, true));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_is_on(NULL, &is_on));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_is_on((const dal_relay_t *)1, NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_get_last_status(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_safe_off(NULL));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_poll(NULL));
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
    /* active-high, initial off → LOW */
    TEST_ASSERT_FALSE(assert_level(12));

    bool current_state = true;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_is_on(&dev, &current_state));
    TEST_ASSERT_FALSE(current_state);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_on(&dev));
    TEST_ASSERT_TRUE(dev.is_on);
    TEST_ASSERT_TRUE(assert_level(12));  /* active-high on → HIGH */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_is_on(&dev, &current_state));
    TEST_ASSERT_TRUE(current_state);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_off(&dev));
    TEST_ASSERT_FALSE(assert_level(12));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_toggle(&dev));
    TEST_ASSERT_TRUE(dev.is_on);
    TEST_ASSERT_TRUE(assert_level(12));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
}

/* ---- SSR / active-low polarity ---- */
void test_relay_ssr_active_low_polarity(void) {
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
    /* active-low, initial on → LOW */
    TEST_ASSERT_FALSE(assert_level(14));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_off(&dev));
    TEST_ASSERT_FALSE(dev.is_on);
    /* active-low off → HIGH */
    TEST_ASSERT_TRUE(assert_level(14));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
}

/* ---- Latching dual-pin: pulse lifecycle and clearing via poll ---- */
void test_relay_latching_dual_pin_pulse_clears_via_poll(void) {
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

    /* initial_state=false → init drives a RESET pulse on reset_pin (active=HIGH). */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev, &cfg));
    TEST_ASSERT_FALSE(dev.is_on);
    TEST_ASSERT_TRUE(dev.pulse_active);
    TEST_ASSERT_FALSE(assert_level(15));       /* set pin inactive */
    TEST_ASSERT_TRUE(assert_level(16));        /* reset pin active (HIGH) */

    /* Advance virtual time past the pulse width and poll → both pins inactive. */
    pal_os_busy_wait_us(31000);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_poll(&dev));
    TEST_ASSERT_FALSE(dev.pulse_active);
    TEST_ASSERT_FALSE(assert_level(15));
    TEST_ASSERT_FALSE(assert_level(16));

    /* turn_on → SET pulse on pin 15. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_on(&dev));
    TEST_ASSERT_TRUE(dev.is_on);
    TEST_ASSERT_TRUE(dev.pulse_active);
    TEST_ASSERT_TRUE(assert_level(15));         /* set active */
    TEST_ASSERT_FALSE(assert_level(16));        /* reset inactive */

    pal_os_busy_wait_us(31000);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_poll(&dev));
    TEST_ASSERT_FALSE(dev.pulse_active);
    TEST_ASSERT_FALSE(assert_level(15));
    TEST_ASSERT_FALSE(assert_level(16));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
}

/* Latching init with initial_state=true establishes a SET pulse (known state). */
void test_relay_latching_initial_state_true_emits_set_pulse(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER,
        .pin = 15,
        .reset_pin = 16,
        .pulse_duration_ms = 50,
        .variant = DAL_RELAY_VARIANT_LATCHING_DUAL_PIN,
        .active_low = false,
        .initial_state = true
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.is_on);
    TEST_ASSERT_TRUE(dev.pulse_active);
    TEST_ASSERT_TRUE(assert_level(15));   /* SET active */
    TEST_ASSERT_FALSE(assert_level(16));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
}

/* Rapid on→off must break-before-make: the two coil pins are never both active. */
void test_relay_latching_break_before_make_no_overlap(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER,
        .pin = 15,
        .reset_pin = 16,
        .pulse_duration_ms = 50,
        .variant = DAL_RELAY_VARIANT_LATCHING_DUAL_PIN,
        .active_low = false,
        .initial_state = false
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev, &cfg));
    pal_os_busy_wait_us(60000);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_poll(&dev));
    TEST_ASSERT_FALSE(dev.pulse_active);

    /* turn_on: only set pin active. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_on(&dev));
    TEST_ASSERT_TRUE(assert_level(15));
    TEST_ASSERT_FALSE(assert_level(16));

    /* Immediate turn_off without waiting: must not leave set active; only reset. */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_off(&dev));
    TEST_ASSERT_FALSE(assert_level(15));
    TEST_ASSERT_TRUE(assert_level(16));

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

void test_relay_pulse_duration_zero_defaults(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER,
        .pin = 15,
        .reset_pin = 16,
        .pulse_duration_ms = 0,   /* → DAL_RELAY_DEFAULT_PULSE_MS (50) */
        .variant = DAL_RELAY_VARIANT_LATCHING_DUAL_PIN,
        .initial_state = false
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_UINT16(DAL_RELAY_DEFAULT_PULSE_MS, dev.config.pulse_duration_ms);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
}

void test_relay_pulse_duration_over_max_rejected(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER,
        .pin = 15,
        .reset_pin = 16,
        .pulse_duration_ms = DAL_RELAY_MAX_PULSE_MS + 1u,
        .variant = DAL_RELAY_VARIANT_LATCHING_DUAL_PIN
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_relay_init(&dev, &cfg));
    /* No claims should leak after rejection. */
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 15));
    TEST_ASSERT_FALSE(pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, 16));
}

/* ---- safe_off ---- */
void test_relay_safe_off_deenergizes(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER,
        .pin = 12,
        .reset_pin = -1,
        .variant = DAL_RELAY_VARIANT_DIRECT_GPIO,
        .active_low = false,
        .initial_state = true     /* init energized */
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.is_on);
    TEST_ASSERT_TRUE(assert_level(12));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_safe_off(&dev));
    TEST_ASSERT_FALSE(dev.is_on);
    TEST_ASSERT_FALSE(assert_level(12));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
}

/* ---- last_status getter ---- */
void test_relay_get_last_status_tracks_operations(void) {
    dal_relay_t dev = {0};
    const dal_relay_config_t cfg = {
        .owner = OWNER, .pin = 12, .reset_pin = -1,
        .variant = DAL_RELAY_VARIANT_DIRECT_GPIO
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_get_last_status(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_on(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_get_last_status(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
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

/* deinit must be idempotent and a no-op on an uninitialized handle. */
void test_relay_deinit_uninitialized_is_noop(void) {
    dal_relay_t dev = {0};
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_relay_deinit(&dev));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_relay_init_null_returns_invalid_arg);
    RUN_TEST(test_relay_ops_before_init_returns_not_initialized);
    RUN_TEST(test_relay_ops_null_returns_invalid_arg);
    RUN_TEST(test_relay_direct_gpio_init_and_state);
    RUN_TEST(test_relay_ssr_active_low_polarity);
    RUN_TEST(test_relay_latching_dual_pin_pulse_clears_via_poll);
    RUN_TEST(test_relay_latching_initial_state_true_emits_set_pulse);
    RUN_TEST(test_relay_latching_break_before_make_no_overlap);
    RUN_TEST(test_relay_latching_missing_reset_pin_fails);
    RUN_TEST(test_relay_pulse_duration_zero_defaults);
    RUN_TEST(test_relay_pulse_duration_over_max_rejected);
    RUN_TEST(test_relay_safe_off_deenergizes);
    RUN_TEST(test_relay_get_last_status_tracks_operations);
    RUN_TEST(test_relay_double_init_returns_already_initialized);
    RUN_TEST(test_relay_gpio_conflict_returns_busy);
    RUN_TEST(test_relay_deinit_uninitialized_is_noop);
    return UNITY_END();
}
