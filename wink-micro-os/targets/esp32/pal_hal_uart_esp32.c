// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_uart_esp32.c
 * @brief ESP32 target PAL HAL UART asynchronous event subsystem implementation.
 */

#include "pal_hal.h"
#include "hal/pal_uart.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_log.h"

#include <string.h>

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define LOG_TAG "pal_uart"

#define UART_RX_BUF_SIZE 1024
#define UART_TX_BUF_SIZE 512

typedef struct {
    bool                      in_use;
    wink_pin_t                tx_pin;
    wink_pin_t                rx_pin;
    uint32_t                  baud_rate;
    QueueHandle_t             uart_queue;
    TaskHandle_t              event_task;
    pal_uart_event_callback_t event_cb;
    void                     *event_cb_arg;
} esp32_uart_port_t;

static esp32_uart_port_t s_ports[PAL_UART_PORT_MAX];
static pal_spinlock_t s_uart_lock = PAL_SPINLOCK_INITIALIZER;

static void esp32_uart_event_task(void *pvParameters) {
    uint8_t port = (uint8_t)(uintptr_t)pvParameters;
    esp32_uart_port_t *p = &s_ports[port];
    uart_event_t event;
    uint8_t dtmp[128];

    for (;;) {
        if (xQueueReceive(p->uart_queue, (void *)&event, portMAX_DELAY)) {
            pal_uart_event_callback_t cb = p->event_cb;
            void *arg = p->event_cb_arg;

            switch (event.type) {
                case UART_DATA: {
                    if (cb != NULL) {
                        int len = uart_read_bytes((uart_port_t)port, dtmp, sizeof(dtmp), 0);
                        if (len > 0) {
                            cb(port, PAL_UART_EVENT_RX_DATA, dtmp, (size_t)len, arg);
                        }
                    }
                    break;
                }
                case UART_FIFO_OVF:
                    uart_flush_input((uart_port_t)port);
                    xQueueReset(p->uart_queue);
                    if (cb != NULL) {
                        cb(port, PAL_UART_EVENT_RX_FIFO_OVF, NULL, 0, arg);
                    }
                    break;
                case UART_BUFFER_FULL:
                    uart_flush_input((uart_port_t)port);
                    xQueueReset(p->uart_queue);
                    if (cb != NULL) {
                        cb(port, PAL_UART_EVENT_BUFFER_FULL, NULL, 0, arg);
                    }
                    break;
                case UART_BREAK:
                    if (cb != NULL) {
                        cb(port, PAL_UART_EVENT_BREAK, NULL, 0, arg);
                    }
                    break;
                case UART_PARITY_ERR:
                    if (cb != NULL) {
                        cb(port, PAL_UART_EVENT_PARITY_ERR, NULL, 0, arg);
                    }
                    break;
                case UART_FRAME_ERR:
                    if (cb != NULL) {
                        cb(port, PAL_UART_EVENT_FRAME_ERR, NULL, 0, arg);
                    }
                    break;
                default:
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_init(uint8_t port, wink_pin_t tx_pin, wink_pin_t rx_pin, uint32_t baud_rate) {
    if (port >= PAL_UART_PORT_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_uart_lock);
    esp32_uart_port_t *p = &s_ports[port];
    if (p->in_use) {
        pal_spinlock_unlock(&s_uart_lock);
        return WINK_ERR_BUSY;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_UART_PORT, port, "pal_uart_esp32");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_uart_lock);
        return st;
    }

    if (tx_pin >= 0) {
        st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)tx_pin, "pal_uart_esp32");
        if (st != WINK_OK) {
            pal_resource_release(PAL_RESOURCE_UART_PORT, port, "pal_uart_esp32");
            pal_spinlock_unlock(&s_uart_lock);
            return st;
        }
    }

    if (rx_pin >= 0) {
        st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)rx_pin, "pal_uart_esp32");
        if (st != WINK_OK) {
            if (tx_pin >= 0) {
                pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)tx_pin, "pal_uart_esp32");
            }
            pal_resource_release(PAL_RESOURCE_UART_PORT, port, "pal_uart_esp32");
            pal_spinlock_unlock(&s_uart_lock);
            return st;
        }
    }

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
        if (rx_pin >= 0) pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)rx_pin, "pal_uart_esp32");
        if (tx_pin >= 0) pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)tx_pin, "pal_uart_esp32");
        pal_resource_release(PAL_RESOURCE_UART_PORT, port, "pal_uart_esp32");
        pal_spinlock_unlock(&s_uart_lock);
        return WINK_ERR_HARDWARE;
    }

    err = uart_set_pin((uart_port_t)port, (int)tx_pin, (int)rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        if (rx_pin >= 0) pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)rx_pin, "pal_uart_esp32");
        if (tx_pin >= 0) pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)tx_pin, "pal_uart_esp32");
        pal_resource_release(PAL_RESOURCE_UART_PORT, port, "pal_uart_esp32");
        pal_spinlock_unlock(&s_uart_lock);
        return WINK_ERR_HARDWARE;
    }

    err = uart_driver_install((uart_port_t)port, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 16, &p->uart_queue, 0);
    if (err != ESP_OK) {
        if (rx_pin >= 0) pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)rx_pin, "pal_uart_esp32");
        if (tx_pin >= 0) pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)tx_pin, "pal_uart_esp32");
        pal_resource_release(PAL_RESOURCE_UART_PORT, port, "pal_uart_esp32");
        pal_spinlock_unlock(&s_uart_lock);
        return WINK_ERR_HARDWARE;
    }

    p->in_use = true;
    p->tx_pin = tx_pin;
    p->rx_pin = rx_pin;
    p->baud_rate = baud_rate;
    p->event_cb = NULL;
    p->event_cb_arg = NULL;

    /* Create background event task */
    xTaskCreate(esp32_uart_event_task, "uart_evt", 2048, (void *)(uintptr_t)port, 12, &p->event_task);

    pal_spinlock_unlock(&s_uart_lock);
    return WINK_OK;
}

