// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_load_cell.c
 * @brief DAL load cell weight sensor driver unit tests.
 */
#include "unity.h"
#include "wink_status.h"
#include "sensor/dal_load_cell.h"
#include "pal_resource.h"
#include "host_test_ctrl.h"
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4996)
#endif

static const char *const OWNER = "test_dal_load_cell";

void setUp(void) {
    sim_reset_time();
    pal_resource_reset();
}

void tearDown(void) {}

void test_load_cell_init_null_returns_invalid_arg(void) {
    const dal_load_cell_config_t cfg = {
        .owner = OWNER,
        .variant = DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE,
        .dt_pin = 4,
        .sck_pin = 5,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_load_cell_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_load_cell_init(NULL, NULL));
}

void test_load_cell_init_same_pin_returns_invalid_arg(void) {
    dal_load_cell_t dev = {0};
    const dal_load_cell_config_t cfg = {
        .owner = OWNER,
        .variant = DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE,
        .dt_pin = 5,
        .sck_pin = 5,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_load_cell_init(&dev, &cfg));
}

void test_load_cell_init_unsupported_variant_returns_unsupported(void) {
    dal_load_cell_t dev = {0};
    const dal_load_cell_config_t cfg = {
        .owner = OWNER,
        .variant = DAL_LOAD_CELL_VARIANT_CS1237_TWO_WIRE,
        .out_in_pin = 4,
        .sclk_pin = 5,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_UNSUPPORTED, dal_load_cell_init(&dev, &cfg));
}

void test_load_cell_read_before_init_returns_not_initialized(void) {
    dal_load_cell_t dev = {0};
    float weight = 0.0f;
    int32_t raw = 0;
    bool ready = false;

    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_load_cell_is_data_ready(&dev, &ready));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_load_cell_request_read(&dev));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_load_cell_get_cached_raw(&dev, &raw));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_load_cell_get_cached_weight_g(&dev, &weight));
#ifndef WINK_STRICT_NONBLOCKING
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_load_cell_read_weight_g(&dev, &weight));
#endif
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_load_cell_tare(&dev));
}

void test_load_cell_already_initialized_returns_error(void) {
    dal_load_cell_t dev = {0};
    const dal_load_cell_config_t cfg = {
        .owner = OWNER,
        .variant = DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE,
        .dt_pin = 4,
        .sck_pin = 5,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_load_cell_init(&dev, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_ALREADY_INITIALIZED, dal_load_cell_init(&dev, &cfg));
    dal_load_cell_deinit(&dev);
}

void test_load_cell_set_calibration_factor_and_weight_calc(void) {
    dal_load_cell_t dev = {0};
    const dal_load_cell_config_t cfg = {
        .owner = OWNER,
        .variant = DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE,
        .dt_pin = 4,
        .sck_pin = 5,
        .calibration_factor = 100.0f,
        .zero_offset = 1000,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_load_cell_init(&dev, &cfg));

    /* Mock raw reading = 2100 -> weight_g = (2100 - 1000) / 100.0 = 11.0g */
    dev.last_raw = 2100;
    float weight = 0.0f;
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_load_cell_set_calibration_factor(&dev, 100.0f));
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_load_cell_get_cached_weight_g(&dev, &weight));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 11.0f, weight);

    dal_load_cell_deinit(&dev);
}

void test_load_cell_deinit_clears_initialized_flag(void) {
    dal_load_cell_t dev = {0};
    const dal_load_cell_config_t cfg = {
        .owner = OWNER,
        .variant = DAL_LOAD_CELL_VARIANT_HX711_TWO_WIRE,
        .dt_pin = 4,
        .sck_pin = 5,
    };
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_load_cell_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.initialized);
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_load_cell_deinit(&dev));
    TEST_ASSERT_FALSE(dev.initialized);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_load_cell_init_null_returns_invalid_arg);
    RUN_TEST(test_load_cell_init_same_pin_returns_invalid_arg);
    RUN_TEST(test_load_cell_init_unsupported_variant_returns_unsupported);
    RUN_TEST(test_load_cell_read_before_init_returns_not_initialized);
    RUN_TEST(test_load_cell_already_initialized_returns_error);
    RUN_TEST(test_load_cell_set_calibration_factor_and_weight_calc);
    RUN_TEST(test_load_cell_deinit_clears_initialized_flag);
    return UNITY_END();
}
