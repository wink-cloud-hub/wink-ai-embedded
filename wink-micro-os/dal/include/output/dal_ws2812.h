// SPDX-License-Identifier: Apache-2.0
/**
 * @file dal_ws2812.h
 * @brief DAL WS2812 addressable RGB LED driver interface.
 */

#ifndef DAL_WS2812_H
#define DAL_WS2812_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
#include "hal/pal_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} dal_ws2812_color_t;

typedef struct {
    wink_pin_t pin;
    uint16_t   num_leds;
} dal_ws2812_config_t;

typedef struct dal_ws2812_s {
    bool                is_initialized;
    dal_ws2812_config_t config;
} dal_ws2812_t;

WINK_WARN_UNUSED_RESULT
wink_status_t dal_ws2812_init(dal_ws2812_t *dev, const dal_ws2812_config_t *config);

wink_status_t dal_ws2812_deinit(dal_ws2812_t *dev);

WINK_WARN_UNUSED_RESULT
wink_status_t dal_ws2812_write(dal_ws2812_t *dev, const dal_ws2812_color_t *pixels, uint16_t count);

#ifdef __cplusplus
}
#endif

#endif /* DAL_WS2812_H */
