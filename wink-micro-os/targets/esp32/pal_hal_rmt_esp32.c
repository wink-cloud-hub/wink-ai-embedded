#include "pal_hal.h"
#include "hal/pal_rmt.h"
#include "hal/pal_dma.h"
#include "osal/pal_deferred.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_log.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "soc/soc_caps.h"

#define LOG_TAG "pal_rmt"

#define RMT_MEM_BLOCK_SYMB_DEFAULT 64
#define RMT_MAX_SYMBOLS_BUFFER      256

_Static_assert(sizeof(pal_rmt_symbol_t) == sizeof(rmt_symbol_word_t),
               "pal_rmt_symbol_t size must match IDF rmt_symbol_word_t");

#if defined(CONFIG_IDF_TARGET_ESP32)
    /* Classic ESP32: use REF_TICK (1MHz) to avoid APB clock scaling breaking WS2812 timings */
    #define PAL_RMT_CLK_SRC RMT_CLK_SRC_REF_TICK
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
    #define PAL_RMT_CLK_SRC RMT_CLK_SRC_XTAL
#else
    #define PAL_RMT_CLK_SRC RMT_CLK_SRC_DEFAULT
#endif

struct pal_rmt_channel_s {
    bool                     in_use;
    uint8_t                  id;
    pal_rmt_channel_config_t cfg;
    rmt_channel_handle_t     chan_handle;
    rmt_encoder_handle_t     copy_encoder;
    pal_rmt_tx_callback_t    tx_cb;
    void                    *tx_cb_arg;
    pal_rmt_rx_callback_t    rx_cb;
    void                    *rx_cb_arg;
    bool                     rx_active;
    rmt_symbol_word_t        rx_symbols[RMT_MAX_SYMBOLS_BUFFER];
    pal_rmt_symbol_t         converted_rx[RMT_MAX_SYMBOLS_BUFFER];
    size_t                   rx_count;
};

static struct pal_rmt_channel_s s_channels[PAL_RMT_CHAN_MAX];
static pal_spinlock_t s_rmt_lock = PAL_SPINLOCK_INITIALIZER;

/* --- ESP-IDF RMT Callbacks --- */

static PAL_ISR bool esp32_rmt_tx_done_cb(rmt_channel_handle_t tx_chan,
                                         const rmt_tx_done_event_data_t *edata,
                                         void *user_data) {
    (void)tx_chan;
    (void)edata;
    struct pal_rmt_channel_s *ch = (struct pal_rmt_channel_s *)user_data;
    if (ch != NULL && ch->tx_cb != NULL) {
        pal_rmt_tx_callback_t cb = ch->tx_cb;
        void *arg = ch->tx_cb_arg;
        ch->tx_cb = NULL;
        ch->tx_cb_arg = NULL;
        cb(arg, WINK_OK);
    }
    return false;
}

static void esp32_rmt_rx_deferred_worker(void *arg) {
    struct pal_rmt_channel_s *ch = (struct pal_rmt_channel_s *)arg;
    if (ch != NULL && ch->rx_cb != NULL && ch->rx_active) {
        ch->rx_cb(ch->rx_cb_arg, ch->converted_rx, ch->rx_count);
    }
}

static PAL_ISR bool esp32_rmt_rx_done_cb(rmt_channel_handle_t rx_chan,
                                         const rmt_rx_done_event_data_t *edata,
                                         void *user_data) {
    (void)rx_chan;
    struct pal_rmt_channel_s *ch = (struct pal_rmt_channel_s *)user_data;
    if (ch != NULL && ch->rx_cb != NULL && ch->rx_active && edata != NULL) {
        size_t count = edata->num_symbols;
        if (count > RMT_MAX_SYMBOLS_BUFFER) {
            count = RMT_MAX_SYMBOLS_BUFFER;
        }
        for (size_t i = 0; i < count; i++) {
            ch->converted_rx[i].duration0_ticks = edata->received_symbols[i].duration0;
            ch->converted_rx[i].level0 = (uint8_t)edata->received_symbols[i].level0;
            ch->converted_rx[i].duration1_ticks = edata->received_symbols[i].duration1;
            ch->converted_rx[i].level1 = (uint8_t)edata->received_symbols[i].level1;
            ch->converted_rx[i]._pad[0] = 0;
            ch->converted_rx[i]._pad[1] = 0;
        }
        ch->rx_count = count;
        pal_deferred_post_from_isr(PAL_DEFERRED_LO, PAL_DEFERRED_LOSSY,
                                   esp32_rmt_rx_deferred_worker, ch);
    }
    return false;
}

