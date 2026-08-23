// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_uart.h
 * @brief PAL UART asynchronous stream and event-driven subsystem interface.
 */

#ifndef PAL_UART_H
#define PAL_UART_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "wink_status.h"
#include "hal/pal_pin_types.h"
#include "wink_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UART asynchronous hardware events.
 */
typedef enum {
    PAL_UART_EVENT_RX_DATA     = 1, /**< Incoming data available in FIFO/buffer */
    PAL_UART_EVENT_RX_FIFO_OVF = 2, /**< RX FIFO hardware overflow */
    PAL_UART_EVENT_BUFFER_FULL = 3, /**< Ring buffer full */
    PAL_UART_EVENT_BREAK       = 4, /**< Line break condition detected */
    PAL_UART_EVENT_PARITY_ERR  = 5, /**< Parity check error */
    PAL_UART_EVENT_FRAME_ERR   = 6, /**< Framing error */
    PAL_UART_EVENT_TX_DONE     = 7, /**< Asynchronous TX transmit completed */
} pal_uart_event_t;

/**
 * @brief Event callback function signature.
 *
 * @param[in] port UART hardware port ID
 * @param[in] event Event type
 * @param[in] data Pointer to available received data (for RX_DATA event, NULL otherwise)
 * @param[in] len Length of data available (0 for non-data events)
 * @param[in] arg User context argument passed during registration
 *
 * @note Callback is invoked in ISR or high-priority task context; strictly no blocking/malloc!
 */
typedef void (*pal_uart_event_callback_t)(uint8_t port,
                                          pal_uart_event_t event,
                                          const uint8_t *data,
                                          size_t len,
                                          void *arg);

/**
 * @brief Initialize specified physical UART port
 * @param[in] port UART hardware port ID (0..PAL_UART_PORT_MAX-1)
 * @param[in] tx_pin Physical TX GPIO pin number
 * @param[in] rx_pin Physical RX GPIO pin number
 * @param[in] baud_rate Baud rate in bps (e.g. 115200)
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_init(uint8_t port, wink_pin_t tx_pin, wink_pin_t rx_pin, uint32_t baud_rate);

/**
 * @brief Deinitialize specified physical UART port
 * @param[in] port UART hardware port ID
 */
void pal_uart_deinit(uint8_t port);

/**
 * @brief Register asynchronous event callback for a UART port.
 * @param[in] port UART hardware port ID
 * @param[in] cb Event callback
 * @param[in] arg User context argument
 * @return WINK_OK on success
 */
wink_status_t pal_uart_set_event_callback(uint8_t port,
                                          pal_uart_event_callback_t cb,
                                          void *arg);

/**
 * @brief Read bytes from UART RX buffer/fifo (polling / synchronous)
 * @param[in] port UART hardware port ID
 * @param[out] buf Destination buffer to copy bytes into
 * @param[in] len Maximum bytes to read
 * @param[out] out_read Pointer to store actual bytes read
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_read(uint8_t port, uint8_t *buf, uint32_t len, uint32_t *out_read);

/**
 * @brief Synchronous blocking write to UART TX stream.
 * @param[in] port UART hardware port ID
 * @param[in] buf Source buffer containing bytes to send
 * @param[in] len Number of bytes to send
 * @return WINK_OK on success, error status code otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len);

/**
 * @brief Asynchronous non-blocking write to UART TX stream.
 * @param[in] port UART hardware port ID
 * @param[in] buf Source buffer containing bytes to send
 * @param[in] len Number of bytes to send
 * @return WINK_OK if queued, error status otherwise
 */
WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_write_async(uint8_t port, const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PAL_UART_H */
