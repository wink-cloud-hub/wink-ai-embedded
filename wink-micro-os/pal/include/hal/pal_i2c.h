// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_i2c.h
 * @brief PAL I2C Bus Lifecycle API.
 */

#ifndef PAL_I2C_H
#define PAL_I2C_H

#include <stdint.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize specified physical I2C bus
 * @param[in] port I2C hardware port ID [0, PAL_I2C_PORTS)
 * @param[in] sda Physical SDA GPIO pin number
 * @param[in] scl Physical SCL GPIO pin number
 * @param[in] hz I2C bus frequency in Hz (e.g. 100000 for 100kHz)
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_i2c_bus_init(uint8_t port, uint8_t sda, uint8_t scl, uint32_t hz);

/**
 * @brief Deinitialize specified physical I2C bus (includes SCL 9-pulse bus recovery sequence)
 * @param[in] port I2C hardware port ID
 */
void pal_i2c_bus_deinit(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif /* PAL_I2C_H */
