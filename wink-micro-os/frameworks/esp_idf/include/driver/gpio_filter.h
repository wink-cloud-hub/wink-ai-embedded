/* SPDX-License-Identifier: LGPL-3.0-only */
/* Harvested from esp-idf v6.1@fff9895c (Apache-2.0) — factual C-ABI only, comments stripped. See NOTICE. */
/* AUTOMATICALLY GENERATED FILE - DO NOT EDIT MANUALLY! Manifest: 542eb37a5dc3604c Source: v6.1@fff9895c Config: esp32/v6.1 */
#ifndef WINK_H_GUARD_DRIVER_GPIO_FILTER_H
#define WINK_H_GUARD_DRIVER_GPIO_FILTER_H
#ifndef __WINK_HARVESTED_DRIVER_GPIO_FILTER_H__
#define __WINK_HARVESTED_DRIVER_GPIO_FILTER_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_sla.h"
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif
/* --- Macros (object + allowlisted function-like; first-wins + #ifndef wrapped) --- */

/* --- Enums (implicit values backfilled; ABI attrs preserved) --- */

/* --- Structs & Typedefs (merged, source order preserved for C forward refs) --- */
typedef struct gpio_glitch_filter_t * gpio_glitch_filter_handle_t;
typedef struct {
    glitch_filter_clock_source_t clk_src;
    gpio_num_t gpio_num;
} gpio_pin_glitch_filter_config_t;
typedef struct {
    glitch_filter_clock_source_t clk_src;
    gpio_num_t gpio_num;
    uint32_t window_width_ns;
    uint32_t window_thres_ns;
} gpio_flex_glitch_filter_config_t;

esp_err_t gpio_del_glitch_filter(gpio_glitch_filter_handle_t filter);
esp_err_t gpio_glitch_filter_disable(gpio_glitch_filter_handle_t filter);
esp_err_t gpio_glitch_filter_enable(gpio_glitch_filter_handle_t filter);
esp_err_t gpio_new_flex_glitch_filter(const gpio_flex_glitch_filter_config_t *config, gpio_glitch_filter_handle_t *ret_filter);
esp_err_t gpio_new_pin_glitch_filter(const gpio_pin_glitch_filter_config_t *config, gpio_glitch_filter_handle_t *ret_filter);


#ifdef __cplusplus
}
#endif
#endif /* __WINK_HARVESTED_DRIVER_GPIO_FILTER_H__ */
#endif /* WINK_H_GUARD_DRIVER_GPIO_FILTER_H */
