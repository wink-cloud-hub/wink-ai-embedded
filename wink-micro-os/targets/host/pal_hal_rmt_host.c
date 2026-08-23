// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_rmt_host.c
 * @brief Host first-class target PAL RMT multi-channel pulse transceiver implementation.
 */
#include "hal/pal_rmt.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_rmt_stub.h"
#include <string.h>

#define HOST_RMT_MAX_TX_SYMBOLS 512

struct pal_rmt_channel_s {
    bool                     in_use;
    uint8_t                  id;
    pal_rmt_channel_config_t cfg;
    bool                     rx_active;
    pal_rmt_tx_callback_t    tx_cb;
    void                    *tx_cb_arg;
    pal_rmt_rx_callback_t    rx_cb;
    void                    *rx_cb_arg;
    pal_rmt_symbol_t         last_tx[HOST_RMT_MAX_TX_SYMBOLS];
    size_t                   last_tx_count;
    wink_status_t            forced_err;
};

static struct pal_rmt_channel_s s_rmt_channels[PAL_RMT_CHAN_MAX];
static pal_spinlock_t s_rmt_lock = PAL_SPINLOCK_INITIALIZER;

/* --- Testing Stub Control Hooks --- */

void stub_rmt_inject_rx(pal_rmt_channel_handle_t ch, const pal_rmt_symbol_t *symbols, size_t count) {
    if (ch == NULL || symbols == NULL || count == 0) {
        return;
    }
    pal_spinlock_lock(&s_rmt_lock);
    if (ch->in_use && ch->cfg.direction == PAL_RMT_DIR_RX && ch->rx_active) {
        pal_rmt_rx_callback_t cb = ch->rx_cb;
        void *arg = ch->rx_cb_arg;
        pal_spinlock_unlock(&s_rmt_lock);
        if (cb != NULL) {
            cb(arg, symbols, count);
        }
        return;
    }
    pal_spinlock_unlock(&s_rmt_lock);
}

void stub_rmt_get_last_tx(pal_rmt_channel_handle_t ch, pal_rmt_symbol_t *out_symbols, size_t *out_count) {
    if (ch == NULL || out_symbols == NULL || out_count == NULL) {
        return;
    }
    pal_spinlock_lock(&s_rmt_lock);
    size_t copy_cnt = ch->last_tx_count;
    if (copy_cnt > *out_count) {
        copy_cnt = *out_count;
    }
    if (copy_cnt > 0) {
        memcpy(out_symbols, ch->last_tx, copy_cnt * sizeof(pal_rmt_symbol_t));
    }
    *out_count = ch->last_tx_count;
    pal_spinlock_unlock(&s_rmt_lock);
}

void stub_rmt_force_failure(pal_rmt_channel_handle_t ch, wink_status_t err) {
    if (ch == NULL) {
        return;
    }
    pal_spinlock_lock(&s_rmt_lock);
    ch->forced_err = err;
    pal_spinlock_unlock(&s_rmt_lock);
}

void stub_rmt_reset(pal_rmt_channel_handle_t ch) {
    if (ch == NULL) {
        return;
    }
    pal_spinlock_lock(&s_rmt_lock);
    ch->last_tx_count = 0;
    ch->forced_err = WINK_OK;
    pal_spinlock_unlock(&s_rmt_lock);
}

/* --- PAL RMT Public Multi-Channel API --- */

WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_acquire_channel(const pal_rmt_channel_config_t *cfg,
                                      pal_rmt_channel_handle_t *out_ch) {
    if (cfg == NULL || out_ch == NULL || cfg->pin < 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_rmt_lock);

    /* Find free hardware channel */
    struct pal_rmt_channel_s *slot = NULL;
    for (uint8_t i = 0; i < PAL_RMT_CHAN_MAX; i++) {
        if (!s_rmt_channels[i].in_use) {
            slot = &s_rmt_channels[i];
            slot->id = i;
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_rmt_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    /* Claim channel and pin */
    wink_status_t st = pal_resource_claim(PAL_RESOURCE_RMT_CHAN, slot->id, "pal_rmt_host");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_rmt_lock);
        return st;
    }

    st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin, "pal_rmt_host");
    if (st != WINK_OK) {
        pal_resource_release(PAL_RESOURCE_RMT_CHAN, slot->id, "pal_rmt_host");
        pal_spinlock_unlock(&s_rmt_lock);
        return st;
    }

    slot->in_use = true;
    slot->cfg = *cfg;
    if (slot->cfg.resolution_hz == 0) {
        slot->cfg.resolution_hz = 10000000; /* Default 10 MHz */
    }
    slot->rx_active = false;
    slot->tx_cb = NULL;
    slot->tx_cb_arg = NULL;
    slot->rx_cb = NULL;
    slot->rx_cb_arg = NULL;
    slot->last_tx_count = 0;
    slot->forced_err = WINK_OK;

    *out_ch = slot;
    pal_spinlock_unlock(&s_rmt_lock);
    return WINK_OK;
}

wink_status_t pal_rmt_release_channel(pal_rmt_channel_handle_t ch) {
    if (ch == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_rmt_lock);
    if (!ch->in_use || ch->id >= PAL_RMT_CHAN_MAX) {
        pal_spinlock_unlock(&s_rmt_lock);
        return WINK_ERR_INVALID_ARG;
    }

    pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)ch->cfg.pin, "pal_rmt_host");
    pal_resource_release(PAL_RESOURCE_RMT_CHAN, ch->id, "pal_rmt_host");

    ch->in_use = false;
    ch->rx_active = false;

    pal_spinlock_unlock(&s_rmt_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_tx_send(pal_rmt_channel_handle_t ch,
                              const pal_rmt_symbol_t *symbols,
                              size_t count,
                              pal_rmt_tx_callback_t cb,
                              void *arg) {
    if (ch == NULL || symbols == NULL || count == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_rmt_lock);
    if (!ch->in_use || ch->cfg.direction != PAL_RMT_DIR_TX) {
        pal_spinlock_unlock(&s_rmt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    if (ch->forced_err != WINK_OK) {
        wink_status_t err = ch->forced_err;
        ch->forced_err = WINK_OK;
        pal_spinlock_unlock(&s_rmt_lock);
        if (cb != NULL) {
            cb(arg, err);
        }
        return err;
    }

    size_t copy_cnt = (count > HOST_RMT_MAX_TX_SYMBOLS) ? HOST_RMT_MAX_TX_SYMBOLS : count;
    memcpy(ch->last_tx, symbols, copy_cnt * sizeof(pal_rmt_symbol_t));
    ch->last_tx_count = copy_cnt;

    pal_spinlock_unlock(&s_rmt_lock);

    if (cb != NULL) {
        cb(arg, WINK_OK);
    }
    return WINK_OK;
}

wink_status_t pal_rmt_rx_set_callback(pal_rmt_channel_handle_t ch,
                                      pal_rmt_rx_callback_t cb,
                                      void *arg) {
    if (ch == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_rmt_lock);
    if (!ch->in_use || ch->cfg.direction != PAL_RMT_DIR_RX) {
        pal_spinlock_unlock(&s_rmt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    ch->rx_cb = cb;
    ch->rx_cb_arg = arg;
    pal_spinlock_unlock(&s_rmt_lock);
    return WINK_OK;
}

wink_status_t pal_rmt_rx_start(pal_rmt_channel_handle_t ch) {
    if (ch == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_rmt_lock);
    if (!ch->in_use || ch->cfg.direction != PAL_RMT_DIR_RX) {
        pal_spinlock_unlock(&s_rmt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    ch->rx_active = true;
    pal_spinlock_unlock(&s_rmt_lock);
    return WINK_OK;
}

wink_status_t pal_rmt_rx_stop(pal_rmt_channel_handle_t ch) {
    if (ch == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_rmt_lock);
    if (!ch->in_use) {
        pal_spinlock_unlock(&s_rmt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    ch->rx_active = false;
    pal_spinlock_unlock(&s_rmt_lock);
    return WINK_OK;
}
