// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_spi_stub.h
 * @brief Host target testing stub hooks for SPI injection and verification.
 */
#ifndef PAL_SPI_STUB_H
#define PAL_SPI_STUB_H

#include <stdint.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inject RX bytes into the simulated SPI response queue for a bus.
 * @param[in] bus Bus index
 * @param[in] bytes Byte array to inject
 * @param[in] len Length of bytes
 */
void stub_spi_inject_rx(uint8_t bus, const uint8_t *bytes, size_t len);

/**
 * @brief Retrieve the last transmitted TX bytes on a bus.
 * @param[in] bus Bus index
 * @param[out] out Buffer to store the captured bytes
 * @param[in,out] out_len Pointer to max capacity, receives actual captured length
 */
void stub_spi_get_last_tx(uint8_t bus, uint8_t *out, size_t *out_len);

/**
 * @brief Force the next SPI transfer on a bus to fail with specified status.
 * @param[in] bus Bus index
 * @param[in] err Error code to return (e.g. WINK_ERR_HARDWARE, WINK_ERR_IO)
 */
void stub_spi_force_failure(uint8_t bus, wink_status_t err);

/**
 * @brief Reset simulated SPI stub state (queues and capture buffers).
 * @param[in] bus Bus index
 */
void stub_spi_reset(uint8_t bus);

#ifdef __cplusplus
}
#endif

#endif /* PAL_SPI_STUB_H */
