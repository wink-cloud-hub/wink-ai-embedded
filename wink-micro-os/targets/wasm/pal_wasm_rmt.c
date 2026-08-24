// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_rmt.c
 * @brief Wasm target PAL RMT pulse transceiver soft-simulation subsystem.
 */
#include "hal/pal_rmt.h"
#include "wasm_bridge.h"
#include "pal_wasm_completion.h"
#include "pal_resource.h"
#include <string.h>

#define WASM_RMT_CHANNELS_MAX 4

struct pal_rmt_channel_s {
    bool                     in_use;
    uint8_t                  id;
    pal_rmt_channel_config_t cfg;
    pal_rmt_tx_callback_t    tx_cb;
    void                    *tx_cb_arg;
    pal_rmt_rx_callback_t    rx_cb;
    void                    *rx_cb_arg;
    bool                     rx_active;
};

static struct pal_rmt_channel_s s_channels[WASM_RMT_CHANNELS_MAX];

WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_acquire_channel(const pal_rmt_channel_config_t *cfg,
                                      pal_rmt_channel_handle_t *out_ch) {
    if (cfg == NULL || out_ch == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    struct pal_rmt_channel_s *slot = NULL;
    for (uint8_t i = 0; i < WASM_RMT_CHANNELS_MAX; i++) {
        if (!s_channels[i].in_use) {
            slot = &s_channels[i];
            slot->id = i;
            break;
        }
    }
    if (slot == NULL) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    slot->in_use = true;
    slot->cfg = *cfg;
    if (slot->cfg.resolution_hz == 0) {
        slot->cfg.resolution_hz = 10000000; /* 10 MHz default -> 100ns tick */
    }
    slot->tx_cb = NULL;
    slot->tx_cb_arg = NULL;
    slot->rx_cb = NULL;
    slot->rx_cb_arg = NULL;
    slot->rx_active = false;

    *out_ch = slot;
    return WINK_OK;
}

wink_status_t pal_rmt_release_channel(pal_rmt_channel_handle_t ch) {
    if (ch == NULL || !ch->in_use) {
        return WINK_ERR_INVALID_ARG;
    }
    ch->in_use = false;
    ch->rx_active = false;
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_tx_send(pal_rmt_channel_handle_t ch,
                              const pal_rmt_symbol_t *symbols,
                              size_t count,
                              pal_rmt_tx_callback_t cb,
                              void *arg) {
    if (ch == NULL || !ch->in_use || ch->cfg.direction != PAL_RMT_DIR_TX ||
        symbols == NULL || count == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    /* 1. Forward symbol stream to JS engine */
    js_pal_rmt_tx(ch->id, symbols, (uint32_t)count, ch->cfg.resolution_hz);

    /* 2. Calculate symbol transmission duration in microseconds */
    uint64_t total_ticks = 0;
    for (size_t i = 0; i < count; i++) {
        total_ticks += symbols[i].duration0_ticks + symbols[i].duration1_ticks;
    }

    uint32_t res_hz = ch->cfg.resolution_hz;
    uint32_t duration_us = (uint32_t)((total_ticks * 1000000ULL + res_hz - 1) / res_hz);
    if (duration_us == 0) duration_us = 1;

    /* 3. Schedule completion callback */
    if (cb != NULL) {
        return pal_wasm_schedule_complete_us(duration_us, (pal_wasm_completion_cb_t)cb, arg);
    }

    return WINK_OK;
}

wink_status_t pal_rmt_rx_set_callback(pal_rmt_channel_handle_t ch,
                                      pal_rmt_rx_callback_t cb,
                                      void *arg) {
    if (ch == NULL || !ch->in_use || ch->cfg.direction != PAL_RMT_DIR_RX) {
        return WINK_ERR_INVALID_ARG;
    }
    ch->rx_cb = cb;
    ch->rx_cb_arg = arg;
    return WINK_OK;
}

wink_status_t pal_rmt_rx_start(pal_rmt_channel_handle_t ch) {
    if (ch == NULL || !ch->in_use || ch->cfg.direction != PAL_RMT_DIR_RX) {
        return WINK_ERR_INVALID_ARG;
    }
    ch->rx_active = true;
    return WINK_OK;
}

wink_status_t pal_rmt_rx_stop(pal_rmt_channel_handle_t ch) {
    if (ch == NULL || !ch->in_use || ch->cfg.direction != PAL_RMT_DIR_RX) {
        return WINK_ERR_INVALID_ARG;
    }
    ch->rx_active = false;
    return WINK_OK;
}

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#ifndef EMSCRIPTEN_KEEPALIVE
#define EMSCRIPTEN_KEEPALIVE
#endif

EMSCRIPTEN_KEEPALIVE
void pal_wasm_inject_rmt_rx(uint8_t channel_id, const pal_rmt_symbol_t *symbols, uint32_t count) {
    if (channel_id < WASM_RMT_CHANNELS_MAX && symbols != NULL && count > 0) {
        struct pal_rmt_channel_s *ch = &s_channels[channel_id];
        if (ch->in_use && ch->cfg.direction == PAL_RMT_DIR_RX && ch->rx_active && ch->rx_cb != NULL) {
            ch->rx_cb(ch->rx_cb_arg, symbols, (size_t)count);
        }
    }
}

/* ========================================================================= */
/* Legacy Pulse-Capture Stubs for Wasm                                       */
/* ========================================================================= */

static bool s_pulse_capture_active = false;

WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_init(wink_pin_t pin, pal_rmt_edge_t start_edge) {
    (void)pin;
    (void)start_edge;
    s_pulse_capture_active = true;
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_pulse_capture_arm(void) {
    if (!s_pulse_capture_active) return WINK_ERR_INVALID_STATE;
    return WINK_OK;
}

void pal_rmt_pulse_capture_deinit(void) {
    s_pulse_capture_active = false;
}

bool pal_rmt_pulse_capture_is_active(void) {
    return s_pulse_capture_active;
}
