// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_spi_esp32.c
 * @brief ESP32 target PAL HAL SPI master implementation with GDMA hardware acceleration.
 */
#include "hal/pal_spi.h"
#include "hal/pal_dma.h"
#include "osal/pal_deferred.h"
#include "pal_resource.h"
#include "pal_spinlock.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_idf_version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define LOG_TAG "pal_spi"

struct pal_spi_device_s {
    bool                    in_use;
    uint8_t                 bus_id;
    pal_spi_device_config_t cfg;
    spi_device_handle_t     handle;
    pal_spi_dma_callback_t  active_cb;
    void                   *active_cb_arg;
    spi_transaction_t       trans;
};

typedef struct {
    bool                     is_initialized;
    pal_spi_bus_config_t     cfg;
    spi_host_device_t        host_id;
    SemaphoreHandle_t        bus_mutex;
    struct pal_spi_device_s  devices[PAL_SPI_DEV_MAX_PER_BUS];
} esp32_spi_bus_t;

static esp32_spi_bus_t s_buses[PAL_SPI_BUS_MAX];
static pal_spinlock_t  s_spi_lock = PAL_SPINLOCK_INITIALIZER;

static inline spi_host_device_t get_esp_spi_host(uint8_t bus) {
    return (bus == 0) ? SPI2_HOST : SPI3_HOST;
}

static void esp32_spi_deferred_worker(void *arg) {
    struct pal_spi_device_s *dev = (struct pal_spi_device_s *)arg;
    if (dev != NULL && dev->active_cb != NULL) {
        pal_spi_dma_callback_t cb = dev->active_cb;
        void *cb_arg = dev->active_cb_arg;
        dev->active_cb = NULL;
        dev->active_cb_arg = NULL;
        cb(cb_arg, WINK_OK);
    }
}

static void PAL_ISR esp32_spi_post_transfer_cb(spi_transaction_t *trans) {
    if (trans == NULL || trans->user == NULL) {
        return;
    }
    struct pal_spi_device_s *dev = (struct pal_spi_device_s *)trans->user;
    if (dev->trans.rx_buffer != NULL && dev->trans.length > 0) {
        pal_dma_cache_invalidate(dev->trans.rx_buffer, dev->trans.length / 8);
    }
    if (dev->active_cb != NULL) {
        pal_deferred_post_from_isr(PAL_DEFERRED_LO, PAL_DEFERRED_LOSSY,
                                   esp32_spi_deferred_worker, dev);
    }
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_init_bus(const pal_spi_bus_config_t *cfg) {
    if (cfg == NULL || cfg->spi_bus >= PAL_SPI_BUS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_spi_lock);
    esp32_spi_bus_t *b = &s_buses[cfg->spi_bus];
    if (b->is_initialized) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_BUSY;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_SPI_BUS, cfg->spi_bus, "pal_spi_esp32");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_spi_lock);
        return st;
    }

    spi_host_device_t host = get_esp_spi_host(cfg->spi_bus);
    spi_bus_config_t buscfg = {
        .mosi_io_num = cfg->mosi,
        .miso_io_num = cfg->miso,
        .sclk_io_num = cfg->sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 65536,
        .intr_flags = ESP_INTR_FLAG_IRAM,
    };

    spi_dma_chan_t dma_chan = cfg->dma_enabled ? SPI_DMA_CH_AUTO : SPI_DMA_DISABLED;
    esp_err_t err = spi_bus_initialize(host, &buscfg, dma_chan);
    if (err != ESP_OK) {
        pal_resource_release(PAL_RESOURCE_SPI_BUS, cfg->spi_bus, "pal_spi_esp32");
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_HARDWARE;
    }

    b->bus_mutex = xSemaphoreCreateMutex();
    b->is_initialized = true;
    b->cfg = *cfg;
    b->host_id = host;
    for (size_t i = 0; i < PAL_SPI_DEV_MAX_PER_BUS; i++) {
        b->devices[i].in_use = false;
        b->devices[i].handle = NULL;
        b->devices[i].active_cb = NULL;
    }

    pal_spinlock_unlock(&s_spi_lock);
    return WINK_OK;
}

