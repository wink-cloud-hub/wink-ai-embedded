// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_ch2_spi.c
 * @brief Wasm target PAL SPI channel implementation with pull-model async completion.
 */
#include "hal/pal_spi.h"
#include "wasm_bridge.h"
#include "pal_wasm_completion.h"
#include "pal_resource.h"
#include "pal_wasm_common.h"
#include <string.h>

#define WASM_SPI_BUS_MAX 2
#define WASM_SPI_DEV_MAX 8

struct pal_spi_device_s {
    bool                     in_use;
    uint8_t                  bus_id;
    uint8_t                  cs_pin;
    uint32_t                 clock_hz;
    uint8_t                  mode;
    pal_spi_device_config_t  cfg;
};

static bool s_bus_initialized[WASM_SPI_BUS_MAX];
static struct pal_spi_device_s s_devices[WASM_SPI_DEV_MAX];

WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_init_bus(const pal_spi_bus_config_t *cfg) {
    if (cfg == NULL || cfg->spi_bus >= WASM_SPI_BUS_MAX) {
        return WINK_ERR_INVALID_ARG;
    }
    if (s_bus_initialized[cfg->spi_bus]) {
        return WINK_ERR_BUSY;
    }
    s_bus_initialized[cfg->spi_bus] = true;
    return WINK_OK;
}

wink_status_t pal_spi_deinit_bus(uint8_t bus_id) {
    if (bus_id >= WASM_SPI_BUS_MAX || !s_bus_initialized[bus_id]) {
        return WINK_ERR_INVALID_ARG;
    }
    s_bus_initialized[bus_id] = false;
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_add_device(uint8_t bus, const pal_spi_device_config_t *cfg, pal_spi_device_handle_t *out_handle) {
    if (cfg == NULL || out_handle == NULL || bus >= WASM_SPI_BUS_MAX || !s_bus_initialized[bus]) {
        return WINK_ERR_INVALID_ARG;
    }

    struct pal_spi_device_s *slot = NULL;
    for (int i = 0; i < WASM_SPI_DEV_MAX; i++) {
        if (!s_devices[i].in_use) {
            slot = &s_devices[i];
            break;
        }
    }
    if (slot == NULL) {
        return WINK_ERR_RESOURCE_EXHAUSTED;
    }

    slot->in_use = true;
    slot->bus_id = bus;
    slot->cs_pin = (uint8_t)cfg->cs_pin;
    slot->clock_hz = (cfg->clock_hz > 0) ? cfg->clock_hz : 1000000;
    slot->mode = (uint8_t)cfg->mode;
    slot->cfg = *cfg;

    *out_handle = slot;
    return WINK_OK;
}

wink_status_t pal_spi_remove_device(pal_spi_device_handle_t dev) {
    if (dev == NULL || !dev->in_use) {
        return WINK_ERR_INVALID_ARG;
    }
    dev->in_use = false;
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_transfer_dma(pal_spi_device_handle_t dev,
                                   const uint8_t *tx,
                                   uint8_t *rx,
                                   size_t len,
                                   pal_spi_dma_callback_t cb,
                                   void *arg) {
    if (dev == NULL || !dev->in_use || (tx == NULL && rx == NULL) || len == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    /* 1. Pass data to JS (Axis C CH2) */
    js_pal_spi_transfer(dev->bus_id, dev->cs_pin, tx, (uint32_t)len, rx, dev->mode, dev->clock_hz);

    /* 2. Schedule completion based on modeled baud rate */
    if (cb != NULL) {
        uint32_t delta_us = (uint32_t)((len * 8 * 1000000ULL + dev->clock_hz - 1) / dev->clock_hz);
        if (delta_us == 0) delta_us = 1;

        return pal_wasm_schedule_complete_us(delta_us, (pal_wasm_completion_cb_t)cb, arg);
    }

    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_transfer_polling(pal_spi_device_handle_t dev,
                                       const uint8_t *tx,
                                       uint8_t *rx,
                                       size_t len) {
    if (dev == NULL || !dev->in_use || (tx == NULL && rx == NULL) || len == 0) {
        return WINK_ERR_INVALID_ARG;
    }

    js_pal_spi_transfer(dev->bus_id, dev->cs_pin, tx, (uint32_t)len, rx, dev->mode, dev->clock_hz);
    uint32_t delta_us = (uint32_t)((len * 8 * 1000000ULL + dev->clock_hz - 1) / dev->clock_hz);
    if (delta_us > 0) {
        pal_wasm_advance_virtual_clock((uint64_t)delta_us);
    }
    return WINK_OK;
}
