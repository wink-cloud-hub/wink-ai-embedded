// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_spi_host.c
 * @brief Host first-class target PAL SPI implementation with protocol injection stub.
 */
#include "hal/pal_spi.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_spi_stub.h"
#include <string.h>

#define HOST_SPI_RX_QUEUE_SIZE 1024
#define HOST_SPI_TX_LOG_SIZE   1024

struct pal_spi_device_s {
    bool                    in_use;
    uint8_t                 bus_id;
    pal_spi_device_config_t cfg;
};

typedef struct {
    bool                     is_initialized;
    pal_spi_bus_config_t     cfg;
    struct pal_spi_device_s  devices[PAL_SPI_DEV_MAX_PER_BUS];
    uint8_t                  rx_queue[HOST_SPI_RX_QUEUE_SIZE];
    size_t                   rx_head;
    size_t                   rx_tail;
    uint8_t                  last_tx[HOST_SPI_TX_LOG_SIZE];
    size_t                   last_tx_len;
    wink_status_t            forced_err;
} host_spi_bus_t;

static host_spi_bus_t s_buses[PAL_SPI_BUS_MAX];
static pal_spinlock_t s_spi_lock = PAL_SPINLOCK_INITIALIZER;

/* --- Testing Stub Control API --- */

void stub_spi_inject_rx(uint8_t bus, const uint8_t *bytes, size_t len) {
    if (bus >= PAL_SPI_BUS_MAX || bytes == NULL || len == 0) {
        return;
    }
    pal_spinlock_lock(&s_spi_lock);
    host_spi_bus_t *b = &s_buses[bus];
    for (size_t i = 0; i < len; i++) {
        size_t next = (b->rx_tail + 1) % HOST_SPI_RX_QUEUE_SIZE;
        if (next != b->rx_head) {
            b->rx_queue[b->rx_tail] = bytes[i];
            b->rx_tail = next;
        }
    }
    pal_spinlock_unlock(&s_spi_lock);
}

void stub_spi_get_last_tx(uint8_t bus, uint8_t *out, size_t *out_len) {
    if (bus >= PAL_SPI_BUS_MAX || out == NULL || out_len == NULL) {
        return;
    }
    pal_spinlock_lock(&s_spi_lock);
    host_spi_bus_t *b = &s_buses[bus];
    size_t copy_len = b->last_tx_len;
    if (copy_len > *out_len) {
        copy_len = *out_len;
    }
    if (copy_len > 0) {
        memcpy(out, b->last_tx, copy_len);
    }
    *out_len = b->last_tx_len;
    pal_spinlock_unlock(&s_spi_lock);
}

void stub_spi_force_failure(uint8_t bus, wink_status_t err) {
    if (bus >= PAL_SPI_BUS_MAX) {
        return;
    }
    pal_spinlock_lock(&s_spi_lock);
    s_buses[bus].forced_err = err;
    pal_spinlock_unlock(&s_spi_lock);
}

void stub_spi_reset(uint8_t bus) {
    if (bus >= PAL_SPI_BUS_MAX) {
        return;
    }
    pal_spinlock_lock(&s_spi_lock);
    host_spi_bus_t *b = &s_buses[bus];
    b->rx_head = 0;
    b->rx_tail = 0;
    b->last_tx_len = 0;
    b->forced_err = WINK_OK;
    pal_spinlock_unlock(&s_spi_lock);
}

/* --- PAL SPI Public API --- */

WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_init_bus(const pal_spi_bus_config_t *cfg) {
    if (cfg == NULL || cfg->spi_bus >= PAL_SPI_BUS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_spi_lock);
    host_spi_bus_t *b = &s_buses[cfg->spi_bus];
    if (b->is_initialized) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_BUSY;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_SPI_BUS, cfg->spi_bus, "pal_spi_host");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_spi_lock);
        return st;
    }

    b->is_initialized = true;
    b->cfg = *cfg;
    b->rx_head = 0;
    b->rx_tail = 0;
    b->last_tx_len = 0;
    b->forced_err = WINK_OK;
    for (size_t i = 0; i < PAL_SPI_DEV_MAX_PER_BUS; i++) {
        b->devices[i].in_use = false;
    }

    pal_spinlock_unlock(&s_spi_lock);
    return WINK_OK;
}