/* --- PAL Multi-Channel RMT API --- */

WINK_WARN_UNUSED_RESULT
wink_status_t pal_rmt_acquire_channel(const pal_rmt_channel_config_t *cfg,
                                      pal_rmt_channel_handle_t *out_ch) {
    if (cfg == NULL || out_ch == NULL || cfg->pin < 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_rmt_lock);

    struct pal_rmt_channel_s *slot = NULL;
    for (uint8_t i = 0; i < PAL_RMT_CHAN_MAX; i++) {
        if (!s_channels[i].in_use) {
            slot = &s_channels[i];
            slot->id = i;
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_rmt_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_RMT_CHAN, slot->id, "pal_rmt_esp32");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_rmt_lock);
        return st;
    }

    st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin, "pal_rmt_esp32");
    if (st != WINK_OK) {
        pal_resource_release(PAL_RESOURCE_RMT_CHAN, slot->id, "pal_rmt_esp32");
        pal_spinlock_unlock(&s_rmt_lock);
        return st;
    }

    uint32_t res_hz = cfg->resolution_hz > 0 ? cfg->resolution_hz : 10000000;
    size_t mem_syms = cfg->mem_block_symbols > 0 ? cfg->mem_block_symbols : RMT_MEM_BLOCK_SYMB_DEFAULT;

    if (cfg->direction == PAL_RMT_DIR_TX) {
        rmt_tx_channel_config_t tx_cfg = {
            .gpio_num = cfg->pin,
            .clk_src = PAL_RMT_CLK_SRC,
            .resolution_hz = res_hz,
            .mem_block_symbols = mem_syms,
            .trans_queue_depth = 4,
            .intr_flags = ESP_INTR_FLAG_IRAM,
        };
#if defined(CONFIG_SOC_RMT_SUPPORT_DMA) && CONFIG_SOC_RMT_SUPPORT_DMA
        tx_cfg.flags.with_dma = cfg->dma_enabled;
#endif
        esp_err_t err = rmt_new_tx_channel(&tx_cfg, &slot->chan_handle);
        if (err != ESP_OK) {
            pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin, "pal_rmt_esp32");
            pal_resource_release(PAL_RESOURCE_RMT_CHAN, slot->id, "pal_rmt_esp32");
            pal_spinlock_unlock(&s_rmt_lock);
            return WINK_ERR_HARDWARE;
        }

        rmt_copy_encoder_config_t enc_cfg = {};
        err = rmt_new_copy_encoder(&enc_cfg, &slot->copy_encoder);
        if (err != ESP_OK) {
            rmt_del_channel(slot->chan_handle);
            pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin, "pal_rmt_esp32");
            pal_resource_release(PAL_RESOURCE_RMT_CHAN, slot->id, "pal_rmt_esp32");
            pal_spinlock_unlock(&s_rmt_lock);
            return WINK_ERR_HARDWARE;
        }

        rmt_tx_event_callbacks_t cbs = {
            .on_trans_done = esp32_rmt_tx_done_cb,
        };
        rmt_tx_register_event_callbacks(slot->chan_handle, &cbs, slot);
        rmt_enable(slot->chan_handle);
    } else {
        rmt_rx_channel_config_t rx_cfg = {
            .gpio_num = cfg->pin,
            .clk_src = PAL_RMT_CLK_SRC,
            .resolution_hz = res_hz,
            .mem_block_symbols = mem_syms,
            .intr_flags = ESP_INTR_FLAG_IRAM,
        };
#if defined(CONFIG_SOC_RMT_SUPPORT_DMA) && CONFIG_SOC_RMT_SUPPORT_DMA
        rx_cfg.flags.with_dma = cfg->dma_enabled;
#endif
        esp_err_t err = rmt_new_rx_channel(&rx_cfg, &slot->chan_handle);
        if (err != ESP_OK) {
            pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)cfg->pin, "pal_rmt_esp32");
            pal_resource_release(PAL_RESOURCE_RMT_CHAN, slot->id, "pal_rmt_esp32");
            pal_spinlock_unlock(&s_rmt_lock);
            return WINK_ERR_HARDWARE;
        }

        rmt_rx_event_callbacks_t cbs = {
            .on_recv_done = esp32_rmt_rx_done_cb,
        };
        rmt_rx_register_event_callbacks(slot->chan_handle, &cbs, slot);
        rmt_enable(slot->chan_handle);
        slot->copy_encoder = NULL;
    }

    slot->in_use = true;
    slot->cfg = *cfg;
    slot->tx_cb = NULL;
    slot->tx_cb_arg = NULL;
    slot->rx_cb = NULL;
    slot->rx_cb_arg = NULL;
    slot->rx_active = false;

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

    ch->rx_active = false;
    rmt_disable(ch->chan_handle);
    if (ch->copy_encoder != NULL) {
        rmt_del_encoder(ch->copy_encoder);
        ch->copy_encoder = NULL;
    }
    rmt_del_channel(ch->chan_handle);
    ch->chan_handle = NULL;

    pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)ch->cfg.pin, "pal_rmt_esp32");
    pal_resource_release(PAL_RESOURCE_RMT_CHAN, ch->id, "pal_rmt_esp32");

    ch->in_use = false;
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

    /* 1. Flush DMA cache for output symbol stream */
    pal_dma_cache_clean(symbols, count * sizeof(pal_rmt_symbol_t));

    /* 2. Fast critical section: update callback context and grab channel handle */
    pal_spinlock_lock(&s_rmt_lock);
    if (!ch->in_use || ch->cfg.direction != PAL_RMT_DIR_TX) {
        pal_spinlock_unlock(&s_rmt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    ch->tx_cb = cb;
    ch->tx_cb_arg = arg;
    rmt_channel_handle_t chan = ch->chan_handle;
    rmt_encoder_handle_t enc = ch->copy_encoder;
    pal_spinlock_unlock(&s_rmt_lock);

    /* 3. Transmit outside spinlock (zero-copy on S3+ GDMA / layout-compatible cast) */
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
    };
    esp_err_t err = rmt_transmit(chan, enc, (const rmt_symbol_word_t *)symbols,
                                 count * sizeof(rmt_symbol_word_t), &tx_cfg);
    if (err != ESP_OK) {
        pal_spinlock_lock(&s_rmt_lock);
        ch->tx_cb = NULL;
        ch->tx_cb_arg = NULL;
        pal_spinlock_unlock(&s_rmt_lock);
        return WINK_ERR_HARDWARE;
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
    rmt_receive_config_t recv_cfg = {
        .signal_range_min_ns = 1000,
        .signal_range_max_ns = 25000000,
    };
    esp_err_t err = rmt_receive(ch->chan_handle, ch->rx_symbols,
                                sizeof(ch->rx_symbols), &recv_cfg);
    pal_spinlock_unlock(&s_rmt_lock);
    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

wink_status_t pal_rmt_rx_stop(pal_rmt_channel_handle_t ch) {
    if (ch == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_rmt_lock);
    if (!ch->in_use || ch->cfg.direction != PAL_RMT_DIR_RX) {
        pal_spinlock_unlock(&s_rmt_lock);
        return WINK_ERR_INVALID_STATE;
    }

    ch->rx_active = false;
    rmt_disable(ch->chan_handle);
    rmt_enable(ch->chan_handle);
    pal_spinlock_unlock(&s_rmt_lock);
    return WINK_OK;
}

/* --- Legacy Pulse-Capture Singleton Backward Compatibility --- */

static pal_rmt_channel_handle_t s_legacy_pulse_chan = NULL;
static SemaphoreHandle_t        s_legacy_rx_done_sem = NULL;
static uint32_t                 s_legacy_last_pulse_us = 0;

static void legacy_rmt_rx_cb(void *arg, const pal_rmt_symbol_t *symbols, size_t count) {
    (void)arg;
    if (count > 0 && symbols != NULL) {
        /* Level 1 duration in ticks (1 tick = 1us at 1MHz) */
        s_legacy_last_pulse_us = (uint32_t)symbols[0].duration0_ticks;
    }
    if (s_legacy_rx_done_sem != NULL) {
        BaseType_t high_task_wakeup = pdFALSE;
        xSemaphoreGiveFromISR(s_legacy_rx_done_sem, &high_task_wakeup);
    }
}

wink_status_t pal_rmt_pulse_capture_init(wink_pin_t pin, pal_rmt_edge_t start_edge) {
    (void)start_edge;
    if (s_legacy_pulse_chan != NULL) {
        pal_rmt_pulse_capture_deinit();
    }
    if (s_legacy_rx_done_sem == NULL) {
        s_legacy_rx_done_sem = xSemaphoreCreateBinary();
    }
    pal_rmt_channel_config_t cfg = {
        .pin = pin,
        .direction = PAL_RMT_DIR_RX,
        .resolution_hz = 1000000, /* 1us/tick */
        .mem_block_symbols = 64,
    };
    wink_status_t st = pal_rmt_acquire_channel(&cfg, &s_legacy_pulse_chan);
    if (st == WINK_OK) {
        pal_rmt_rx_set_callback(s_legacy_pulse_chan, legacy_rmt_rx_cb, NULL);
    }
    return st;
}

wink_status_t pal_rmt_pulse_capture_arm(void) {
    if (s_legacy_pulse_chan == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_legacy_rx_done_sem != NULL) {
        xSemaphoreTake(s_legacy_rx_done_sem, 0);
    }
    return pal_rmt_rx_start(s_legacy_pulse_chan);
}

#ifndef WINK_STRICT_NONBLOCKING
wink_status_t pal_rmt_pulse_capture_wait_armed(uint32_t timeout_us, uint32_t *pulse_us_out) {
    if (pulse_us_out == NULL || s_legacy_pulse_chan == NULL) {
        return WINK_ERR_INVALID_ARG;
    }
    *pulse_us_out = 0;
    TickType_t wait_ticks = pdMS_TO_TICKS((timeout_us + 999) / 1000 + 1);
    BaseType_t ok = xSemaphoreTake(s_legacy_rx_done_sem, wait_ticks);
    if (ok != pdPASS) {
        return WINK_ERR_TIMEOUT;
    }
    *pulse_us_out = s_legacy_last_pulse_us;
    return WINK_OK;
}

wink_status_t pal_rmt_pulse_capture_wait(uint32_t timeout_us, uint32_t *pulse_us_out) {
    wink_status_t s = pal_rmt_pulse_capture_arm();
    if (s != WINK_OK) return s;
    return pal_rmt_pulse_capture_wait_armed(timeout_us, pulse_us_out);
}
#endif

void pal_rmt_pulse_capture_deinit(void) {
    if (s_legacy_pulse_chan != NULL) {
        pal_rmt_release_channel(s_legacy_pulse_chan);
        s_legacy_pulse_chan = NULL;
    }
    if (s_legacy_rx_done_sem != NULL) {
        vSemaphoreDelete(s_legacy_rx_done_sem);
        s_legacy_rx_done_sem = NULL;
    }
}

bool pal_rmt_pulse_capture_is_active(void) {
    return (s_legacy_pulse_chan != NULL);
}

#endif /* ESP_PLATFORM */
