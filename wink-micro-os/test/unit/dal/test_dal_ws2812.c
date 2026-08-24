// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_ws2812.c
 * @brief Unit tests for DAL WS2812 / SK6812 RGB LED driver.
 */
#include "unity.h"
#include "output/dal_ws2812.h"
#include "pal_resource.h"
#include <string.h>

void setUp(void) {
    pal_resource_reset();
}

void tearDown(void) {
    pal_resource_reset();
}

void test_ws2812_init_and_deinit(void) {
    dal_ws2812_t dev = {0};
    dal_ws2812_config_t cfg = {
        .pin = 18,
        .num_leds = 8,
        .variant = DAL_WS2812_VARIANT_WS2812,
    };

    /* Invalid args */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ws2812_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ws2812_init(&dev, NULL));

    dal_ws2812_config_t bad_cfg = cfg;
    bad_cfg.num_leds = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ws2812_init(&dev, &bad_cfg));

    /* Valid init */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ws2812_init(&dev, &cfg));
    TEST_ASSERT_TRUE(dev.is_initialized);

    /* Valid deinit */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ws2812_deinit(&dev));
    TEST_ASSERT_FALSE(dev.is_initialized);
}

void test_ws2812_write(void) {
    dal_ws2812_t dev = {0};
    dal_ws2812_config_t cfg = {
        .pin = 18,
        .num_leds = 3,
        .variant = DAL_WS2812_VARIANT_WS2812,
    };

    dal_ws2812_color_t colors[3] = {
        { .r = 255, .g = 0,   .b = 0 },   /* Red */
        { .r = 0,   .g = 255, .b = 0 },   /* Green */
        { .r = 0,   .g = 0,   .b = 255 }, /* Blue */
    };

    /* Write before init */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_NOT_INITIALIZED, dal_ws2812_write(&dev, colors, 3));

    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ws2812_init(&dev, &cfg));

    /* Write with NULL pixels */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ws2812_write(&dev, NULL, 3));

    /* Write count > num_leds */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, dal_ws2812_write(&dev, colors, 4));

    /* Valid write */
    TEST_ASSERT_EQUAL_INT(WINK_OK, dal_ws2812_write(&dev, colors, 3));

    dal_ws2812_deinit(&dev);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ws2812_init_and_deinit);
    RUN_TEST(test_ws2812_write);
    return UNITY_END();
}
