// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_uart.h
 * @brief PAL UART Asynchronous Stream & Lifecycle API (Dual Target Contract).
 */

#ifndef PAL_UART_H
#define PAL_UART_H

#include <stdint.h>
#include <stddef.h>
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize specified physical UART port
 * @param[in] port UART hardware port ID
 * @param[in] tx_pin Physical TX GPIO pin number
 * @param[in] rx_pin Physical RX GPIO pin number
 * @param[in] baud_rate Baud rate in bps (e.g. 115200)
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_init(uint8_t port, uint8_t tx_pin, uint8_t rx_pin, uint32_t baud_rate);

/**
 * @brief Deinitialize specified physical UART port
 * @param[in] port UART hardware port ID
 */
void pal_uart_deinit(uint8_t port);

/**
 * @brief Read bytes from UART RX buffer/fifo
 * @param[in] port UART hardware port ID
 * @param[out] buf Destination buffer to copy bytes into
 * @param[in] len Maximum bytes to read
 * @param[out] out_read Pointer to store actual bytes read
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_read(uint8_t port, uint8_t *buf, uint32_t len, uint32_t *out_read);

/**
 * @brief Write bytes to UART TX stream
 * @param[in] port UART hardware port ID
 * @param[in] buf Source buffer containing bytes to send
 * @param[in] len Number of bytes to send
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* PAL_UART_H */
