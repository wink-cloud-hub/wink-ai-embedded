// SPDX-License-Identifier: Apache-2.0
/**
 * @file dal_ws2812.c
 * @brief DAL WS2812 / SK6812 addressable RGB LED driver implementation.
 */
#include "output/dal_ws2812.h"
#include "hal/pal_rmt.h"
#include "hal/pal_dma.h"
#include "pal_resource.h"
#include <string.h>

#define WS2812_RMT_RES_HZ 10000000 /* 10 MHz -> 100ns / tick */
#define WS2812_MAX_LEDS_STATIC 128
#define WS2812_MAX_SYMBOLS_STATIC (WS2812_MAX_LEDS_STATIC * 24 + 1)

static PAL_DMA_BUF_ATTR PAL_DMA_BUF_ALIGN pal_rmt_symbol_t s_sym_buf[WS2812_MAX_SYMBOLS_STATIC];

static inline void encode_byte(uint8_t byte, pal_rmt_symbol_t *out_syms) {
    for (int bit = 7; bit >= 0; bit--) {
        if (byte & (1 << bit)) {
            /* T1: 800ns High (8 ticks), 450ns Low (5 ticks) */
            out_syms[7 - bit].duration0_ticks = 8;
            out_syms[7 - bit].level0 = 1;
            out_syms[7 - bit].duration1_ticks = 5;
            out_syms[7 - bit].level1 = 0;
        } else {
            /* T0: 350ns High (4 ticks), 900ns Low (9 ticks) */
            out_syms[7 - bit].duration0_ticks = 4;
            out_syms[7 - bit].level0 = 1;
            out_syms[7 - bit].duration1_ticks = 9;
            out_syms[7 - bit].level1 = 0;
        }
        out_syms[7 - bit]._pad[0] = 0;
        out_syms[7 - bit]._pad[1] = 0;
    }
}

WINK_WARN_UNUSED_RESULT
wink_status_t dal_ws2812_init(dal_ws2812_t *dev, const dal_ws2812_config_t *config) {
    if (dev == NULL || config == NULL || config->pin < 0 || config->num_leds == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_rmt_channel_config_t rmt_cfg = {
        .pin = config->pin,
        .direction = PAL_RMT_DIR_TX,
        .resolution_hz = WS2812_RMT_RES_HZ,
        .mem_block_symbols = 64,
        .dma_enabled = true,
        .max_symbols = (uint32_t)(config->num_leds * 24 + 1),
    };

    pal_rmt_channel_handle_t chan = NULL;
    wink_status_t st = pal_rmt_acquire_channel(&rmt_cfg, &chan);
    if (st != WINK_OK) {
        return st;
    }

    dev->is_initialized = true;
    dev->config = *config;
    dev->rmt_chan = chan;
    return WINK_OK;
}

wink_status_t dal_ws2812_deinit(dal_ws2812_t *dev) {
    if (dev == NULL || !dev->is_initialized) {
        return WINK_ERR_NOT_INITIALIZED;
    }

    if (dev->rmt_chan != NULL) {
        pal_rmt_release_channel(dev->rmt_chan);
        dev->rmt_chan = NULL;
    }

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

    uint16_t active_count = (count > WS2812_MAX_LEDS_STATIC) ? WS2812_MAX_LEDS_STATIC : count;
    size_t sym_idx = 0;

    for (uint16_t i = 0; i < active_count; i++) {
        /* WS2812 order: G -> R -> B */
        encode_byte(pixels[i].g, &s_sym_buf[sym_idx]);
        sym_idx += 8;
        encode_byte(pixels[i].r, &s_sym_buf[sym_idx]);
        sym_idx += 8;
        encode_byte(pixels[i].b, &s_sym_buf[sym_idx]);
        sym_idx += 8;
    }

    /* Reset pulse: >= 50us (500 ticks) LOW */
    s_sym_buf[sym_idx] = pal_rmt_make_reset_symbol(WS2812_RMT_RES_HZ, 60);
    sym_idx++;

    return pal_rmt_tx_send(dev->rmt_chan, s_sym_buf, sym_idx, NULL, NULL);
}

void dal_ws2812_safe_off(dal_ws2812_t *dev) {
    if (dev == NULL || !dev->is_initialized || dev->rmt_chan == NULL) {
        return;
    }
    uint16_t leds = (dev->config.num_leds > WS2812_MAX_LEDS_STATIC) ? WS2812_MAX_LEDS_STATIC : dev->config.num_leds;
    size_t sym_idx = 0;
    for (uint16_t i = 0; i < leds; i++) {
        encode_byte(0, &s_sym_buf[sym_idx]);
        sym_idx += 8;
        encode_byte(0, &s_sym_buf[sym_idx]);
        sym_idx += 8;
        encode_byte(0, &s_sym_buf[sym_idx]);
        sym_idx += 8;
    }
    s_sym_buf[sym_idx] = pal_rmt_make_reset_symbol(WS2812_RMT_RES_HZ, 60);
    sym_idx++;
    WINK_IGNORE_RESULT(pal_rmt_tx_send(dev->rmt_chan, s_sym_buf, sym_idx, NULL, NULL));
}