wink_status_t pal_spi_deinit_bus(uint8_t bus) {
    if (bus >= PAL_SPI_BUS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_spi_lock);
    esp32_spi_bus_t *b = &s_buses[bus];
    if (!b->is_initialized) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_OK;
    }

    for (size_t i = 0; i < PAL_SPI_DEV_MAX_PER_BUS; i++) {
        if (b->devices[i].in_use) {
            spi_bus_remove_device(b->devices[i].handle);
            if (b->devices[i].cfg.cs_pin >= 0) {
                pal_resource_release(PAL_RESOURCE_SPI_CS, (uint32_t)b->devices[i].cfg.cs_pin, "pal_spi_esp32_dev");
            }
            b->devices[i].in_use = false;
        }
    }

    spi_bus_free(b->host_id);
    if (b->bus_mutex != NULL) {
        vSemaphoreDelete(b->bus_mutex);
        b->bus_mutex = NULL;
    }
    b->is_initialized = false;
    pal_resource_release(PAL_RESOURCE_SPI_BUS, bus, "pal_spi_esp32");
    pal_spinlock_unlock(&s_spi_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_add_device(uint8_t bus, const pal_spi_device_config_t *cfg,
                                 pal_spi_device_handle_t *out_dev) {
    if (bus >= PAL_SPI_BUS_MAX || cfg == NULL || out_dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_spi_lock);
    esp32_spi_bus_t *b = &s_buses[bus];
    if (!b->is_initialized) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_INVALID_STATE;
    }

    struct pal_spi_device_s *slot = NULL;
    for (size_t i = 0; i < PAL_SPI_DEV_MAX_PER_BUS; i++) {
        if (!b->devices[i].in_use) {
            slot = &b->devices[i];
            break;
        }
    }
    if (slot == NULL) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    if (cfg->cs_pin >= 0) {
        wink_status_t cs_st = pal_resource_claim(PAL_RESOURCE_SPI_CS, (uint32_t)cfg->cs_pin, "pal_spi_esp32_dev");
        if (cs_st != WINK_OK) {
            pal_spinlock_unlock(&s_spi_lock);
            return cs_st;
        }
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = (int)cfg->clock_hz,
        .mode = cfg->mode,
        .spics_io_num = cfg->cs_pin,
        .queue_size = 4,
        .post_cb = esp32_spi_post_transfer_cb,
    };
    if (cfg->cs_active_high) {
        devcfg.flags |= SPI_DEVICE_POSITIVE_CS;
    }
    if (cfg->cs_setup_ns > 0 && cfg->clock_hz > 0) {
        devcfg.cs_ena_pretrans = (uint8_t)(((uint64_t)cfg->cs_setup_ns * cfg->clock_hz + 999999999ULL) / 1000000000ULL);
    }
    if (cfg->cs_hold_ns > 0 && cfg->clock_hz > 0) {
        devcfg.cs_ena_posttrans = (uint8_t)(((uint64_t)cfg->cs_hold_ns * cfg->clock_hz + 999999999ULL) / 1000000000ULL);
    }

    spi_device_handle_t esp_handle;
    esp_err_t err = spi_bus_add_device(b->host_id, &devcfg, &esp_handle);
    if (err != ESP_OK) {
        if (cfg->cs_pin >= 0) {
            pal_resource_release(PAL_RESOURCE_SPI_CS, (uint32_t)cfg->cs_pin, "pal_spi_esp32_dev");
        }
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_HARDWARE;
    }

    slot->in_use = true;
    slot->bus_id = bus;
    slot->cfg = *cfg;
    slot->handle = esp_handle;
    slot->active_cb = NULL;
    slot->active_cb_arg = NULL;
    *out_dev = slot;

    pal_spinlock_unlock(&s_spi_lock);
    return WINK_OK;
}

