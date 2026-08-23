// SPDX-License-Identifier: Apache-2.0
/**
 * @file dal_ws2812.c
 * @brief DAL WS2812 addressable RGB LED driver implementation.
 */
#include "output/dal_ws2812.h"
#include "hal/pal_rmt.h"
#include "pal_resource.h"
#include <string.h>

#define WS2812_MAX_LOCAL_LEDS 64

WINK_WARN_UNUSED_RESULT
wink_status_t dal_ws2812_init(dal_ws2812_t *dev, const dal_ws2812_config_t *config) {
    if (dev == NULL || config == NULL || config->pin < 0 || config->num_leds == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)config->pin, "dal_ws2812");
    if (st != WINK_OK) {
        return st;
    }

    dev->is_initialized = true;
    dev->config = *config;
    return WINK_OK;
}

wink_status_t dal_ws2812_deinit(dal_ws2812_t *dev) {
    if (dev == NULL || !dev->is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)dev->config.pin, "dal_ws2812");
    dev->is_initialized = false;
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t dal_ws2812_write(dal_ws2812_t *dev, const dal_ws2812_color_t *pixels, uint16_t count) {
    if (dev == NULL || !dev->is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }
    if (pixels == NULL || count == 0 || count > dev->config.num_leds) {
        return WINK_ERR_INVALID_ARG;
    }

    uint8_t grb_buffer[WS2812_MAX_LOCAL_LEDS * 3];
    uint16_t active_count = (count > WS2812_MAX_LOCAL_LEDS) ? WS2812_MAX_LOCAL_LEDS : count;

    for (uint16_t i = 0; i < active_count; i++) {
        grb_buffer[i * 3 + 0] = pixels[i].g;
        grb_buffer[i * 3 + 1] = pixels[i].r;
        grb_buffer[i * 3 + 2] = pixels[i].b;
    }

    return pal_rmt_ws2812_write(dev->config.pin, grb_buffer, (size_t)(active_count * 3));
}
