// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_adc.c
 * @brief PAL ADC subsystem unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "hal/pal_adc.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#  pragma warning(disable: 4996)
#endif

void setUp(void) {
    pal_resource_reset();
    for (uint8_t i = 0; i < PAL_ADC_CHANNELS; i++) {
        pal_adc_deinit(i);
    }
}

void tearDown(void) {}

void test_pal_adc_init_invalid_args(void) {
    pal_adc_config_t cfg = { .pin = 36, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_adc_init(PAL_ADC_CHANNELS, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_adc_init(0, NULL));

    pal_adc_config_t bad_pin_cfg = { .pin = -1, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_adc_init(0, &bad_pin_cfg));
}

void test_pal_adc_uninitialized_reads(void) {
    uint16_t val = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, pal_adc_read_raw(0, &val));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, pal_adc_read_mv(0, &val));

    wink_pin_t pin = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, pal_adc_channel_pin(0, &pin));

    uint16_t fs = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, pal_adc_full_scale_mv(0, &fs));
}

void test_pal_adc_init_deinit_reinit(void) {
    pal_adc_config_t cfg = { .pin = 36, .full_scale_mv = 3100, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(0, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_ALREADY_INITIALIZED, pal_adc_init(0, &cfg));

    pal_adc_deinit(0);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(0, &cfg));
}

void test_pal_adc_pin_channel_mapping(void) {
    pal_adc_config_t cfg = { .pin = 39, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(1, &cfg));

    wink_pin_t pin = -1;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_channel_pin(1, &pin));
    TEST_ASSERT_EQUAL_INT(39, pin);

    pal_adc_channel_t ch = 0xFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_pin_channel(39, &ch));
    TEST_ASSERT_EQUAL_INT(1, ch);

    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_FOUND, pal_adc_pin_channel(25, &ch));
}

void test_pal_adc_inject_raw_and_read(void) {
    pal_adc_config_t cfg = { .pin = 34, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(2, &cfg));

    pal_host_adc_inject_raw(2, 2047);

    uint16_t raw = 0;
    uint16_t mv = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(2, &raw));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_mv(2, &mv));
    TEST_ASSERT_EQUAL_INT(2047, raw);
    TEST_ASSERT_EQUAL_UINT16(1650, mv);
}

void test_pal_adc_inject_mv_and_read(void) {
    pal_adc_config_t cfg = { .pin = 35, .full_scale_mv = 3100, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(3, &cfg));

    pal_host_adc_inject_mv(3, 3100);

    uint16_t raw = 0;
    uint16_t mv = 0;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(3, &raw));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_mv(3, &mv));
    TEST_ASSERT_EQUAL_INT(4095, raw);
    TEST_ASSERT_EQUAL_UINT16(3100, mv);
}

void test_pal_adc_zero_value_cache_contract(void) {
    pal_adc_config_t cfg = { .pin = 32, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(4, &cfg));

    pal_host_adc_inject_raw(4, 0);

    uint16_t raw = 0xFFFF;
    uint16_t mv = 0xFFFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(4, &raw));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_mv(4, &mv));
    TEST_ASSERT_EQUAL_UINT16(0, raw);
    TEST_ASSERT_EQUAL_UINT16(0, mv);
}

void test_pal_adc_read_mv_before_read_raw_returns_zero_not_timeout(void) {
    pal_adc_config_t cfg = { .pin = 33, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(5, &cfg));

    uint16_t mv = 0xFFFF;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_mv(5, &mv));
    TEST_ASSERT_EQUAL_UINT16(0, mv);
}

void test_pal_adc_raw_mv_consistency_across_scale(void) {
    pal_adc_config_t cfg = { .pin = 34, .full_scale_mv = 3300, .resolution_bits = 12 };
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_init(6, &cfg));

    static const struct { uint16_t raw; uint16_t expect_mv; } cases[] = {
        { 0,    0   },
        { 2048, 1650 },
        { 4095, 3300 },
    };
    for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        pal_host_adc_inject_raw(6, cases[i].raw);
        uint16_t raw = 0, mv = 0, raw2 = 0;
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(6, &raw));
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_mv(6, &mv));
        TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_read_raw(6, &raw2));
        TEST_ASSERT_EQUAL_UINT16(cases[i].raw, raw);
        TEST_ASSERT_EQUAL_UINT16(cases[i].expect_mv, mv);
        TEST_ASSERT_EQUAL_UINT16(cases[i].raw, raw2);
    }
}

void test_pal_adc_dual_resource_claim(void) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_ADC_CHANNEL, 0, "dal_analog_knob_0"));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 36, "dal_analog_knob_0"));

    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 36, "dal_button_0"));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_BUSY, pal_resource_claim(PAL_RESOURCE_ADC_CHANNEL, 0, "dal_sensor_1"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pal_adc_init_invalid_args);
    RUN_TEST(test_pal_adc_uninitialized_reads);
    RUN_TEST(test_pal_adc_init_deinit_reinit);
    RUN_TEST(test_pal_adc_pin_channel_mapping);
    RUN_TEST(test_pal_adc_inject_raw_and_read);
    RUN_TEST(test_pal_adc_inject_mv_and_read);
    RUN_TEST(test_pal_adc_zero_value_cache_contract);
    RUN_TEST(test_pal_adc_read_mv_before_read_raw_returns_zero_not_timeout);
    RUN_TEST(test_pal_adc_raw_mv_consistency_across_scale);
    RUN_TEST(test_pal_adc_dual_resource_claim);
    return UNITY_END();
}
