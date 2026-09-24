/* SPDX-License-Identifier: LGPL-3.0-only */
#ifndef LED_STRIP_H
#define LED_STRIP_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *led_strip_handle_t;

typedef struct {
    uint32_t strip_gpio_num;
    uint32_t max_leds;
    uint8_t led_pixel_format;
    uint8_t led_model;
    struct {
        uint32_t invert_out: 1;
    } flags;
} led_strip_config_t;

typedef struct {
    uint32_t clk_src;
    uint32_t resolution_hz;
    struct {
        uint32_t with_dma: 1;
    } flags;
} led_strip_rmt_config_t;

esp_err_t led_strip_new_rmt_device(const led_strip_config_t *config, const led_strip_rmt_config_t *rmt_config, led_strip_handle_t *ret_strip);
esp_err_t led_strip_set_pixel(led_strip_handle_t strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue);
esp_err_t led_strip_refresh(led_strip_handle_t strip);
esp_err_t led_strip_clear(led_strip_handle_t strip);
esp_err_t led_strip_del(led_strip_handle_t strip);

#ifdef __cplusplus
}
#endif

#endif /* LED_STRIP_H */
