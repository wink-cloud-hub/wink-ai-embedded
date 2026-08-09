// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_ws2812_sim.c
 * @brief Unit tests for Wasm Target Channel 4 WS2812 Framebuffer simulation path.
 */
#include "unity.h"
#include "wink_status.h"
#include "hal/pal_gpio.h"
#include "pal_resource.h"
#include <stdint.h>

extern wink_status_t pal_ws2812_write(wink_pin_t pin, const uint8_t *rgb_buf, size_t num_leds);

#ifndef __EMSCRIPTEN__
void js_pal_ws2812_write(uint16_t pin, const uint8_t *buf, uint32_t len)
{
    (void)pin;
    (void)buf;
    (void)len;
}
#endif

void setUp(void)
{
    pal_resource_reset();
    pal_gpio_init(12, PAL_GPIO_OUTPUT);
    pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 12, "ws2812_test");
}

void tearDown(void)
{
    pal_resource_release(PAL_RESOURCE_GPIO_PIN, 12, "ws2812_test");
    pal_resource_reset();
}

void test_ws2812_write_valid_buffer(void)
{
    uint8_t rgb_data[9] = {
        255, 0, 0,    /* LED 0: Red */
        0, 255, 0,    /* LED 1: Green */
        0, 0, 255     /* LED 2: Blue */
    };

    wink_status_t st = pal_ws2812_write(12, rgb_data, 3);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
}

void test_ws2812_write_invalid_unclaimed_pin(void)
{
    uint8_t rgb_data[3] = {255, 255, 255};
    wink_status_t st = pal_ws2812_write(13, rgb_data, 1);
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_STATE, st);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ws2812_write_valid_buffer);
    RUN_TEST(test_ws2812_write_invalid_unclaimed_pin);
    return UNITY_END();
}
