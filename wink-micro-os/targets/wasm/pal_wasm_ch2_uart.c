// SPDX-License-Identifier: Apache-2.0
/**
 * @file pal_wasm_ch2_uart.c
 * @brief Wasm target Axis A (CH2u) UART RX SPSC asynchronous ring buffer & lifecycle implementation.
 */

#include <emscripten/emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "hal/pal_uart.h"
#include "osal/pal_osal.h"
#include "pal_irq.h"
#include "pal_wasm_common.h"
#include "wasm_bridge.h"

#define WASM_UART_MAX_PORTS 2
#define WASM_UART_RX_FIFO_SIZE 256

/* Software IRQ numbers raised when a byte is pushed into the RX FIFO. The
 * firmware/UART DAL registers a handler via pal_irq_enable() to receive it. */
#define WASM_UART_RX_IRQ_BASE 20u

static pal_os_ringbuf_handle_t s_uart_rx_fifo[WASM_UART_MAX_PORTS] = {NULL, NULL};
static bool s_uart_inited[WASM_UART_MAX_PORTS] = {false, false};

static void ensure_port_fifo_created(uint8_t port)
{
    if (port >= WASM_UART_MAX_PORTS) {
        return;
    }
    if (s_uart_rx_fifo[port] == NULL) {
        s_uart_rx_fifo[port] = pal_os_ringbuf_create(WASM_UART_RX_FIFO_SIZE);
        if (s_uart_rx_fifo[port] == NULL) {
            pal_wasm_report_oom("uart_rx_fifo",
                                (uint32_t)WASM_UART_RX_FIFO_SIZE);
        }
    }
}

EMSCRIPTEN_KEEPALIVE
bool pal_wasm_push_uart_rx_byte(uint8_t port, uint8_t byte)
{
    WASM_FAULT_GUARD(false);

    if (port >= WASM_UART_MAX_PORTS) {
        return false;
    }

    ensure_port_fifo_created(port);

    if (s_uart_rx_fifo[port] == NULL) {
        return false;
    }

    wink_status_t st = pal_os_ringbuf_push(s_uart_rx_fifo[port], &byte, sizeof(byte));
    if (st != WINK_OK) {
        /* Overrun: drop newest byte and log fault (G6 overrun policy) */
        pal_wasm_log_fault(FAULT_TYPE_UART_OVERRUN, port);
        return false;
    }

    /* Raise the UART RX software IRQ; cooperative single-core, no race. */
    pal_irq_set_pending(WASM_UART_RX_IRQ_BASE + port);
    pal_wasm_dispatch_pending_irqs();
    return true;
}

EMSCRIPTEN_KEEPALIVE
void pal_wasm_push_uart_rx_error(uint8_t port, uint8_t error_flags)
{
    WASM_FAULT_GUARD_VOID();

    if (port >= WASM_UART_MAX_PORTS) {
        return;
    }

    /* flags: 1=FRAMING, 2=PARITY, 4=OVERRUN */
    if (error_flags & 4) {
        pal_wasm_log_fault(FAULT_TYPE_UART_OVERRUN, port);
    }
}

EMSCRIPTEN_KEEPALIVE
uint32_t pal_wasm_get_uart_rx_available(uint8_t port)
{
    if (port >= WASM_UART_MAX_PORTS || s_uart_rx_fifo[port] == NULL) {
        return 0;
    }
    return pal_os_ringbuf_used(s_uart_rx_fifo[port]);
}

wink_status_t pal_uart_init(uint8_t port, uint8_t tx_pin, uint8_t rx_pin, uint32_t baud_rate)
{
    (void)tx_pin;
    (void)rx_pin;
    (void)baud_rate;

    if (port >= WASM_UART_MAX_PORTS) {
        return WINK_ERR_INVALID_ARG;
    }

    ensure_port_fifo_created(port);
    if (s_uart_rx_fifo[port] == NULL) {
        return WINK_ERR_NO_MEM;
    }

    s_uart_inited[port] = true;
    return WINK_OK;
}

void pal_uart_deinit(uint8_t port)
{
    if (port >= WASM_UART_MAX_PORTS) {
        return;
    }
    if (s_uart_rx_fifo[port] != NULL) {
        pal_os_ringbuf_destroy(s_uart_rx_fifo[port]);
        s_uart_rx_fifo[port] = NULL;
    }
    s_uart_inited[port] = false;
}

wink_status_t pal_uart_read(uint8_t port, uint8_t *buf, uint32_t len, uint32_t *out_read)
{
    if (port >= WASM_UART_MAX_PORTS || !buf || !out_read) {
        return WINK_ERR_INVALID_ARG;
    }

    *out_read = 0;
    ensure_port_fifo_created(port);

    if (s_uart_rx_fifo[port] == NULL) {
        return WINK_ERR_NO_MEM;
    }

    uint32_t available = pal_os_ringbuf_used(s_uart_rx_fifo[port]);
    uint32_t to_read = (len < available) ? len : available;

    for (uint32_t i = 0; i < to_read; i++) {
        uint8_t b = 0;
        if (pal_os_ringbuf_pop(s_uart_rx_fifo[port], &b, sizeof(b)) == WINK_OK) {
            buf[i] = b;
            (*out_read)++;
        } else {
            break;
        }
    }

    return WINK_OK;
}

wink_status_t pal_uart_write(uint8_t port, const uint8_t *buf, uint32_t len)
{
    if (port >= WASM_UART_MAX_PORTS || (!buf && len > 0)) {
        return WINK_ERR_INVALID_ARG;
    }

    if (len > 0) {
        js_pal_uart_write(port, buf, len);
    }
    return WINK_OK;
}

void pal_wasm_ch2_uart_reset(void)
{
    for (uint8_t port = 0; port < WASM_UART_MAX_PORTS; port++) {
        if (s_uart_rx_fifo[port] != NULL) {
            pal_os_ringbuf_destroy(s_uart_rx_fifo[port]);
            s_uart_rx_fifo[port] = NULL;
        }
        s_uart_inited[port] = false;
    }
}
