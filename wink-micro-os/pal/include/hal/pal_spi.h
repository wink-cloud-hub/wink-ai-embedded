// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_spi.h
 * @brief PAL SPI master subsystem interface with asynchronous DMA engine.
 *
 * Provides thread-safe, static-allocated SPI master and DMA transfer abstractions.
 *
 * Rules:
 * - Buffer attributes: DMA buffers must carry PAL_DMA_BUF_ATTR and be 4-byte aligned (internal RAM).
 * - Allocation: No runtime dynamic allocation; static pools allocated at init/add_device.
 * - Callbacks: Triggered in ISR context (ESP-IDF) or task context (Host). Strictly NO blocking/malloc/logging in callbacks.
 */
#ifndef PAL_SPI_H
#define PAL_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hal/pal_pin_types.h"
#include "wink_status.h"
#include "wink_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAL_SPI_DEV_MAX_PER_BUS
#define PAL_SPI_DEV_MAX_PER_BUS 4
#endif

#ifndef PAL_SPI_DEFAULT_TIMEOUT_MS
#define PAL_SPI_DEFAULT_TIMEOUT_MS 1000
#endif

typedef struct {
    uint8_t    spi_bus;     /**< 0 = SPI2_HOST (HSPI), 1 = SPI3_HOST (VSPI) */
    wink_pin_t sclk;
    wink_pin_t mosi;
    wink_pin_t miso;
    uint32_t   clock_hz;    /**< Maximum bus clock frequency (e.g. 1 MHz ~ 40 MHz) */
    uint8_t    mode;        /**< SPI mode 0..3 (CPOL/CPHA) */
    bool       dma_enabled; /**< Enable DMA engine for high-speed transfers */
    uint32_t   timeout_ms;  /**< Timeout in ms for SPI queue/transfer (0 = PAL_SPI_DEFAULT_TIMEOUT_MS) */
} pal_spi_bus_config_t;

typedef struct {
    wink_pin_t cs_pin;         /**< Chip select pin */
    uint32_t   clock_hz;       /**< Device clock frequency */
    uint8_t    mode;           /**< SPI mode 0..3 */
    bool       cs_active_high; /**< True if CS active high, false if active low */
    uint16_t   cs_setup_ns;    /**< CS setup time before clock in nanoseconds */
    uint16_t   cs_hold_ns;     /**< CS hold time after clock in nanoseconds */
} pal_spi_device_config_t;

typedef struct pal_spi_device_s *pal_spi_device_handle_t;

/**
 * @brief Asynchronous completion callback.
 *
 * Invocation Context:
 * - ESP-IDF: ISR context (spi_transaction_event_t post-transfer).
 * - Host: Direct in caller thread or test simulator dispatch.
 * - Wasm: Soft interrupt / pull dispatch.
 *
 * @note Inside callback: strictly no blocking / log / malloc! Use FromISR OSAL primitives.
 */
typedef void (*pal_spi_dma_callback_t)(void *arg, wink_status_t result);

/**
 * @brief Initialize an SPI master bus.
 * @param[in] cfg Bus configuration
 * @return WINK_OK on success,
 *         WINK_ERR_INVALID_ARG if invalid config,
 *         WINK_ERR_BUSY if bus already initialized,
 *         WINK_ERR_HARDWARE on peripheral failure
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_init_bus(const pal_spi_bus_config_t *cfg);

/**
 * @brief Deinitialize an SPI master bus.
 * @param[in] bus Bus index (0..PAL_SPI_BUS_MAX-1)
 * @return WINK_OK on success
 */
wink_status_t pal_spi_deinit_bus(uint8_t bus);

/**
 * @brief Add a device to an initialized SPI bus.
 * @param[in] bus Bus index
 * @param[in] cfg Device configuration
 * @param[out] out_dev Output device handle
 * @return WINK_OK on success,
 *         WINK_ERR_INVALID_ARG on invalid parameters,
 *         WINK_ERR_RESOURCE_EXHAUSTED if device pool for this bus is full,
 *         WINK_ERR_INVALID_STATE if bus not initialized
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_add_device(uint8_t bus, const pal_spi_device_config_t *cfg,
                                 pal_spi_device_handle_t *out_dev);

/**
 * @brief Remove a device from its SPI bus.
 * @param[in] dev Device handle
 * @return WINK_OK on success
 */
wink_status_t pal_spi_remove_device(pal_spi_device_handle_t dev);

/**
 * @brief Asynchronous DMA full-duplex transfer.
 * @param[in] dev Device handle
 * @param[in] tx_buf Transmit buffer (PAL_DMA_BUF_ATTR, 4-byte aligned, can be NULL if rx-only)
 * @param[out] rx_buf Receive buffer (PAL_DMA_BUF_ATTR, 4-byte aligned, can be NULL if tx-only)
 * @param[in] len Transfer length in bytes
 * @param[in] cb Completion callback (invoked upon transfer end)
 * @param[in] cb_arg User argument passed to callback
 * @return WINK_OK if queued successfully, WINK_ERR_BUSY if device busy, WINK_ERR_INVALID_ARG on error
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_spi_transfer_dma(pal_spi_device_handle_t dev,
                                   const uint8_t *tx_buf, uint8_t *rx_buf,
                                   size_t len,
                                   pal_spi_dma_callback_t cb, void *cb_arg);

/**
 * @brief Synchronous polling transfer.
 * @note Only allowed in task context or initialization paths, strictly forbidden in ISR context.
 * @param[in] dev Device handle
 * @param[in] tx Transmit buffer (can be NULL)
 * @param[out] rx Receive buffer (can be NULL)
 * @param[in] len Transfer length in bytes
 * @return WINK_OK on success
 */
wink_status_t pal_spi_transfer_polling(pal_spi_device_handle_t dev,
                                       const uint8_t *tx, uint8_t *rx, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PAL_SPI_H */
