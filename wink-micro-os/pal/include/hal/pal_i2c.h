// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_i2c.h
 * @brief PAL I2C Bus Lifecycle, Transfer Timeout, and Bus Recovery API (ADR-0067).
 */

#ifndef PAL_I2C_H
#define PAL_I2C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "wink_status.h"
#include "hal/pal_pin_types.h"
#include "hal/pal_target_caps.h"
#include "wink_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PAL_I2C_DEFAULT_TIMEOUT_MS
#define PAL_I2C_DEFAULT_TIMEOUT_MS 1000
#endif

/**
 * @brief Query physical SDA/SCL GPIO pins mapped to specified I2C port
 * @param[in] port I2C port ID [0, PAL_I2C_PORT_MAX)
 * @param[out] out_sda Output pointer for SDA pin
 * @param[out] out_scl Output pointer for SCL pin
 * @return WINK_OK on success, WINK_ERR_INVALID_ARG on out of bounds, WINK_ERR_UNSUPPORTED if target lacks routing
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_port_pins(uint8_t port, wink_pin_t *out_sda, wink_pin_t *out_scl);

/**
 * @brief Initialize specified physical I2C bus
 * @note Task-only. Automatically claims I2C port & GPIO pin resources under RAII model (ADR-0065).
 * @param[in] port I2C hardware port ID [0, PAL_I2C_PORT_MAX)
 * @param[in] sda Physical SDA GPIO pin number
 * @param[in] scl Physical SCL GPIO pin number
 * @param[in] hz I2C bus frequency in Hz (e.g. 100000 for 100kHz, 400000 for 400kHz)
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_bus_init(uint8_t port, wink_pin_t sda, wink_pin_t scl, uint32_t hz);

/**
 * @brief Deinitialize specified physical I2C bus and release claimed hardware resources
 * @param[in] port I2C hardware port ID
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_bus_deinit(uint8_t port);

/**
 * @brief Execute NXP standard SCL 9-pulse sequence + STOP condition to recover a locked I2C bus (SDA stuck low)
 * @param[in] port I2C hardware port ID
 * @return WINK_OK on successful bus release, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_bus_recover(uint8_t port);

#ifndef WINK_STRICT_NONBLOCKING
/**
 * @brief Perform synchronous I2C bus transfer with explicit wall-clock timeout
 * @param[in] port I2C hardware port ID
 * @param[in] dev_addr I2C 7-bit target slave address
 * @param[in] write_buf Data buffer to write (can be NULL if read-only)
 * @param[in] write_len Byte length of write buffer (0 if read-only)
 * @param[out] read_buf Data buffer for read response (can be NULL if write-only)
 * @param[in] read_len Byte length of read buffer (0 if write-only)
 * @param[in] timeout_ms Wall-clock timeout in milliseconds (0 = use PAL_I2C_DEFAULT_TIMEOUT_MS, UINT32_MAX = infinite)
 * @return WINK_OK on success, WINK_ERR_TIMEOUT on timeout (triggers bus_recover), error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_transfer_timeout(uint8_t port, uint16_t dev_addr,
                                       const uint8_t *write_buf, uint32_t write_len,
                                       uint8_t *read_buf, uint32_t read_len,
                                       uint32_t timeout_ms);

/**
 * @brief Legacy synchronous I2C transfer wrapper (defaults to PAL_I2C_DEFAULT_TIMEOUT_MS)
 */
static inline wink_status_t
pal_i2c_transfer(uint8_t port, uint16_t dev_addr,
                 const uint8_t *write_buf, uint32_t write_len,
                 uint8_t *read_buf, uint32_t read_len)
{
    return pal_i2c_transfer_timeout(port, dev_addr, write_buf, write_len,
                                    read_buf, read_len, PAL_I2C_DEFAULT_TIMEOUT_MS);
}

/**
 * @brief Scan a range of I2C 7-bit addresses for responding devices
 * @param[in] port I2C hardware port ID [0, PAL_I2C_PORT_MAX)
 * @param[in] start_addr First 7-bit address to probe
 * @param[in] end_addr Last 7-bit address to probe
 * @param[out] out_found_bitmap 16-byte buffer (128 bits) receiving scan results
 * @param[in] bitmap_bytes Must be 16 (for 128-bit address space)
 * @return WINK_OK on completion, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_scan(uint8_t port, uint8_t start_addr, uint8_t end_addr,
                            uint8_t *out_found_bitmap, size_t bitmap_bytes);
#endif /* WINK_STRICT_NONBLOCKING */

#ifdef __cplusplus
}
#endif

#endif /* PAL_I2C_H */
