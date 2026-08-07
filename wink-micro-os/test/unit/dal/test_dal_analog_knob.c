// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_analog_knob.c
 * @brief DAL analog knob driver unit tests.
 */
#ifndef WINK_USE_ANALOG_KNOB
#define WINK_USE_ANALOG_KNOB 1
#endif

#include "unity.h"
#include "wink_status.h"
#include "input/dal_analog_knob.h"
#include "hal/pal_adc.h"
#include "hal/pal_hal.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"

static const char *const OWNER = "test_dal_analog_knob";

void setUp(void) {
    pal_resource_reset();
    pal_host_reset_gpio_levels();
    for (uint8_t i = 0; i < PAL_ADC_CHANNELS; i++) {
        pal_adc_deinit(i);
    }
}

void tearDown(void) {}

void test_analog_knob_init_null_returns_invalid_arg(void) {
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 36,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_analog_knob_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_analog_knob_init(NULL, NULL));

    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t null_owner_cfg = { .owner = NULL, .pin = 36 };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_analog_knob_init(&dev, &null_owner_cfg));
}

void test_analog_knob_ops_before_init_returns_not_initialized(void) {
    dal_analog_knob_t dev = {0};
    uint16_t promille = 0;
    uint16_t mv = 0;
    bool changed = false;
    wink_status_t status = WINK_OK;

    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_analog_knob_read_mv(&dev, &mv));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_analog_knob_poll(&dev, &changed, &promille));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_analog_knob_get_status(&dev, &status));
}

void test_analog_knob_already_initialized_returns_error(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 36,
        .enable_pin = -1,
        .min_mv = 0,
        .max_mv = 3300,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_ALREADY_INITIALIZED, dal_analog_knob_init(&dev, &cfg));
}

void test_analog_knob_conversion_0mv_mid_fullscale(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 36,
        .min_mv = 0,
        .max_mv = 3300,
        .hysteresis_promille = 10,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));

    pal_adc_channel_t ch = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(36, &ch));

    /* 0 mV -> 0 promille */
    pal_host_adc_inject_mv(ch, 0);
    uint16_t promille = 0xFFFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(0, promille);

    /* 1650 mV -> 500 promille */
    pal_host_adc_inject_mv(ch, 1650);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(500, promille);

    /* 3300 mV -> 1000 promille */
    pal_host_adc_inject_mv(ch, 3300);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(1000, promille);
}

void test_analog_knob_inverted_polarity(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 39,
        .min_mv = 0,
        .max_mv = 3300,
        .inverted = true,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));

    pal_adc_channel_t ch = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(39, &ch));

    /* 0 mV inverted -> 1000 promille */
    pal_host_adc_inject_mv(ch, 0);
    uint16_t promille = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(1000, promille);

    /* 3300 mV inverted -> 0 promille */
    pal_host_adc_inject_mv(ch, 3300);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(0, promille);
}

void test_analog_knob_hysteresis_suppression_in_poll(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 34,
        .min_mv = 0,
        .max_mv = 3300,
        .hysteresis_promille = 20, /* 20 promille threshold */
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));

    pal_adc_channel_t ch = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(34, &ch));

    /* Initial read: 0 mV -> 0 promille */
    pal_host_adc_inject_mv(ch, 0);
    bool changed = false;
    uint16_t promille = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_poll(&dev, &changed, &promille));
    TEST_ASSERT_EQUAL_UINT16(0, promille);

    /* Small fluctuation: 33 mV (~10 promille < 20 hysteresis threshold) -> changed should be false and promille remains 0 */
    pal_host_adc_inject_mv(ch, 33);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_poll(&dev, &changed, &promille));
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL_UINT16(0, promille);

    /* Large change: 330 mV (~100 promille >= 20 hysteresis threshold) -> changed should be true and promille updates to 100 */
    pal_host_adc_inject_mv(ch, 330);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_poll(&dev, &changed, &promille));
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL_UINT16(100, promille);
}

void test_analog_knob_endpoint_clamping(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 36,
        .min_mv = 0,
        .max_mv = 3300,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));

    pal_adc_channel_t ch = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(36, &ch));

    /* 20 mV (approx 6 promille <= 10) -> clamped to 0 */
    pal_host_adc_inject_mv(ch, 20);
    uint16_t promille = 0xFFFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(0, promille);

    /* 3280 mV (approx 994 promille >= 990) -> clamped to 1000 */
    pal_host_adc_inject_mv(ch, 3280);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(1000, promille);
}