wink_status_t pal_spi_remove_device(pal_spi_device_handle_t dev) {
    if (dev == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_spi_lock);
    if (!dev->in_use || dev->bus_id >= PAL_SPI_BUS_MAX) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_INVALID_ARG;
    }

    spi_bus_remove_device(dev->handle);
    if (dev->cfg.cs_pin >= 0) {
        pal_resource_release(PAL_RESOURCE_SPI_CS, (uint32_t)dev->cfg.cs_pin, "pal_spi_esp32_dev");
    }
    dev->in_use = false;
    dev->handle = NULL;

    pal_spinlock_unlock(&s_spi_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_transfer_dma(pal_spi_device_handle_t dev,
                                   const uint8_t *tx_buf, uint8_t *rx_buf,
                                   size_t len,
                                   pal_spi_dma_callback_t cb, void *cb_arg) {
    if (dev == NULL || (tx_buf == NULL && rx_buf == NULL) || len == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    /* Clean DMA cache before transmission */
    if (tx_buf != NULL) {
        pal_dma_cache_clean(tx_buf, len);
    }

    pal_spinlock_lock(&s_spi_lock);
    if (!dev->in_use) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_INVALID_STATE;
    }

    if (dev->active_cb != NULL) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_BUSY;
    }

    dev->active_cb = cb;
    dev->active_cb_arg = cb_arg;

    memset(&dev->trans, 0, sizeof(dev->trans));
    dev->trans.length = len * 8;
    dev->trans.tx_buffer = tx_buf;
    dev->trans.rx_buffer = rx_buf;
    dev->trans.user = dev;

    spi_device_handle_t handle = dev->handle;
    uint32_t timeout_ms = s_buses[dev->bus_id].cfg.timeout_ms;
    if (timeout_ms == 0) {
        timeout_ms = PAL_SPI_DEFAULT_TIMEOUT_MS;
    }
    TickType_t wait_ticks = pdMS_TO_TICKS(timeout_ms);

    pal_spinlock_unlock(&s_spi_lock);

    /* Queue transmission outside spinlock */
    esp_err_t err = spi_device_queue_trans(handle, &dev->trans, wait_ticks);
    if (err != ESP_OK) {
        pal_spinlock_lock(&s_spi_lock);
        dev->active_cb = NULL;
        dev->active_cb_arg = NULL;
        pal_spinlock_unlock(&s_spi_lock);
        return (err == ESP_ERR_TIMEOUT) ? WINK_ERR_TIMEOUT : WINK_ERR_HARDWARE;
    }

    return WINK_OK;
}

wink_status_t pal_spi_transfer_polling(pal_spi_device_handle_t dev,
                                       const uint8_t *tx, uint8_t *rx, size_t len) {
    if (dev == NULL || (tx == NULL && rx == NULL) || len == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_spi_lock);
    if (!dev->in_use) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_INVALID_STATE;
    }
    spi_device_handle_t handle = dev->handle;
    SemaphoreHandle_t bus_mutex = s_buses[dev->bus_id].bus_mutex;
    uint32_t timeout_ms = s_buses[dev->bus_id].cfg.timeout_ms;
    if (timeout_ms == 0) {
        timeout_ms = PAL_SPI_DEFAULT_TIMEOUT_MS;
    }
    TickType_t wait_ticks = pdMS_TO_TICKS(timeout_ms);
    pal_spinlock_unlock(&s_spi_lock);

    if (bus_mutex != NULL) {
        if (xSemaphoreTake(bus_mutex, wait_ticks) != pdPASS) {
            return WINK_ERR_TIMEOUT;
        }
    }

    if (tx != NULL) {
        pal_dma_cache_clean(tx, len);
    }

    spi_transaction_t trans;
    memset(&trans, 0, sizeof(trans));
    trans.length = len * 8;
    trans.tx_buffer = tx;
    trans.rx_buffer = rx;

    esp_err_t err = spi_device_polling_transmit(handle, &trans);

    if (rx != NULL && err == ESP_OK) {
        pal_dma_cache_invalidate(rx, len);
    }

    if (bus_mutex != NULL) {
        xSemaphoreGive(bus_mutex);
    }

    return (err == ESP_OK) ? WINK_OK : WINK_ERR_HARDWARE;
}

#else

/* Non-ESP32 fallback stubs for cross-compilation static analysis */
WINK_WARN_UNUSED_RESULT wink_status_t pal_spi_init_bus(const pal_spi_bus_config_t *cfg) { (void)cfg; return WINK_ERR_UNSUPPORTED; }
wink_status_t pal_spi_deinit_bus(uint8_t bus) { (void)bus; return WINK_ERR_UNSUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_spi_add_device(uint8_t bus, const pal_spi_device_config_t *cfg, pal_spi_device_handle_t *out_dev) { (void)bus; (void)cfg; (void)out_dev; return WINK_ERR_UNSUPPORTED; }
wink_status_t pal_spi_remove_device(pal_spi_device_handle_t dev) { (void)dev; return WINK_ERR_UNSUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_spi_transfer_dma(pal_spi_device_handle_t dev, const uint8_t *tx_buf, uint8_t *rx_buf, size_t len, pal_spi_dma_callback_t cb, void *cb_arg) { (void)dev; (void)tx_buf; (void)rx_buf; (void)len; (void)cb; (void)cb_arg; return WINK_ERR_UNSUPPORTED; }
wink_status_t pal_spi_transfer_polling(pal_spi_device_handle_t dev, const uint8_t *tx, uint8_t *rx, size_t len) { (void)dev; (void)tx; (void)rx; (void)len; return WINK_ERR_UNSUPPORTED; }

#endif

