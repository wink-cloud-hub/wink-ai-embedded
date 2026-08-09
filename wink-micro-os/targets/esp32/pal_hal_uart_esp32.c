// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_uart_esp32.c
 * @brief ESP32 target PAL HAL UART subsystem implementation (Dual Target Contract).
 */

#include "pal_hal.h"
#include "hal/pal_uart.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"

static const char *TAG = "wink_pal_uart";

wink_status_t pal_uart_init(uint8_t port, uint8_t tx_pin, uint8_t rx_pin, uint32_t baud_rate)
{
    uart_config_t uart_config = {
        .baud_rate = (int)baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config((uart_port_t)port, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed port=%d: %d", port, err);
        return WINK_ERR_HARDWARE_FAILURE;
    }

    err = uart_set_pin((uart_port_t)port, (int)tx_pin, (int)rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed port=%d: %d", port, err);
        return WINK_ERR_HARDWARE_FAILURE;
    }

    err = uart_driver_install((uart_port_t)port, 256 * 2, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed port=%d: %d", port, err);
        return WINK_ERR_HARDWARE_FAILURE;
    }

    return WINK_OK;
}

void pal_uart_deinit(uint8_t port)
{
    uart_driver_delete((uart_port_t)port);
}

wink_status_t pal_uart_read(uint8_t port, uint8_t *buf, uint32_t len, uint32_t *out_read)
{
    if (!buf || !out_read) {
        return WINK_ERR_INVALID_ARGUMENT;
    }

    int read_bytes = uart_read_bytes((uart_port_t)port, buf, len, 0);
    if (read_bytes < 0) {
        *out_read = 0;
        return WINK_ERR_HARDWARE_FAILURE;
    }

    *out_read = (uint32_t)read_bytes;
    return WINK_OK;
}

wink_status_t pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len)
{
    if (!buf && len > 0) {
        return WINK_ERR_INVALID_ARGUMENT;
    }

    int sent = uart_write_bytes((uart_port_t)port, (const char *)buf, (size_t)len);
    if (sent < 0) {
        return WINK_ERR_HARDWARE_FAILURE;
    }

    return WINK_OK;
}

#else

/* Host / Mock fallback when compiling outside ESP-IDF for ESP32 target tests */

wink_status_t pal_uart_init(uint8_t port, uint8_t tx_pin, uint8_t rx_pin, uint32_t baud_rate)
{
    (void)port; (void)tx_pin; (void)rx_pin; (void)baud_rate;
    return WINK_OK;
}

void pal_uart_deinit(uint8_t port)
{
    (void)port;
}

wink_status_t pal_uart_read(uint8_t port, uint8_t *buf, uint32_t len, uint32_t *out_read)
{
    (void)port; (void)buf; (void)len;
    if (out_read) *out_read = 0;
    return WINK_OK;
}

wink_status_t pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len)
{
    (void)port; (void)buf; (void)len;
    return WINK_OK;
}

#endif