void pal_uart_deinit(uint8_t port) {
    if (port >= PAL_UART_PORT_MAX) {
        return;
    }

    pal_spinlock_lock(&s_uart_lock);
    esp32_uart_port_t *p = &s_ports[port];
    if (!p->in_use) {
        pal_spinlock_unlock(&s_uart_lock);
        return;
    }

    if (p->event_task != NULL) {
        vTaskDelete(p->event_task);
        p->event_task = NULL;
    }

    uart_driver_delete((uart_port_t)port);

    if (p->rx_pin >= 0) {
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)p->rx_pin, "pal_uart_esp32");
    }
    if (p->tx_pin >= 0) {
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)p->tx_pin, "pal_uart_esp32");
    }
    pal_resource_release(PAL_RESOURCE_UART_PORT, port, "pal_uart_esp32");

    p->in_use = false;
    pal_spinlock_unlock(&s_uart_lock);
}

wink_status_t pal_uart_set_event_callback(uint8_t port,
                                          pal_uart_event_callback_t cb,
                                          void *arg) {
    if (port >= PAL_UART_PORT_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_uart_lock);
    esp32_uart_port_t *p = &s_ports[port];
    if (!p->in_use) {
        pal_spinlock_unlock(&s_uart_lock);
        return WINK_ERR_INVALID_STATE;
    }

    p->event_cb = cb;
    p->event_cb_arg = arg;
    pal_spinlock_unlock(&s_uart_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_read(uint8_t port, uint8_t *buf, uint32_t len, uint32_t *out_read) {
    if (port >= PAL_UART_PORT_MAX || !buf || !out_read) {
        return WINK_ERR_INVALID_ARG;
    }

    int read_bytes = uart_read_bytes((uart_port_t)port, buf, len, 0);
    if (read_bytes < 0) {
        *out_read = 0;
        return WINK_ERR_HARDWARE;
    }

    *out_read = (uint32_t)read_bytes;
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len) {
    if (port >= PAL_UART_PORT_MAX || (!buf && len > 0)) {
        return WINK_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return WINK_OK;
    }

    int sent = uart_write_bytes((uart_port_t)port, (const char *)buf, (size_t)len);
    if (sent < 0) {
        return WINK_ERR_HARDWARE;
    }

    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_write_async(uint8_t port, const uint8_t *buf, size_t len) {
    return pal_uart_write(port, buf, (uint32_t)len);
}

#else

/* Non-ESP32 fallback stubs for cross-compilation static analysis */
WINK_WARN_UNUSED_RESULT wink_status_t pal_uart_init(uint8_t port, wink_pin_t tx_pin, wink_pin_t rx_pin, uint32_t baud_rate) { (void)port; (void)tx_pin; (void)rx_pin; (void)baud_rate; return WINK_ERR_NOT_SUPPORTED; }
void pal_uart_deinit(uint8_t port) { (void)port; }
wink_status_t pal_uart_set_event_callback(uint8_t port, pal_uart_event_callback_t cb, void *arg) { (void)port; (void)cb; (void)arg; return WINK_ERR_NOT_SUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_uart_read(uint8_t port, uint8_t *buf, uint32_t len, uint32_t *out_read) { (void)port; (void)buf; (void)len; if (out_read) *out_read = 0; return WINK_ERR_NOT_SUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len) { (void)port; (void)buf; (void)len; return WINK_ERR_NOT_SUPPORTED; }
WINK_WARN_UNUSED_RESULT wink_status_t pal_uart_write_async(uint8_t port, const uint8_t *buf, size_t len) { (void)port; (void)buf; (void)len; return WINK_ERR_NOT_SUPPORTED; }

#endif
