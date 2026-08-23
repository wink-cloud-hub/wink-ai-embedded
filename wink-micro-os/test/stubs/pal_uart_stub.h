// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_uart_stub.h
 * @brief Host target testing stub hooks for UART event and byte stream injection.
 */
#ifndef PAL_UART_STUB_H
#define PAL_UART_STUB_H

#include <stdint.h>
#include <stddef.h>
#include "hal/pal_uart.h"
#include "wink_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inject RX bytes into the simulated UART buffer and fire RX_DATA event if callback set.
 * @param[in] port UART port ID
 * @param[in] data Byte array
 * @param[in] len Length in bytes
 */
void stub_uart_inject_rx(uint8_t port, const uint8_t *data, size_t len);

/**
 * @brief Directly trigger a simulated UART hardware event.
 * @param[in] port UART port ID
 * @param[in] event Event type
 * @param[in] data Data pointer (or NULL)
 * @param[in] len Data length
 */
void stub_uart_inject_event(uint8_t port, pal_uart_event_t event, const uint8_t *data, size_t len);

/**
 * @brief Retrieve the last transmitted TX bytes on a port.
 * @param[in] port UART port ID
 * @param[out] out_data Destination buffer
 * @param[in,out] out_len Pointer to max capacity, receives actual length
 */
void stub_uart_get_last_tx(uint8_t port, uint8_t *out_data, size_t *out_len);

/**
 * @brief Force the next UART operation on a port to fail.
 * @param[in] port UART port ID
 * @param[in] err Error status
 */
void stub_uart_force_failure(uint8_t port, wink_status_t err);

/**
 * @brief Reset the simulated UART stub state for a port.
 * @param[in] port UART port ID
 */
void stub_uart_reset(uint8_t port);

#ifdef __cplusplus
}
#endif

#endif /* PAL_UART_STUB_H */
