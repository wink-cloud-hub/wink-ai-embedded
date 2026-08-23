// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_hal_uart_host.c
 * @brief Host first-class target PAL UART implementation with event callback simulation and injection stub.
 */
#include "hal/pal_uart.h"
#include "pal_resource.h"
#include "pal_spinlock.h"
#include "pal_uart_stub.h"
#include <string.h>

#define HOST_UART_RX_BUF_SIZE 1024
#define HOST_UART_TX_BUF_SIZE 1024

typedef struct {
    bool                      in_use;
    wink_pin_t                tx_pin;
    wink_pin_t                rx_pin;
    uint32_t                  baud_rate;
    pal_uart_event_callback_t event_cb;
    void                     *event_cb_arg;
    uint8_t                   rx_buf[HOST_UART_RX_BUF_SIZE];
    size_t                    rx_head;
    size_t                    rx_tail;
    uint8_t                   last_tx[HOST_UART_TX_BUF_SIZE];
    size_t                    last_tx_len;
    wink_status_t             forced_err;
} host_uart_port_t;

static host_uart_port_t s_ports[PAL_UART_PORT_MAX];
static pal_spinlock_t s_uart_lock = PAL_SPINLOCK_INITIALIZER;

/* --- Testing Stub Control Hooks --- */

void stub_uart_inject_rx(uint8_t port, const uint8_t *data, size_t len) {
    if (port >= PAL_UART_PORT_MAX || data == NULL || len == 0) {
        return;
    }
    pal_spinlock_lock(&s_uart_lock);
    host_uart_port_t *p = &s_ports[port];
    if (!p->in_use) {
        pal_spinlock_unlock(&s_uart_lock);
        return;
    }

    for (size_t i = 0; i < len; i++) {
        size_t next = (p->rx_tail + 1) % HOST_UART_RX_BUF_SIZE;
        if (next != p->rx_head) {
            p->rx_buf[p->rx_tail] = data[i];
            p->rx_tail = next;
        }
    }

    pal_uart_event_callback_t cb = p->event_cb;
    void *arg = p->event_cb_arg;
    pal_spinlock_unlock(&s_uart_lock);

    if (cb != NULL) {
        cb(port, PAL_UART_EVENT_RX_DATA, data, len, arg);
    }
}

void stub_uart_inject_event(uint8_t port, pal_uart_event_t event, const uint8_t *data, size_t len) {
    if (port >= PAL_UART_PORT_MAX) {
        return;
    }
    pal_spinlock_lock(&s_uart_lock);
    host_uart_port_t *p = &s_ports[port];
    if (!p->in_use) {
        pal_spinlock_unlock(&s_uart_lock);
        return;
    }
    pal_uart_event_callback_t cb = p->event_cb;
    void *arg = p->event_cb_arg;
    pal_spinlock_unlock(&s_uart_lock);

    if (cb != NULL) {
        cb(port, event, data, len, arg);
    }
}

void stub_uart_get_last_tx(uint8_t port, uint8_t *out_data, size_t *out_len) {
    if (port >= PAL_UART_PORT_MAX || out_data == NULL || out_len == NULL) {
        return;
    }
    pal_spinlock_lock(&s_uart_lock);
    host_uart_port_t *p = &s_ports[port];
    size_t copy_len = p->last_tx_len;
    if (copy_len > *out_len) {
        copy_len = *out_len;
    }
    if (copy_len > 0) {
        memcpy(out_data, p->last_tx, copy_len);
    }
    *out_len = p->last_tx_len;
    pal_spinlock_unlock(&s_uart_lock);
}

void stub_uart_force_failure(uint8_t port, wink_status_t err) {
    if (port >= PAL_UART_PORT_MAX) {
        return;
    }
    pal_spinlock_lock(&s_uart_lock);
    s_ports[port].forced_err = err;
    pal_spinlock_unlock(&s_uart_lock);
}

void stub_uart_reset(uint8_t port) {
    if (port >= PAL_UART_PORT_MAX) {
        return;
    }
    pal_spinlock_lock(&s_uart_lock);
    host_uart_port_t *p = &s_ports[port];
    p->rx_head = 0;
    p->rx_tail = 0;
    p->last_tx_len = 0;
    p->forced_err = WINK_OK;
    pal_spinlock_unlock(&s_uart_lock);
}

/* --- PAL UART Public API --- */

WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_init(uint8_t port, wink_pin_t tx_pin, wink_pin_t rx_pin, uint32_t baud_rate) {
    if (port >= PAL_UART_PORT_MAX) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_uart_lock);
    host_uart_port_t *p = &s_ports[port];
    if (p->in_use) {
        pal_spinlock_unlock(&s_uart_lock);
        return WINK_ERR_BUSY;
    }

    wink_status_t st = pal_resource_claim(PAL_RESOURCE_UART_PORT, port, "pal_uart_host");
    if (st != WINK_OK) {
        pal_spinlock_unlock(&s_uart_lock);
        return st;
    }

    if (tx_pin >= 0) {
        st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)tx_pin, "pal_uart_host");
        if (st != WINK_OK) {
            pal_resource_release(PAL_RESOURCE_UART_PORT, port, "pal_uart_host");
            pal_spinlock_unlock(&s_uart_lock);
            return st;
        }
    }

    if (rx_pin >= 0) {
        st = pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)rx_pin, "pal_uart_host");
        if (st != WINK_OK) {
            if (tx_pin >= 0) {
                pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)tx_pin, "pal_uart_host");
            }
            pal_resource_release(PAL_RESOURCE_UART_PORT, port, "pal_uart_host");
            pal_spinlock_unlock(&s_uart_lock);
            return st;
        }
    }

    p->in_use = true;
    p->tx_pin = tx_pin;
    p->rx_pin = rx_pin;
    p->baud_rate = baud_rate;
    p->event_cb = NULL;
    p->event_cb_arg = NULL;
    p->rx_head = 0;
    p->rx_tail = 0;
    p->last_tx_len = 0;
    p->forced_err = WINK_OK;

    pal_spinlock_unlock(&s_uart_lock);
    return WINK_OK;
}

void pal_uart_deinit(uint8_t port) {
    if (port >= PAL_UART_PORT_MAX) {
        return;
    }

    pal_spinlock_lock(&s_uart_lock);
    host_uart_port_t *p = &s_ports[port];
    if (!p->in_use) {
        pal_spinlock_unlock(&s_uart_lock);
        return;
    }

    if (p->rx_pin >= 0) {
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)p->rx_pin, "pal_uart_host");
    }
    if (p->tx_pin >= 0) {
        pal_resource_release(PAL_RESOURCE_GPIO_PIN, (uint32_t)p->tx_pin, "pal_uart_host");
    }
    pal_resource_release(PAL_RESOURCE_UART_PORT, port, "pal_uart_host");

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
    host_uart_port_t *p = &s_ports[port];
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
    if (port >= PAL_UART_PORT_MAX || buf == NULL || out_read == NULL) {
        return WINK_ERR_INVALID_ARG;
    }

    pal_spinlock_lock(&s_uart_lock);
    host_uart_port_t *p = &s_ports[port];
    if (!p->in_use) {
        pal_spinlock_unlock(&s_uart_lock);
        return WINK_ERR_INVALID_STATE;
    }

    if (p->forced_err != WINK_OK) {
        wink_status_t err = p->forced_err;
        p->forced_err = WINK_OK;
        pal_spinlock_unlock(&s_uart_lock);
        return err;
    }

    uint32_t count = 0;
    while (count < len && p->rx_head != p->rx_tail) {
        buf[count++] = p->rx_buf[p->rx_head];
        p->rx_head = (p->rx_head + 1) % HOST_UART_RX_BUF_SIZE;
    }

    *out_read = count;
    pal_spinlock_unlock(&s_uart_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len) {
    if (port >= PAL_UART_PORT_MAX || (buf == NULL && len > 0)) {
        return WINK_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return WINK_OK;
    }

    pal_spinlock_lock(&s_uart_lock);
    host_uart_port_t *p = &s_ports[port];
    if (!p->in_use) {
        pal_spinlock_unlock(&s_uart_lock);
        return WINK_ERR_INVALID_STATE;
    }

    if (p->forced_err != WINK_OK) {
        wink_status_t err = p->forced_err;
        p->forced_err = WINK_OK;
        pal_spinlock_unlock(&s_uart_lock);
        return err;
    }

    size_t copy_len = (len > HOST_UART_TX_BUF_SIZE) ? HOST_UART_TX_BUF_SIZE : len;
    memcpy(p->last_tx, buf, copy_len);
    p->last_tx_len = copy_len;

    pal_spinlock_unlock(&s_uart_lock);
    return WINK_OK;
}

WINK_WARN_UNUSED_RESULT
wink_status_t pal_uart_write_async(uint8_t port, const uint8_t *buf, size_t len) {
    wink_status_t st = pal_uart_write(port, buf, (uint32_t)len);
    if (st == WINK_OK) {
        pal_spinlock_lock(&s_uart_lock);
        host_uart_port_t *p = &s_ports[port];
        pal_uart_event_callback_t cb = p->event_cb;
        void *arg = p->event_cb_arg;
        pal_spinlock_unlock(&s_uart_lock);
        if (cb != NULL) {
            cb(port, PAL_UART_EVENT_TX_DONE, NULL, 0, arg);
        }
    }
    return st;
}
