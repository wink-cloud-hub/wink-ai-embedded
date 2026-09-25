// SPDX-License-Identifier: LGPL-3.0-only
#ifndef DRIVER_UART_H
#define DRIVER_UART_H

#include "esp_err.h"
#include "hal/uart_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <stddef.h>

#define UART_PIN_NO_CHANGE (-1)

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t *uart_config);
esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num, int rts_io_num, int cts_io_num);
esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size, int queue_size, QueueHandle_t *uart_queue, int intr_alloc_flags);
esp_err_t uart_driver_delete(uart_port_t uart_num);
int uart_write_bytes(uart_port_t uart_num, const void *src, size_t size);
int uart_read_bytes(uart_port_t uart_num, void *buf, uint32_t length, TickType_t ticks_to_wait);
esp_err_t uart_flush(uart_port_t uart_num);
esp_err_t uart_get_buffered_data_len(uart_port_t uart_num, size_t *size);

void esp_uart_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_UART_H */