void test_analog_knob_zero_span_protection(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 35,
        .min_mv = 1500,
        .max_mv = 1500, /* min_mv == max_mv defense */
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));

    pal_adc_channel_t ch = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(35, &ch));

    pal_host_adc_inject_mv(ch, 2000);

    uint16_t promille = 0xFFFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    /* Should return 0 without floating-point exception or crash */
    TEST_ASSERT_EQUAL_UINT16(0, promille);
}

void test_analog_knob_guard_c_zero_default_range(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 33,
        .min_mv = 0,
        .max_mv = 0, /* Guard C: 0 && 0 -> platform full scale 3300mV */
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));

    pal_adc_channel_t ch = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(33, &ch));

    pal_host_adc_inject_mv(ch, 1650);
    uint16_t promille = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(500, promille);
}

void test_analog_knob_enable_pin_power_control(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 32,
        .enable_pin = 4,
        .min_mv = 0,
        .max_mv = 3300,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));

    /* Enable pin should be driven HIGH on init */
    bool level = false;
    TEST_ASSERT_TRUE(pal_host_get_gpio_level(4, &level));
    TEST_ASSERT_TRUE(level);

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_deinit(&dev));

    /* Enable pin should be driven LOW on deinit */
    TEST_ASSERT_TRUE(pal_host_get_gpio_level(4, &level));
    TEST_ASSERT_FALSE(level);
}

void test_analog_knob_variant_center_detent(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 31,
        .min_mv = 0,
        .max_mv = 3300,
        .variant = DAL_ANALOG_KNOB_VARIANT_CENTER_DETENT,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));

    pal_adc_channel_t ch = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(31, &ch));

    /* 1617 mV (490 promille in 480~520 range) -> clamped to 500 */
    pal_host_adc_inject_mv(ch, 1617);
    uint16_t promille = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(500, promille);

    /* 990 mV (300 promille outside 480~520 range) -> remains 300 */
    pal_host_adc_inject_mv(ch, 990);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(300, promille);
}

void test_analog_knob_variant_logarithmic(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 30,
        .min_mv = 0,
        .max_mv = 3300,
        .variant = DAL_ANALOG_KNOB_VARIANT_LOGARITHMIC,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));

    pal_adc_channel_t ch = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(30, &ch));

    /* 825 mV (250 promille raw voltage) -> log-to-linear sqrt(250 * 1000) = 500 promille */
    pal_host_adc_inject_mv(ch, 825);
    uint16_t promille = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(500, promille);
}

void test_analog_knob_variant_anti_logarithmic(void) {
    dal_analog_knob_t dev = {0};
    dal_analog_knob_config_t cfg = {
        .owner = OWNER,
        .pin = 27,
        .min_mv = 0,
        .max_mv = 3300,
        .variant = DAL_ANALOG_KNOB_VARIANT_ANTI_LOGARITHMIC,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_init(&dev, &cfg));

    pal_adc_channel_t ch = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(27, &ch));

    /* 1650 mV (500 promille raw voltage) -> antilog (500 * 500)/1000 = 250 promille */
    pal_host_adc_inject_mv(ch, 1650);
    uint16_t promille = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_analog_knob_read_promille(&dev, &promille));
    TEST_ASSERT_EQUAL_UINT16(250, promille);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_analog_knob_init_null_returns_invalid_arg);
    RUN_TEST(test_analog_knob_ops_before_init_returns_not_initialized);
    RUN_TEST(test_analog_knob_already_initialized_returns_error);
    RUN_TEST(test_analog_knob_conversion_0mv_mid_fullscale);
    RUN_TEST(test_analog_knob_inverted_polarity);
    RUN_TEST(test_analog_knob_hysteresis_suppression_in_poll);
    RUN_TEST(test_analog_knob_endpoint_clamping);
    RUN_TEST(test_analog_knob_zero_span_protection);
    RUN_TEST(test_analog_knob_guard_c_zero_default_range);
    RUN_TEST(test_analog_knob_enable_pin_power_control);
    RUN_TEST(test_analog_knob_variant_center_detent);
    RUN_TEST(test_analog_knob_variant_logarithmic);
    RUN_TEST(test_analog_knob_variant_anti_logarithmic);
    return UNITY_END();
}