wink_status_t pal_spi_deinit_bus(uint8_t bus) {
    if (bus >= PAL_SPI_BUS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_spi_lock);
    host_spi_bus_t *b = &s_buses[bus];
    if (!b->is_initialized) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_OK;
    }

    for (size_t i = 0; i < PAL_SPI_DEV_MAX_PER_BUS; i++) {
        if (b->devices[i].in_use) {
            pal_resource_release(PAL_RESOURCE_SPI_CS, (uint32_t)b->devices[i].cfg.cs_pin, "pal_spi_host_dev");
            b->devices[i].in_use = false;
        }
    }

    b->is_initialized = false;
    pal_resource_release(PAL_RESOURCE_SPI_BUS, bus, "pal_spi_host");
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
    host_spi_bus_t *b = &s_buses[bus];
    if (!b->is_initialized) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_INVALID_STATE;
    }

    /* Check if device slot available */
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
        wink_status_t cs_st = pal_resource_claim(PAL_RESOURCE_SPI_CS, (uint32_t)cfg->cs_pin, "pal_spi_host_dev");
        if (cs_st != WINK_OK) {
            pal_spinlock_unlock(&s_spi_lock);
            return cs_st;
        }
    }

    slot->in_use = true;
    slot->bus_id = bus;
    slot->cfg = *cfg;
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

    if (dev->cfg.cs_pin >= 0) {
        pal_resource_release(PAL_RESOURCE_SPI_CS, (uint32_t)dev->cfg.cs_pin, "pal_spi_host_dev");
    }
    dev->in_use = false;

    pal_spinlock_unlock(&s_spi_lock);
    return WINK_OK;
}

static wink_status_t host_spi_do_transfer(pal_spi_device_handle_t dev,
                                          const uint8_t *tx_buf, uint8_t *rx_buf,
                                          size_t len) {
    if (dev == NULL || (tx_buf == NULL && rx_buf == NULL) || len == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_spi_lock);
    if (!dev->in_use || dev->bus_id >= PAL_SPI_BUS_MAX) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_INVALID_STATE;
    }

    host_spi_bus_t *b = &s_buses[dev->bus_id];
    if (!b->is_initialized) {
        pal_spinlock_unlock(&s_spi_lock);
        return WINK_ERR_INVALID_STATE;
    }

    if (b->forced_err != WINK_OK) {
        wink_status_t err = b->forced_err;
        b->forced_err = WINK_OK; /* One-shot error */
        pal_spinlock_unlock(&s_spi_lock);
        return err;
    }

    /* Record TX bytes */
    if (tx_buf != NULL) {
        size_t copy_tx = (len > HOST_SPI_TX_LOG_SIZE) ? HOST_SPI_TX_LOG_SIZE : len;
        memcpy(b->last_tx, tx_buf, copy_tx);
        b->last_tx_len = copy_tx;
    } else {
        b->last_tx_len = 0;
    }

    /* Consume RX bytes from injection queue, fallback to 0xFF */
    if (rx_buf != NULL) {
        for (size_t i = 0; i < len; i++) {
            if (b->rx_head != b->rx_tail) {
                rx_buf[i] = b->rx_queue[b->rx_head];
                b->rx_head = (b->rx_head + 1) % HOST_SPI_RX_QUEUE_SIZE;
            } else {
                rx_buf[i] = 0xFF; /* Default pulled-up idle bus */
            }
        }
    }

    pal_spinlock_unlock(&s_spi_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_transfer_dma(pal_spi_device_handle_t dev,
                                   const uint8_t *tx_buf, uint8_t *rx_buf,
                                   size_t len,
                                   pal_spi_dma_callback_t cb, void *cb_arg) {
    wink_status_t st = host_spi_do_transfer(dev, tx_buf, rx_buf, len);
    if (cb != NULL) {
        cb(cb_arg, st);
    }
    return st;
}

wink_status_t pal_spi_transfer_polling(pal_spi_device_handle_t dev,
                                       const uint8_t *tx, uint8_t *rx, size_t len) {
    return host_spi_do_transfer(dev, tx, rx, len);
}
