// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_ch4_buffer.c
 * @brief Wasm target Axis A (CH4) Buffer Payload / WS2812 Framebuffer implementation.
 */

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "pal_hal.h"
#include "pal_resource.h"
#include "wasm_bridge.h"
#include "pal_wasm_internal.h"

wink_status_t pal_ws2812_write(wink_pin_t pin, const uint8_t *rgb_buf, size_t num_leds)
{
    if (pin < 0 || pin >= WASM_SIM_MAX_PINS || rgb_buf == NULL || num_leds == 0) {
        return WINK_ERR_INVALID_ARG;
    }
    if (!pal_resource_is_claimed(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin)) {
        return WINK_ERR_INVALID_STATE;
    }

    uint32_t payload_len = (uint32_t)(num_leds * 3u);
    js_pal_ws2812_write((uint16_t)pin, rgb_buf, payload_len);
    return WINK_OK;
}

void pal_wasm_ch4_buffer_reset(void)
{
    /* Reset state if any */
}
